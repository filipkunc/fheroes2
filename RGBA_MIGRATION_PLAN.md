# Display = single RGBA surface — final state

## Why

Custom monster portraits (Thor, Succubus, Azure Dragon, …) are hi-res PNGs that
can't survive the engine's 256-colour palette path. Earlier iterations layered
SDL overlay textures, then a per-scope RGBA buffer-paint pool with a forwarding
stack, then a per-frame palette mirror — each iteration produced its own long
tail of Z-order bugs (overlays bleeding through modals, buffer paints surviving
into the wrong scope, palette content over-painting hi-res portraits, etc.).

The current rendering pipeline is a pure painter algorithm into a single
`Display`-owned RGBA framebuffer at physical resolution. There is no SDL
overlay, no per-scope target, no buffer-paint pool, no forwarding stack,
no scope-key filtering. **Order of execution = order of pixel writes.** Every
frame ends with one `SDL_UpdateTexture` + `SDL_RenderCopy`.

## How it works

`Display` owns `_screenRGBA` — a screen-sized `RGBAImage` at physical pixel
resolution (`gameW * physicalScale × gameH * physicalScale`), allocated by
`setResolution` / `resetRenderer`.

Every drawing primitive that writes to `Image::_data` (`Blit`, `Fill`,
`AlphaBlit`, `ApplyPalette`, `ApplyAlpha`, `Copy`, `Flip`, `DrawLine`,
`DrawRect`, `Resize`, `SubpixelResize`, `SetPixel`, `Transpose`,
`ReplaceColorId{,ByTransformId}`, `addGradientShadow`,
`CreateDitheringTransition`, …) ends with a call to `out._notifyWrite(roi)`.

`Image` carries an optional `_onWrite` hook. Sprites and intermediate buffers
have a null hook (one branch per write). `Display` installs a hook in
`setResolution` that mirrors the just-written rect from indexed → `_screenRGBA`
at physical scale. Index 0 is skipped (legacy palette-zero convention; harmless
in the new model).

Hi-res RGBA paints (`renderHiResMonsterPortrait`, battle's terrain/units, spell
effects) bypass the indexed buffer entirely and write directly to
`Display::screenRGBA()` at absolute physical-pixel coords. They land *after*
the palette mirror because the widget code calls them after the palette
draw — the painter algorithm guarantees correct Z-order by construction.

`Display::render(roi)` is then trivially:

1. Optional pre-processing palette swap.
2. Software cursor — last paint, drawn directly into `_screenRGBA` via
   `BlitIndexedToRGBAScaled`.
3. Engine `renderScreenRGBA` — one `SDL_UpdateTexture(_screenTexture)` +
   `SDL_RenderCopy` letterboxed to actual output, then `SDL_RenderPresent`.

### Files

- `src/engine/image.{h,cpp}` — `Image::_onWrite` (`std::function<void(int32_t,int32_t,int32_t,int32_t)>`),
  `_setWriteHook`, `_notifyWrite`. Every primitive ends with `out._notifyWrite(roi)`.
- `src/engine/screen.{h,cpp}` — `Display::_screenRGBA`, `Display::render`,
  `mirrorIndexedRectToScreenRGBA` (the hook lambda body),
  `paintPaletteToScreenRGBA` (used only by `changePalette` for color cycling
  re-mirror — not per frame).
- `src/fheroes2/agg/agg_image.cpp` — `renderHiResMonsterPortrait` direct-blits
  the portrait into `Display::screenRGBA()`.
- `src/fheroes2/battle/battle_interface.{h,cpp}` — battle writes the scene
  directly into `Display::screenRGBA()` via `_blitOnSurface` /
  `_alphaBlitOnSurface` / `_copyOnSurface` / `_copyFullSurface` (offset by
  `_interfacePosition`). `_battleAreaWidthPx` / `_battleAreaHeightPx` cache
  the battlefield rect in physical pixels for the spell effects (DimRGBA,
  DeathWave, HolyShout, Armageddon, Earthquake, …).

### Color cycling

`Display::changePalette` calls `paintPaletteToScreenRGBA(*this)` once per
palette change to re-mirror the indexed buffer under the new palette table.
Cycling fires roughly every 150 ms (`AGG::ApplyICNCycling`), much less often
than per-frame, so the cost is amortised vs. the old per-frame full mirror.

### Fade transitions

`fadeDisplay` in `ui_tool.cpp` calls `Fill` + `ApplyAlpha` per frame; each
call hits the hook and `_screenRGBA` updates incrementally. Battle's fade-in
/ fade-out walks frames calling `ApplyAlpha` on the indexed buffer — same
behaviour. No `suspendDialogForwarding` shenanigans needed; the fade is just
palette writes that mirror naturally.

### Cursor

The software cursor is direct-blitted into `_screenRGBA` last, in
`Display::render`. The cursor area is overwritten by palette mirrors on the
next frame's redraw, so no backup/restore is needed — same model as before,
simpler implementation.

## What this deleted

The accumulated workarounds, in one block:

- The `_rgbaBufferPaints` pool and the `RGBABufferPaint` struct on `Display`,
  along with `registerRGBABufferPaint`, `removeRGBABufferPaintAt`,
  `removeRGBABufferPaintsForScope`, `removeRGBABufferPaintsForTarget`,
  `removeRGBABufferPaintsInRect`.
- `Image::DialogForwardingFrame`, the forwarding stack
  (`pushDialogForwarding` / `popDialogForwarding`,
  `getActiveDialogForwarding`, `getDialogFwdDepth`, `getDialogFwdStack`,
  `setDialogForwarding`, `clearDialogForwarding`, `suspendDialogForwarding`,
  `resumeDialogForwarding`), the cached top-of-stack
  (`_dialogFwdTarget`, `_dialogFwdOffsetX/Y`, `_dialogFwdScale`,
  `_dialogFwdSuspendDepth`), and `ScopedDialogForwarding`.
- All 13 forwarding-guard structs (`AdventureMapForwardingGuard` in
  `game_startgame.cpp`; `CastleDialogForwardingGuard` in `castle_dialog.cpp`;
  `KingdomOverviewForwardingGuard` in `kingdom_overview.cpp`;
  `MeetingForwardingGuard` in `heroes_meeting.cpp`;
  `HeroDialogForwardingGuard` in `heroes_dialog.cpp`;
  `DialogSurfaceGuard` ×4 in `dialog_armyinfo.cpp`, `dialog_recruit.cpp`,
  `dialog_selectcount.cpp`, `dialog_selectitems.cpp`;
  `BattleOnlyForwardingGuard` in `battle_only.cpp`;
  `SummaryForwardingGuard` in `battle_dialogs.cpp`;
  `CastleDetailsForwardingGuard` in `editor_castle_details_window.cpp`).
- The per-frame `paintPaletteToScreenRGBA` call from `Display::render`
  (the function itself stays for the `changePalette` re-mirror).
- Battle's `_mainSurfaceRGBA` and `_rgbaScale` members and the
  `_syncIndexedRegionToRGBA` helper — battle now writes directly to
  `Display::screenRGBA()` at offset.
- `Battle::Interface::ctor`'s `pushDialogForwarding` and
  `~Interface`'s `popDialogForwarding` / `removeRGBABufferPaintsForTarget`.
- The `Fill(display, _interfacePosition.x, …, 0)` clear in
  `redrawPreRender` (it was a transparency gate for the legacy per-frame
  palette mirror — meaningless in the new model).
- All `removeRGBABufferPaintsInRect` / `ForTarget` / `ForScope` call sites
  scattered across widgets.

## Hi-res portrait callers

All call `renderHiResMonsterPortrait`, which direct-blits to
`Display::screenRGBA()` at absolute physical-pixel coords:

- `ArmyBar::RedrawItem`
- `drawMiniMonsters` (compact + non-compact branches)
- `Battle::TurnOrder::addCustomMonsterOverlays`
- `Dialog::ArmyInfo::DrawMonster`
- `MonsterDialogElement::draw` (Select Count / Recruit / …)
- `SelectEnumMonster::RedrawItem`

## Key gotchas

- **Order of widget code matters.** Hi-res RGBA paints land on whatever was
  last written. Widget code MUST draw the palette art *first* (so the hook
  mirrors it) and call `renderHiResMonsterPortrait` *after*. All current
  callers do this.
- **`Image::_onWrite` lifetime.** The hook is set on `Display::instance()`
  only, in `setResolution`. Sprites and intermediate buffers never get a
  hook (one branch per write — no allocation cost).
- **Battle area dimensions are cached at construction.**
  `_battleAreaWidthPx` / `_battleAreaHeightPx` capture the battlefield rect
  in physical pixels. Spell effects (DimRGBA, DeathWave, HolyShout, …) use
  these and `(_interfacePosition.x * scale, _interfacePosition.y * scale)`
  to address the right sub-rect of `_screenRGBA`.

## Verification

1. **Build clean.** Debug x64 MSBuild succeeds without errors.
2. **Adventure map.** Status panel custom monsters render once, no flicker.
3. **Open every dialog with custom monsters.** Castle dialog, hero dialog,
   hero meeting, kingdom overview, recruit, select count, select monster,
   army info, editor castle details. Open every sub-modal reachable from
   each (right-click slot → Army Info; left-click slot → Select Count;
   add/select monster → Select Monster list). Drag/swap slots, split
   stacks, scroll lists, close and reopen.
4. **Battle.** Enter combat with custom monsters; turn-order portraits,
   hi-res unit sprites, fade in/out, spell effects (lightning,
   death wave, holy shout, armageddon, earthquake, cold ring) all work.
5. **Color cycling.** Gold/water/lava palette cycles update on screen.
6. **Battle ↔ adventure map round trip.** Status panel portraits
   re-populate after combat.
7. **Cursor on hi-res monsters.** Hover a custom-monster portrait —
   cursor draws on top, pixel-for-pixel.
8. **Letterbox / pillarbox.** Resize window to 16:9 with a 4:3 game;
   black bars remain solid black.
9. **Single SDL upload per frame.** Step `Display::render` in a debugger;
   confirm one `SDL_UpdateTexture` + one `SDL_RenderCopy` per frame.

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
