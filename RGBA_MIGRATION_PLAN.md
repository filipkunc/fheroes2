# Screen-level RGBA composition migration — plan & status

## Why

Custom monster portraits (Thor, Succubus, Dachshund, …) are hi-res PNGs that shouldn't go through the engine's 256-colour palette path. Originally each portrait was registered via `Display::addRGBAOverlay`, which is an SDL-texture-layer overlay painted on top of everything. That produced a long tail of Z-order bugs — overlays bleeding through modals, overlays getting wiped by `ClearAllCustomMonsterRGBAOverlays`, multi-slot / multi-bar portraits stomping each other, portraits missing after dismissal, etc.

The target architecture mirrors what `Battle::Interface` already does with `_mainSurfaceRGBA`:

1. **Each screen owns one RGBA surface** sized to its viewport.
2. **`setDialogForwarding` pipes palette writes** from that screen into the RGBA surface at render time (indexed → RGBA conversion).
3. **Hi-res portraits blit directly** into the surface via `BlitRGBAScaled`, bypassing palette quantization.
4. **One `addRGBAOverlay(screenRGBA, …)`** at the root draws the composited result.
5. **Modal dialogs stack their own smaller RGBA** on top via the forwarding stack. When popped, they stop writing to their surface and their overlay is removed.

Net effect: Z-ordering by construction, zero ad-hoc overlay masking, no more `ClearAllCustomMonsterRGBAOverlays` needed for custom portraits.

## Status snapshot (2026-04)

### ✅ Done — Phase 1 infrastructure

| Piece | File | Purpose |
|---|---|---|
| Stack-based forwarding | `src/engine/image.h`, `src/engine/image.cpp` | `pushDialogForwarding` / `popDialogForwarding` / `getActiveDialogForwarding`. Legacy `set`/`clear` kept as shims so `battle_interface.cpp` keeps working. `_dialogFwdTarget` etc. are kept in sync with top of stack so `screen.cpp`'s forwarding loop still reads them directly. |
| `ScopedDialogForwarding` RAII | `src/engine/image.h` | Push/pop only. |
| `renderHiResMonsterPortrait` helper | `src/fheroes2/agg/agg_image.h`, `agg_image.cpp` | Single entry point every hi-res portrait caller funnels through. If forwarding is active → `registerRGBABufferPaint` for post-forwarding direct-paint into the target surface. Otherwise → `Display::addRGBAOverlay` fallback (preserved so unmigrated screens keep working). |
| Persistent `RGBABufferPaint` pool | `src/engine/screen.h`, `src/engine/screen.cpp` | `registerRGBABufferPaint`, `removeRGBABufferPaintAt(src, gameX, gameY)`, `removeRGBABufferPaintsForTarget(dst)`. Identity is **game-space** coords (so widgets remove what they added without knowing surface math). Applied after the palette→RGBA forwarding loop in `Display::render()`. |

### ✅ Done — Phase 2 first migration

| Screen / widget | Status |
|---|---|
| Hero dialog (`src/fheroes2/heroes/heroes_dialog.cpp`) | Owns `dialogRGBA`, pushes forwarding, single overlay. Local `HeroDialogForwardingGuard` RAII handles teardown across all 4 return paths. |
| Battle Only setup (`src/fheroes2/battle/battle_only.cpp`) | Same pattern; local `BattleOnlyForwardingGuard`. |
| Modals — ArmyInfo, SelectCount, RecruitMonster, selectMonster | Each owns a sub-rect `dialogRGBA`, pushes forwarding, local `DialogSurfaceGuard` handles cleanup. |
| `ArmyBar::RedrawItem` | Both mini-sprite and full-sprite branches route custom portraits through `renderHiResMonsterPortrait`. |
| `SelectEnumMonster::RedrawItem` | Same. |

### 🔥 Known architectural constraint

The forwarding loop only overwrites RGBA where palette > 0. That means direct-paints into the surface survive *only* where the palette underneath is index 0. For `ArmyBar` slots the slot background is currently non-zero palette (STRIP sprite), so the forwarding pass overwrites the slot region with palette pixels each frame. The post-forwarding paint queue drains **after** forwarding, which is what makes the portrait re-appear on top. This works as long as the widget re-registers the paint on every `RedrawItem`.

