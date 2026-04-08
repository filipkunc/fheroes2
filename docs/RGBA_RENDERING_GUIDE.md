# RGBA Rendering — Where to Look

Guide for adding 32-bit RGBA sprite rendering alongside the existing 8-bit indexed pipeline.

## The Rendering Pipeline (current)

```
Sprite Load (AGG::GetICN)  →  indexed uint8_t + transform layer
        ↓
Battle Draw (AlphaBlit)    →  palette lookups to blend colors
        ↓
Display Buffer             →  8-bit indexed (_mainSurface)
        ↓
copyImageToSurface()       →  palette[index] → RGBA conversion
        ↓
SDL_UpdateTexture          →  upload RGBA to GPU
        ↓
SDL_RenderPresent          →  show on screen
```

For RGBA, the goal is to skip all palette lookups and work with RGBA data directly.

---

## Key Files and Locations

### 1. Image/Sprite Storage — `src/engine/image.h`

**Lines 36-126:** `Image` class
- Stores `std::unique_ptr<uint8_t[]> _data` — two layers: image (palette indices) + transform (effects)
- `image()` returns pointer to image layer, `transform()` returns pointer to transform layer
- `width()`, `height()`, `singleLayer()`, `resize()`, `reset()`

**Lines 128-181:** `Sprite` class extends Image
- Adds `_x`, `_y` position offsets
- `setPosition()`, `x()`, `y()`

**What to add:** A parallel `RGBAImage` class (or template) that stores `uint32_t` per pixel. Could also add an optional `_rgbaData` member to existing Image to keep API compatibility.

---

### 2. PNG Loading — `src/engine/image_tool.cpp`

**Lines 170-247:** `fheroes2::Load()`
- Line 182: `IMG_Load()` loads PNG as SDL surface
- Line 196: Converts to `SDL_PIXELFORMAT_BGRA32`
- Lines 223-243: **The quantization loop** — reads BGRA pixels, calls `GetColorId()` to map RGB→palette index
  - Line 226-228: alpha == 0 → transparent (transform=1)
  - Line 230-232: semi-transparent black → shadow (transform=2)
  - Line 235, 240: `GetColorId(R, G, B)` → palette index ← **this is where color is lost**

**For RGBA:** Add a `LoadRGBA()` function that skips `GetColorId()` and stores the raw BGRA pixel data directly. The SDL surface is already BGRA32 at line 201 — just memcpy instead of converting.

---

### 3. Core Blitting — `src/engine/image.cpp`

**Lines 918-1047:** `AlphaBlit()` with flip support (used for battle sprites)
- Lines 940: Gets palette pointer for color lookups
- Lines 957-958: `currentPalette[inPixel * 3]` — palette lookup to get RGB from index
- Line 963: `GetPALColorId()` — converts blended RGB back to palette index
- Lines 968-994: Double-layer path with transform handling

**Lines 1177-1301:** `Blit()` with transform layer
- Line 1211: Shadow via `transformTable[transform * 256 + outPixel]`
- Line 1194: Direct copy when transform == 0

**For RGBA:** Add `AlphaBlit_RGBA()` that does direct channel math:
```cpp
outR = (inR * alpha + outR * (255 - alpha)) / 255;
// Same for G, B, A
```
No palette lookups, no `GetColorId()`, no `GetPALColorId()`.

---

### 4. Battle Sprite Drawing — `src/fheroes2/battle/battle_interface.cpp`

**Line 2003:** Loads monster sprite
```cpp
const fheroes2::Sprite & monsterSprite = fheroes2::AGG::GetICN( unit.GetMonsterSprite(), unit.GetFrame() );
```

**Line 2087:** Draws monster sprite — **THE critical call**
```cpp
fheroes2::AlphaBlit( troopSprite, _mainSurface, offset.x, offset.y, unit.GetCustomAlpha(), unit.isReflect() );
```

**Line 2023:** Draws contour/selection highlight
```cpp
fheroes2::Blit( monsterContour, _mainSurface, drawnPosition.x, drawnPosition.y, unit.isReflect() );
```

**For RGBA:** Check if sprite has RGBA data, call `AlphaBlit_RGBA()` instead. The `_mainSurface` needs to support receiving RGBA pixels (either by being RGBA itself, or via a separate overlay).

---

### 5. Display and SDL Surface — `src/engine/screen.cpp`

**Lines 1217-1311:** Surface creation
- Line 1258: `SDL_CreateRGBSurface( 0, width, height, isPaletteModeSupported ? 8 : 32, ... )`
- Creates either 8-bit or 32-bit surface depending on renderer

**Lines 276-345:** `copyImageToSurface()` — **palette → RGBA conversion**
- Line 299: `const uint32_t * transform = _palette32Bit.data()`
- Line 302: `*out = *(transform + *in)` — indexed → RGBA lookup for each pixel
- This runs every frame for the entire display

**Lines 1121-1164:** `render()` — frame submission
- Line 1129: `copyImageToSurface()` — converts indexed buffer to RGBA
- Line 1133: `SDL_UpdateTexture()` — uploads to GPU
- Line 1157: `SDL_RenderCopy()` — renders texture
- Line 1163: `SDL_RenderPresent()` — presents to window

