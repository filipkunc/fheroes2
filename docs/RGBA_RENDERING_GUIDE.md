# RGBA Rendering — Battle System Architecture

The battle rendering system uses a 32-bit RGBA pipeline. All battlefield content renders to
`_mainSurfaceRGBA` at physical screen resolution, composited as an SDL overlay on top of the
indexed Display.

## Rendering Pipeline

```
Indexed Sprites (AGG::GetICN)
        │
        ▼
Wrapper Functions (_blitOnSurface, _copyOnSurface, etc.)
        │  indexed→RGBA conversion via BlitIndexedToRGBAScaled
        ▼
_mainSurfaceRGBA (physical resolution, 32-bit RGBA)
        │
        ▼
SDL Overlay Texture (SDL_BLENDMODE_BLEND)
        │  composited on top of indexed Display
        ▼
SDL_RenderPresent → screen
```

The indexed Display surface is used only for UI elements outside the battlefield (status bar,
buttons) and as a conduit for dialog forwarding.

---

## Key Surfaces

| Surface | Type | Resolution | Purpose |
|---------|------|-----------|---------|
| `_mainSurfaceRGBA` | `RGBAImage` | Physical (e.g. 1280x720) | Primary battle render target |
| `_battleGroundRGBA` | `RGBAImage` | Physical | Static background cache (terrain, grid, obstacles) |
| Display | `Image` (indexed) | Game (e.g. 640x480) | UI elements, dialog forwarding |

`_mainSurface` (indexed) and `_battleGround` (indexed) have been removed.

---

## Battle Frame Lifecycle

Each frame follows this sequence:

1. **RedrawCover()** — `_copyFullSurface()` resets `_mainSurfaceRGBA` from `_battleGroundRGBA` (fast memcpy), then draws dynamic hexagon highlights
2. **RedrawArmies()** — draws all troops, missiles, effects via wrapper functions
3. **redrawPreRender()** — fills Display battle area with index 0, registers RGBA overlay, draws UI
4. **display.render()** — SDL composites overlay on top of Display; dialog forwarding runs

---

## Wrapper Functions (battle_interface.cpp)

All indexed sprite drawing goes through these wrappers which convert to RGBA:

| Wrapper | Indexed→RGBA Function |
|---------|----------------------|
| `_blitOnSurface()` | `BlitIndexedToRGBAScaled()` |
| `_alphaBlitOnSurface()` | `BlitIndexedToRGBAScaledAlpha()` |
| `_alphaBlitOnSurface()` (region) | `BlitIndexedToRGBAScaledRegion()` |
| `_copyOnSurface()` | `BlitIndexedToRGBAScaledRegion()` |
| `_copyFullSurface()` | `CopyRGBA()` (from pre-converted background) |

---

## Format-Aware Dispatch (image.cpp)

The core `Blit()`, `Copy()`, `AlphaBlit()`, and `Fill()` functions detect when the output
`Image` has `RGBA_32BIT` format and automatically convert indexed input pixels to RGBA via
palette lookup. This enables indexed sprites to render directly to RGBA Images:

```cpp
// Indexed sprite → RGBA output: auto-converts via palette
fheroes2::Image rgbaTarget( width, height, fheroes2::ImageFormat::RGBA_32BIT );
fheroes2::Blit( indexedSprite, rgbaTarget, x, y );  // auto-dispatch
```

Internal functions: `BlitIndexedToRGBAOutput()`, `CopyIndexedToRGBAOutput()`.

---

## Background Building (_redrawBattleGround)

The battlefield background is built directly in RGBA:

1. Create a game-resolution `Image` with `RGBA_32BIT` format
2. Blit indexed sprites (terrain, borders, obstacles, castle, moat, grid, ground objects) onto it — the format-aware dispatch auto-converts each sprite to RGBA
3. Scale the result to physical resolution using nearest-neighbor into `_battleGroundRGBA`

---

## Dialog Forwarding