Consequence: if a widget doesn't redraw when its slot's palette is dirtied (e.g. a partial redraw from an unrelated widget forwards the slot's palette back in), the RGBA version of the portrait survives because the paint is persistent in `_rgbaBufferPaints` and re-applied every frame. ✅

### 🐛 Important gotcha uncovered — **use RAII for push/pop**

Every manual `pushDialogForwarding` followed by a `popDialogForwarding` further down the function is a **crash risk** if there's any early return between them. Early returns that skip the pop leave a forwarding frame on the stack with a dangling `RGBAImage*` — the next `Display::render()` faults dereferencing it.

Historical example: `Dialog::ArmyInfo` had a `return Dialog::ZERO;` in the right-click-preview branch between push and pop. Dismissing armies in Battle Only reliably crashed because the right-click preview path was hit during the dismiss flow.

**Rule for every new migration:** use a RAII struct (see `DialogSurfaceGuard` pattern in the four modal files, or `HeroDialogForwardingGuard` / `BattleOnlyForwardingGuard`) that pops the forwarding frame **and** removes overlay + buffer-paint registrations targeting the local RGBA. Never manually call `pushDialogForwarding` without one.

### 🧩 Widget-side key detail

`registerRGBABufferPaint` stores **both** game-space coords (identity / dedup key) and surface-space coords (actual blit target). `removeRGBABufferPaintAt` uses game-space — same values widgets already track. This fixed an accumulation bug where removal failed silently because the widget passed game coords but the entry was keyed on surface coords.

## ⏳ Remaining work

### Phase 3 — Adventure map + status panel

#### ✅ Partial — compact `drawMiniMonsters` routed through helper

`src/fheroes2/army/army_ui_helper.cpp` compact branch (status panel portraits)
now calls `renderHiResMonsterPortrait` instead of `Display::addRGBAOverlay`
directly. Behaviourally identical today — no forwarding is active on the
adventure map, so the helper takes the fallback path — but every call is now
upgrade-ready: the moment a screen-sized RGBA is installed on `AdventureMap`,
the compact path direct-paints into it with no further changes to the caller.

Per-frame `ClearAllCustomMonsterRGBAOverlays()` on the compact branch is kept
as-is; it's a sledgehammer but is not breaking anything.

#### 🚧 Deferred — screen-wide RGBA install on `AdventureMap`

The full plan was: install a screen-sized `RGBAImage` on `AdventureMap`, push
forwarding across the whole adventure-map session, and let every palette write
forward into it.

Blocker: `Battle::Interface` is entered **from inside** the adventure-map
session (not from a caller above it). On entry, the battle ctor calls
`Display::clearRGBAOverlays()` and `Image::setDialogForwarding()` (which
*replaces* the stack top rather than pushing) — both of which would wipe any
adventure-map RGBA state we installed before the battle. On exit, the battle
dtor calls `clearDialogForwarding()` and `clearRGBAOverlays()` again. End
result: whatever we install on the adventure-map side gets wiped by every
combat.

Making this work end-to-end requires cooperation from `battle_interface.cpp`
— roughly:
- Convert the ctor/dtor `setDialogForwarding` / `clearDialogForwarding` pair
  to `pushDialogForwarding` / `popDialogForwarding` so the adventure-map
  frame underneath survives.
- Convert the two mid-battle `clearDialogForwarding` / `setDialogForwarding`
  "suspend-during-fade" pairs to something that doesn't touch the stack
  below battle's own frame (likely a `suspendForwarding()` /
  `resumeForwarding()` pair that flips `_dialogFwdTarget` to `nullptr`
  without popping).
- Replace `Display::clearRGBAOverlays()` in the battle entry/exit with
  `removeRGBAOverlay(&_mainSurfaceRGBA)` so the adventure-map overlay stays
  registered across the battle.

That's a cohesive follow-up refactor and deserves its own pass. Track as
**Phase 3b**. Notes that still apply when it's picked up:

- Adventure map scrolls — every scroll step redraws the map palette, which
  forwards into the RGBA. That's fine.
- Animations (flags, monster frame cycles) forward automatically.
- Hero-on-tile mini portraits in the status panel have been reported missing
  after certain transitions — validate the fix here.
- Non-compact `drawMiniMonsters` (quickinfo popups) currently keeps the
  indexed MONS32 blit. Migrating it to `renderHiResMonsterPortrait` needs
  `dialog_quickinfo.cpp` to push its own forwarding frame first, otherwise
  the fallback `addRGBAOverlay` registration leaks when the popup closes.

### ✅ Done — Phase 4 ArmyBar hosts

| Host | File | Guard name |
|---|---|---|
| Castle dialog (garrison + visiting hero) | `src/fheroes2/castle/castle_dialog.cpp` | `CastleDialogForwardingGuard` |
| Kingdom Overview | `src/fheroes2/kingdom/kingdom_overview.cpp` | `KingdomOverviewForwardingGuard` |
| Hero Meeting | `src/fheroes2/heroes/heroes_meeting.cpp` | `MeetingForwardingGuard` |

Each host follows the Phase 2 recipe verbatim: allocate an `RGBAImage` sized
to the dialog footprint, snapshot the current palette via
`BlitIndexedToRGBAScaledRegion`, register one root `addRGBAOverlay`, push a
forwarding frame, and declare an RAII guard that pops the frame and removes
the overlay / buffer-paint registrations before the `RGBAImage` destructs.

Castle dialog subtlety: install goes *after* the construction / mage-guild
early-return branches (which chain to a sibling `OpenDialog` for a different
castle and must not leave a forwarding frame on the stack). The RAII guard
then covers the main event loop, the mid-loop mage-guild early return
(`return mageGuildResult;`), and the normal end-of-function return.

