# Screen-level RGBA composition — final state

## Why

Custom monster portraits (Thor, Succubus, Dachshund, …) are hi-res PNGs that
can't survive the engine's 256-colour palette path. They used to be drawn
via `Display::addRGBAOverlay`, an SDL-texture overlay painted on top of
everything — which produced a long tail of Z-order bugs: overlays bleeding
through modals, overlays getting wiped by ad-hoc `ClearAllCustomMonsterRGBAOverlays`
calls, multi-slot portraits stomping each other, portraits lingering after
dismissal, casualty dialog masking, etc.

The system now works like `Battle::Interface` already did for its main
surface, but extended to every dialog and to the adventure map:

1. **Each scope owns one RGBA surface** sized to its viewport, allocated at
   **physical display resolution** (not game resolution — otherwise hi-res
   PNGs downscale twice).
2. **A forwarding frame is pushed onto a stack** so palette writes inside
   the scope convert indexed → RGBA into that surface at render time.
3. **Hi-res portraits direct-paint** into the active surface via
   `registerRGBABufferPaint`, applied **after** the forwarding pass so they
   survive palette repaints.
4. **One `addRGBAOverlay(rgba, …)`** at the root composites the surface
   onto the screen at physical resolution.
5. **Modals and battle stack their own surfaces on top** via the forwarding
   stack; when popped, their surface teardown cleans up everything it
   registered, parent resumes.

## How it works

### Forwarding stack — `src/engine/image.{h,cpp}`

- `pushDialogForwarding(target, offsetX, offsetY, scale)`: push a frame.
  `scale` is the physical-pixel-per-game-pixel ratio; `target` is an
  `RGBAImage` sized `(gameW × scale, gameH × scale)`.
- `popDialogForwarding()`: pop the top frame.
- `getActiveDialogForwarding()`: read the top frame.
- `getDialogFwdDepth()`: stack size; used to tag overlays at registration.
- `suspendDialogForwarding()` / `resumeDialogForwarding()`: gate the
  forwarding pass **without touching the stack**. Used by battle's fade
  transitions so the adventure-map parent frame stays registered underneath
  battle's frame while palette pixels are alpha-blended.
- `ScopedDialogForwarding` RAII helper: push on construct, pop on destruct.

The legacy `setDialogForwarding` / `clearDialogForwarding` shims are still
present for historical compatibility but no longer have active callers —
everything uses push/pop.

### Paint pool — `src/engine/screen.{h,cpp}`

- `registerRGBABufferPaint(src, dst, gameX, gameY, dstX, dstY, dstW, dstH, flip, alpha)`:
  persistent post-forwarding paint. Identity is `(src, dst, gameX, gameY)`
  so re-registration dedupes.
- `removeRGBABufferPaintAt(src, gameX, gameY)`: remove by widget identity.
- `removeRGBABufferPaintsForTarget(dst)`: remove everything targeting a
  surface — used by each host's RAII on teardown.
- `removeRGBABufferPaintsInRect(dst, gameX, gameY, gameW, gameH)`: remove
  paints whose game-space coord falls in the rect — used by widgets at
  redraw time to wipe stale paints from the previous frame's troop list.
- The render loop drains `_rgbaBufferPaints` **after** the palette→RGBA
  forwarding loop so direct-paints aren't clobbered by the palette mirror.

### Overlay render — `_renderRGBAOverlays` in `src/engine/screen.cpp`

- Overlays are tagged at registration with `depth` (stack size) and
  `shadowsParent` (bool).
- Find the deepest overlay with `shadowsParent=true` (if any). Skip every
  overlay with shallower depth.
- Default is `shadowsParent=false` — dialogs compose naturally on top of
  their parent, so the parent's content remains visible outside the dialog
  rect.