**For RGBA:** If the display buffer is already RGBA, skip `copyImageToSurface()` entirely. Direct upload.

---

### 6. Palette System — `src/engine/screen.cpp`

**Lines 1324-1336:** `updatePalette()`
- Rebuilds `_palette32Bit` lookup table (256 entries of uint32_t)
- For 8-bit surfaces: `SDL_SetPaletteColors()`

**Lines 1602-1610:** `changePalette()`
- Called when palette cycling happens (water, lava, torch animations)
- Triggers `updatePalette()` on render engine

**For RGBA:** Palette cycling doesn't affect RGBA sprites. Original assets keep palette cycling. Custom RGBA sprites are immune (their colors are baked in).

---

### 7. Palette Effects on Sprites — `src/engine/image.cpp`

**Lines 1053-1116:** `ApplyPalette()`
- Remaps palette indices in a sprite (used for Stone, Mirror Image effects)
- Called from `battle_interface.cpp` line 2012

**For RGBA:** Replace with color transform functions (desaturate for stone, tint for effects).

---

## Simplest Approach — Overlay Strategy

Instead of converting the entire pipeline, add an RGBA overlay on top of the existing 8-bit rendered frame:

```
┌──────────────────────┐
│  SDL Renderer        │
│  ┌─────────────────┐ │
│  │ RGBA overlay    │ │  ← custom sprites drawn here (32-bit)
│  │ (transparent)   │ │
│  ├─────────────────┤ │
│  │ 8-bit indexed   │ │  ← original game rendered here (palette)
│  │ (converted to   │ │
│  │  RGBA at display)│ │
│  └─────────────────┘ │
└──────────────────────┘
```

### Implementation:

1. **`screen.cpp`:** Create a second SDL texture for the overlay (`_overlayTexture`), set blend mode to `SDL_BLENDMODE_BLEND`

2. **`agg_image.cpp`:** When loading custom PNGs, store the raw RGBA data in a separate cache (not through `Load()` which quantizes)

3. **`battle_interface.cpp`:** When drawing a custom monster, blit the RGBA data to the overlay surface instead of `_mainSurface`

4. **`screen.cpp` render():** After rendering the main 8-bit texture, render the overlay texture on top:
```cpp
SDL_RenderCopy( _renderer, _texture, nullptr, nullptr );      // 8-bit base
SDL_RenderCopy( _renderer, _overlayTexture, nullptr, nullptr ); // RGBA overlay
SDL_RenderPresent( _renderer );
```

### Pros:
- Minimal changes to existing pipeline
- No risk of breaking original rendering
- Can be done incrementally

### Cons:
- Z-ordering between 8-bit and RGBA sprites needs manual handling
- Double texture upload per frame
- Custom sprites always render on top of original sprites

---

## SDL Rendering Backend

The engine uses **SDL2 hardware-accelerated rendering**, NOT software rendering or direct OpenGL/Vulkan calls.

**`screen.cpp` line 1223:** Requires `SDL_RENDERER_ACCELERATED`
**`screen.cpp` line 1272:** `SDL_CreateRenderer( _window, _driverIndex, SDL_RENDERER_ACCELERATED )`

The actual GPU backend (OpenGL, Vulkan, Direct3D, Metal) is chosen by SDL2 automatically based on platform. The engine only uses SDL2's renderer API:
- `SDL_CreateTexture` / `SDL_UpdateTexture` — texture management
- `SDL_RenderCopy` — draw texture to screen
- `SDL_RenderPresent` — present frame

This means the overlay approach is simple: create a second `SDL_Texture` with `SDL_TEXTUREACCESS_STREAMING` + `SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)`, write RGBA sprite pixels to it, and `SDL_RenderCopy` it after the main texture. SDL handles the GPU alpha compositing — no manual shader or OpenGL code needed.

For more advanced needs (palette cycling shaders, post-processing), raw OpenGL/Vulkan access would require bypassing SDL's renderer, which is a bigger change.

---

## Key Constants and Locations

| What | Where |
|------|-------|
| Image class | `src/engine/image.h:36` |
| Sprite class | `src/engine/image.h:128` |
| Load() PNG→indexed | `src/engine/image_tool.cpp:170` |
| GetColorId() quantization | `src/engine/image_tool.cpp:235` |
| AlphaBlit (battle draw) | `src/engine/image.cpp:918` |
| Blit (opaque draw) | `src/engine/image.cpp:1177` |
| Monster sprite draw call | `src/fheroes2/battle/battle_interface.cpp:2087` |
| Display surface creation | `src/engine/screen.cpp:1258` |
| Indexed→RGBA conversion | `src/engine/screen.cpp:295-302` |
| Render/present | `src/engine/screen.cpp:1121-1164` |
| Palette update | `src/engine/screen.cpp:1324-1336` |
| ApplyPalette (effects) | `src/engine/image.cpp:1053` |
| Custom PNG loading | `src/fheroes2/agg/agg_image.cpp:2452` |