Kingdom Overview and Hero Meeting have single exit paths; the RAII guard is
still used there so future early returns pick up the cleanup for free.

### ✅ Done — Phase 5 Editor

Audit: the only editor screen that hosts an `ArmyBar` directly is
`Editor::castleDetailsDialog` in
`src/fheroes2/editor/editor_castle_details_window.cpp`. That function now
has a `CastleDetailsForwardingGuard` installed immediately after the
`StandardWindow` construction, covering both exit paths (the Exit button's
`return false` and the normal `return true` at end of function).

Everywhere else in `src/fheroes2/editor/` that shows a monster preview goes
through a Phase 2–migrated modal (`Dialog::selectMonsterType`,
`Dialog::SelectCount`, `Dialog::RecruitMonster`, `Dialog::multiSelectMonsters`)
— those modals install and tear down their own screen-RGBA frames, so the
editor caller needs no further work.

### Phase 6 — Cleanup (only after everything above is migrated and verified)

Once the fallback path is no longer exercised:

1. Delete the fallback branch in `renderHiResMonsterPortrait` that calls `Display::addRGBAOverlay`. Every caller must have an active forwarding target by then.
2. Remove `ArmyBar::_slotOverlays` tracking — slots don't need to track overlays anymore because `_rgbaBufferPaints` keyed on `(src, gameX, gameY)` is self-managing via the RAII dtor in `removeRGBABufferPaintsForTarget`. Still need `removeRGBABufferPaintAt` in `ArmyBar::Redraw` pre-pass for the dismissal case.
3. Remove `SelectEnumMonster::_rowOverlays` tracking for the same reason.
4. Remove `Display::removeRGBAOverlayAt` (no consumers left once `ArmyBar` doesn't call it).
5. Audit `AGG::ClearAllCustomMonsterRGBAOverlays` — once no one registers custom-monster `addRGBAOverlay` entries, this becomes a no-op. Can be deleted along with its call sites in dialogs.
6. `addRGBAOverlay` / `removeRGBAOverlay` / `_rgbaOverlays` stays on `Display` — it's still used for each screen's single root overlay (the `screenRGBA`) plus battle's `_mainSurfaceRGBA`.

## Key file locations

| Area | File |
|---|---|
| Forwarding stack | `src/engine/image.h`, `src/engine/image.cpp` (`_dialogFwdStack`, push/pop/set/clear, `ScopedDialogForwarding`) |
| Paint pool + render loop | `src/engine/screen.h`, `src/engine/screen.cpp` (`_rgbaBufferPaints`, `register`/`remove…At`/`removeForTarget`, drain in `Display::render()`) |
| Helper | `src/fheroes2/agg/agg_image.h` / `agg_image.cpp` (`renderHiResMonsterPortrait`) |
| Example host with RAII guard | `src/fheroes2/heroes/heroes_dialog.cpp`, `src/fheroes2/battle/battle_only.cpp` |
| Example modal with RAII guard | `src/fheroes2/dialog/dialog_armyinfo.cpp` (`DialogSurfaceGuard`) — especially the right-click-preview early return |
| ArmyBar portrait path | `src/fheroes2/army/army_bar.cpp` |

## Build / debug quick-ref

```
# Release
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Release /p:Platform=x64 /m

# Debug (with PDB, assertions, JIT attach)
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Debug /p:Platform=x64 /m
```

VS Code:
- `.vscode/launch.json` has `Launch fheroes2 (Release)`, `Launch fheroes2 (Debug)`, and `Attach to fheroes2`.
- `.vscode/tasks.json` has `CMake Build (Release)` and `CMake Build (Debug)`.
- `build/Debug/` has symlinks to `ANIM`/`DATA`/`MAPS`/`MUSIC`/`files` so the debug exe finds game data.

When the debug exe crashes, the VS Code debugger breaks on the fault. Copy the top ~10 frames of the Call Stack panel + locals shown for the top frame — that's usually enough to localize.

## Testing recipes (copy when validating a new phase)

For each screen migrated:

1. Open the screen with a custom-monster army (Thor / Succubus / Dachshund).
2. Open every sub-modal reachable from the screen:
   - Right-click army slot → ArmyInfo
   - Left-click army slot → SelectCount
   - Add/select monster → Select Monster
   - Recruit flow, if applicable
3. Dismiss monsters in various orders. Try dismissing all.
4. Drag/swap monsters between slots (intra-bar) and between bars (inter-bar) if the screen has multiple.
5. Split stacks. Retain split.
6. Close and re-open the screen.
7. For adventure map: scroll, move hero, combat, dialog.

Watch for:
- Portraits disappearing (missing register)
- Portraits lingering where the unit no longer is (missing remove)
- Portraits bleeding through a modal (modal didn't push its own RGBA)
- Flashes/flickers on frame transitions
- Crashes — always catch these with the debug build