- Only `Battle::Interface` sets `shadowsParent=true` (combat is a
  fullscreen takeover; the shallower `screenRGBA` would otherwise obscure
  palette UI battle draws outside `_mainSurfaceRGBA`'s footprint).

### Helper — `src/fheroes2/agg/agg_image.{h,cpp}`

`renderHiResMonsterPortrait(portrait, gameX, gameY, gameWidth, flip, alpha)`:
the one entry point every caller uses. It reads the active forwarding
frame, computes `(dstX, dstY, dstW, dstH)` in the target surface's
coordinate system (scaled by the frame's physical scale), and registers a
buffer paint. **No fallback** — if no frame is active the call is a silent
no-op.

### Physical-resolution RGBA — `Display::getPhysicalScale()`

Every dialog host asks `display.getPhysicalScale()` to size its RGBA.
Buffer paints then land in the surface at physical-pixel dimensions, so
`BlitRGBAScaled` downscales hi-res source (e.g. ~460 px) directly to the
physical target (~300 px on a 3× display) instead of hitting an
intermediate game-space downscale (~100 px) followed by an SDL upscale.

## Who owns a forwarding frame

| Scope | File | RGBA | Guard |
|---|---|---|---|
| Adventure map session | `src/fheroes2/game/game_startgame.cpp` | `screenRGBA` (full screen) | `AdventureMapForwardingGuard` |
| Castle dialog | `src/fheroes2/castle/castle_dialog.cpp` | `dialogRGBA` | `CastleDialogForwardingGuard` |
| Kingdom Overview | `src/fheroes2/kingdom/kingdom_overview.cpp` | `kingdomRGBA` | `KingdomOverviewForwardingGuard` |
| Hero dialog | `src/fheroes2/heroes/heroes_dialog.cpp` | `dialogRGBA` | `HeroDialogForwardingGuard` |
| Hero Meeting | `src/fheroes2/heroes/heroes_meeting.cpp` | `meetingRGBA` | `MeetingForwardingGuard` |
| Army Info | `src/fheroes2/dialog/dialog_armyinfo.cpp` | `dialogRGBA` | `DialogSurfaceGuard` |
| Select Count | `src/fheroes2/dialog/dialog_selectcount.cpp` | `dialogRGBA` | `DialogSurfaceGuard` |
| Recruit Monster | `src/fheroes2/dialog/dialog_recruit.cpp` | `dialogRGBA` | `DialogSurfaceGuard` |
| Select Monster | `src/fheroes2/dialog/dialog_selectitems.cpp` | `dialogRGBA` | `DialogSurfaceGuard` |
| Battle::Only setup | `src/fheroes2/battle/battle_only.cpp` | `setupRGBA` | `BattleOnlyForwardingGuard` |
| Battle | `src/fheroes2/battle/battle_interface.cpp` | `_mainSurfaceRGBA` (`shadowsParent=true`) | — (ctor push, dtor pop) |
| Battle summary | `src/fheroes2/battle/battle_dialogs.cpp` | `summaryRGBA` | `SummaryForwardingGuard` |
| Editor castle details | `src/fheroes2/editor/editor_castle_details_window.cpp` | `dialogRGBA` | `CastleDetailsForwardingGuard` |

Every guard's destructor does: `popDialogForwarding` +
`removeRGBABufferPaintsForTarget(rgba)` + `removeRGBAOverlay(rgba)`.

## Who is a portrait caller

All use `renderHiResMonsterPortrait`:

- `ArmyBar::RedrawItem` (both mini- and full-sprite branches)
- `drawMiniMonsters` (compact status-panel path + non-compact dialog path)
- `Battle::TurnOrder::addCustomMonsterOverlays`
- `Dialog::ArmyInfo::DrawMonster` (animated-frame cycle)
- `MonsterDialogElement::draw` (used by Select Count / Recruit / …)
- `SelectEnumMonster::RedrawItem`

## Per-widget stale-paint cleanup

Each widget that redraws with changing content wipes stale paints before
re-registering, scoped to its own rect on the active forwarding target:

- `ArmyBar::Redraw` → `GetArea()` (bar bounding box)
- `drawMiniMonsters` → the row rect it's about to render into
- `SelectEnumMonster::Redraw` → `rtAreaItems` (scroll area)
- `Dialog::ArmyInfo::DrawMonster` → the full preview `roi` (animation
  frames have varying sprite sizes; rect-scoped clear handles them)
- `Battle::redrawPreRender` → `removeRGBABufferPaintsForTarget(&_mainSurfaceRGBA)`
  before re-registering turn-order paints (positions shift between frames).

## Key gotchas

- **Always use an RAII guard.** A manual push/pop with any early return
  between them is a crash risk: the next `Display::render()` dereferences
  a dangling `RGBAImage*` on the stack.
- **Forwarding loop only overwrites RGBA where palette > 0.** Index-0
  pixels don't forward; the RGBA surface retains whatever was there. For
  most widgets this is invisible because the palette is opaque everywhere
  they care about, but if a widget clears its slot to index 0 and relies
  on the RGBA to show through, it needs to re-register or explicitly clear.
- **Dialog host install goes after early-return branches.** Castle dialog
  chains to a sibling `OpenDialog` on construction / mage-guild early
  returns — those must not leave a forwarding frame on the stack. Install
  the guard AFTER those branches so it only covers the main event loop.
- **Battle sub-dialogs need their own frame.** If a dialog opens during
  battle and does not push its own forwarding frame, buffer paints
  targeting `_mainSurfaceRGBA` (turn-order portraits, etc.) will re-apply
  on top of the dialog content each render. `DialogBattleSummary` pushes
  its own `summaryRGBA` for this reason; any new battle sub-dialog that
  shows UI over `_mainSurfaceRGBA` must do the same.
- **Each animation frame of a sprite is a different RGBAImage src pointer.**
  `(src, dst, gameX, gameY)` dedupe does not catch them. Use
  `removeRGBABufferPaintsInRect` at the start of each redraw over the
  animation rect, not per-frame registration.

## Open / deferred

- **`BlitRGBAScaled` is still nearest-neighbour.** Since the physical-scale
  fix the downscale ratio is small enough (~1.5×) that this isn't
  obviously bad. A box-average version was tried and made pixel-perfect
  PNG edges muddy; reverted. If quality ever matters more than perf,
  options are bilinear, Lanczos, or pre-cached downscaled portraits per
  canonical size.
- **`quickinfo` popups** still don't push their own forwarding frame.
  They work today because the non-compact `drawMiniMonsters` branch
  direct-paints into whatever parent frame is active (adventure map
  `screenRGBA`), and a rect-scoped clear at the start of `drawMiniMonsters`
  handles stale paints. The inconsistency with other modals is cosmetic —
  if you touch `dialog_quickinfo.cpp` later, consider giving it a
  `QuickInfoForwardingGuard` for symmetry.
- **Legacy `setDialogForwarding` / `clearDialogForwarding`** have no active
  callers post-migration. Safe to delete from `image.h` / `image.cpp` if
  anyone wants the cleanup.
- **Validate** in play: hero-on-tile status-panel portrait bug
  ("missing after certain transitions") — believed fixed, but the whole
  transition chain would be worth a targeted test.

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
- `build/Debug/` has symlinks to `ANIM` / `DATA` / `MAPS` / `MUSIC` / `files` so the debug exe finds game data.

When the debug exe crashes, the debugger breaks on the fault. Copy the top
~10 frames of the Call Stack panel + locals on the top frame — usually
enough to localize.

## Testing recipes

For each screen containing a custom-monster army (Thor / Succubus /
Dachshund):

1. Open the screen.
2. Open every sub-modal reachable from it:
   - Right-click slot → ArmyInfo (animates, check no smearing)
   - Left-click slot → SelectCount (portrait crisp, parent visible around dialog)
   - Add / select monster → Select Monster list (scrolling doesn't leave ghosts)
   - Recruit flow if applicable
3. Dismiss monsters in various orders, including all slots.
4. Drag / swap between slots in one bar and between two bars on screens
   that host multiple (hero meeting, castle dialog).
5. Split stacks. Retain split.
6. Close and re-open the screen.
7. Adventure map: scroll, move hero, enter combat, close summary dialog,
   return to map. Status panel portraits must re-populate for the current
   hero.
8. Battle: open turn order with custom monsters, open battle settings /
   spell book / surrender / retreat, close, resume combat.

Watch for:
- Portraits disappearing (missing register — usually a widget that needs a
  `removeRGBABufferPaintsInRect` + re-register pattern).
- Portraits lingering where the unit no longer is (missing remove, or the
  widget's rect is too narrow for the clear).
- Portraits bleeding through a modal (modal didn't push its own frame, so
  parent's buffer paints re-apply).
- Adventure-map status-panel ghosts on focus change.
- Crashes on dialog exit paths — reach for the debug build; unbalanced
  push/pop is the usual cause.