Dialogs (settings, unit info, hero dialog, battle summary) render to the indexed Display
using the existing UI system. A forwarding mechanism in `Display::render()` detects dialog
content and converts it to RGBA:

1. `redrawPreRender()` fills Display's battle area with palette index 0 (black)
2. Dialogs draw non-zero pixels on Display
3. During `Display::render()`, any non-zero pixel in the battle area gets converted indexed→RGBA and written to `_mainSurfaceRGBA`
4. The overlay then shows both the battle scene and the dialog

The forwarding is suspended during fades (`fullRedraw`, `FadeArena`) to prevent alpha-blended
display pixels from corrupting the RGBA surface.

**Setup:** `Image::setDialogForwarding( &_mainSurfaceRGBA, offsetX, offsetY, scale )`
**Teardown:** `Image::clearDialogForwarding()` (in destructor and during fades)

---

## Spell Effects (RGBA-native)

All spell effects operate directly on `_mainSurfaceRGBA`:

| Spell | Function | How it works |
|-------|----------|-------------|
| Death Wave | `CreateDeathWaveEffectRGBA()` | Captures RGBA, geometric pixel distortion, writes back |
| Holy Shout | `CreateHolyShoutEffectRGBA()` | Captures RGBA, cross-blur in RGB space, writes back |
| Armageddon | `WhitenRGBA()` + `CopyRGBA()` | Progressive whitening, then red-tinted shaking |
| Earthquake | `CopyRGBA()` | Captures RGBA, random offset shake |
| Lightning Bolt | `RedrawLightningRGBA()` + `DimRGBA()` | Draws lightning lines, dims scene |
| Missile beam | `DrawLineRGBA()` | Anti-aliased beam lines at physical resolution |

RGBA effect functions are in `src/fheroes2/gui/ui_tool.cpp`.

---

## High-Res Custom Sprites (Thor)

Thor uses pre-rendered PNG sprites at original resolution:

1. PNGs loaded via `LoadRGBA()` into `_rgbaThorFrames[]`
2. Drawn directly to `_mainSurfaceRGBA` via `BlitRGBAScaled()`
3. Bypasses the indexed→RGBA conversion (already RGBA)
4. Falls back to indexed sprite conversion if PNGs not found

---

## TurnOrder Rendering

The TurnOrder bar draws to indexed Display (using standard `Blit`, `Copy`, `Text::draw`),
then `_syncIndexedRegionToRGBA()` converts the affected region to RGBA. This is acceptable
because the TurnOrder is a small UI element.

---

## Key Files

| File | What |
|------|------|
| `src/fheroes2/battle/battle_interface.h` | Surface declarations, wrapper signatures |
| `src/fheroes2/battle/battle_interface.cpp` | All battle rendering, spell effects, dialog handling |
| `src/engine/image.h` | `Image` (indexed/RGBA), `RGBAImage`, dialog forwarding statics |
| `src/engine/image.cpp` | Format-aware `Blit`/`Copy`/`Fill`, `BlitIndexedToRGBAOutput` |
| `src/engine/screen.cpp` | `Display::render()`, dialog forwarding loop, SDL overlay compositing |
| `src/fheroes2/gui/ui_tool.h` | RGBA spell effect function declarations |
| `src/fheroes2/gui/ui_tool.cpp` | `CreateDeathWaveEffectRGBA`, `CreateHolyShoutEffectRGBA`, `WhitenRGBA` |

---

## Future Work

- **Dialogs to RGBA directly**: The format-aware dispatch in `Blit`/`Copy`/`Fill` enables
  dialog functions (which accept `Image &`) to render to an `Image` with `RGBA_32BIT` format.
  This would eliminate the dialog forwarding mechanism entirely.
- **Remove `_syncIndexedRegionToRGBA`**: Once TurnOrder renders to RGBA directly.
- **High-res sprites for all custom monsters**: Same pattern as Thor — load PNGs, draw via `BlitRGBAScaled`.
