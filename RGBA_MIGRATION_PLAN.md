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

#### ✅ Done — Phase 3b screen-wide RGBA on `AdventureMap` + battle cooperation

The adventure-map session now owns a full-screen `RGBAImage` for its whole
lifetime. `AdventureMap::StartGame` allocates `screenRGBA` sized to the
display, snapshots the current palette into it, registers it as the root
overlay, pushes a forwarding frame onto the stack, and installs an
`AdventureMapForwardingGuard` RAII that pops the frame and removes the
overlay / buffer-paint registrations on any exit path.

Battle cooperation (new):

- `Battle::Interface` ctor/dtor use `pushDialogForwarding` /
  `popDialogForwarding` instead of the legacy `set` / `clear`, so battle
  nests on top of the adventure-map frame and restores it cleanly on exit.
- The two in-battle "suspend during fade" pairs (`fullRedraw` fade-in,
  `fadeBattlefield` dim-to-summary) use new `suspendDialogForwarding` /
  `resumeDialogForwarding` primitives that gate the forwarding loop via a
  depth counter without touching the stack. The adventure-map frame stays
  registered throughout.
- `redrawPreRender` and the battle dtor surgically `removeRGBAOverlay(
  &_mainSurfaceRGBA )` + `removeRGBABufferPaintsForTarget( &_mainSurfaceRGBA
  )` + `ClearAllCustomMonsterRGBAOverlays()` instead of the old
  `clearRGBAOverlays()` sledgehammer, preserving the adventure-map root
  overlay across the entire combat.

Net effect: entering / exiting combat from the adventure map now leaves the
screen-RGBA intact. Every adventure-map palette write forwards into
`screenRGBA`, and hi-res custom-monster portraits (status panel and anything
else routed through `renderHiResMonsterPortrait`) direct-paint into the same
surface — correctly Z-ordered with respect to modals that nest on top via
their own forwarding frames.

#### ⏳ Remaining follow-ups

- Non-compact `drawMiniMonsters` (quickinfo popups) still uses the indexed
  MONS32 blit. Migration needs `dialog_quickinfo.cpp` to push its own
  forwarding frame first, otherwise the fallback `addRGBAOverlay`
  registration would leak when the popup closes.
- Validate the hero-on-tile status-panel portrait bug ("missing after
  certain transitions") is actually fixed now.

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

### ✅ Done — Phase 6 Cleanup

Every custom-monster portrait path now goes through `renderHiResMonsterPortrait`
→ buffer paint into the active forwarding surface. `addRGBAOverlay` is only
called for the root compositing surface of each scope (adventure-map
`screenRGBA`, dialog `dialogRGBA`, battle `_mainSurfaceRGBA`, Battle::Only
`setupRGBA`). Fallback code is gone.

Concretely removed:

1. `renderHiResMonsterPortrait` fallback branch. The helper now silently
   no-ops if no forwarding frame is active — every legitimate caller runs
   inside a scope that has pushed one (AdventureMap at the root, dialogs or
   battle on top).
2. `ArmyBar::_slotOverlays` tracking + its RAII dtor logic. Per-frame stale
   paints are wiped via a single `removeRGBABufferPaintsInRect( active->target,
   GetArea() )` at the start of `ArmyBar::Redraw`, scoped to the bar's own
   rect so peer bars are not disturbed. The host screen's forwarding-guard
   handles surface-wide cleanup on dialog close.
3. `SelectEnumMonster::_rowOverlays` tracking. Replaced with the same
   `removeRGBABufferPaintsInRect` pattern scoped to `rtAreaItems`.
4. `Display::removeRGBAOverlayAt`. Zero callers once the trackers above are
   gone.
5. `AGG::ClearAllCustomMonsterRGBAOverlays` + all call sites in
   `battle_only.cpp`, `dialog_selectcount.cpp`, `army_ui_helper.cpp`, and
   battle's `redrawPreRender` / dtor.
6. `Battle::TurnOrder::addCustomMonsterOverlays` now calls
   `renderHiResMonsterPortrait` (direct-paints into `_mainSurfaceRGBA`
   instead of registering overlays). `redrawPreRender` clears any previous
   paints on `_mainSurfaceRGBA` before re-registering, so unit-position
   changes don't ghost.
7. `MonsterDialogElement::redraw` and `Dialog::ArmyInfo::DrawMonster` route
   through the helper; the latter uses `removeRGBABufferPaintsInRect` to
   clear the previous animation frame (cycling frames don't share src
   pointers so dedupe doesn't catch them).

Kept:

- `Display::addRGBAOverlay` / `removeRGBAOverlay` / `_rgbaOverlays` — still
  used for each scope's single root surface (`screenRGBA`, `dialogRGBA`,
  `_mainSurfaceRGBA`, `setupRGBA`).
- `_rgbaBufferPaints` + `registerRGBABufferPaint` / `removeRGBABufferPaintAt`
  / `removeRGBABufferPaintsForTarget` / `removeRGBABufferPaintsInRect` on
  `Display`.
- `Image::suspendDialogForwarding` / `resumeDialogForwarding` for battle's
  mid-fade transitions.
- Overlay depth-based filtering in `_renderRGBAOverlays`: only the deepest-
  depth overlays composite so a nested scope shadows its parent's root.

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
