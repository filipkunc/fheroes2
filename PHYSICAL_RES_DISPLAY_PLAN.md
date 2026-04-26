# Physical-resolution Display

## Why

Current state (commit `43bd1053a`): Display is a single RGBA-format Image at **game** resolution (e.g. 640×480). Drawing primitives convert palette → RGBA at write time. The engine uploads `display.image()` to a game-res streaming texture and SDL upscales it to the physical window via `SDL_RenderSetLogicalSize`.

Hi-res custom monster sprites (Thor, Succubus, Dachshund, Azure Dragon) currently take this round-trip:

```
source PNG (e.g. 460×460 RGBA)
    └─ BlitRGBAScaled downscales → game res (e.g. 100 game-pixel slot)
        └─ SDL bilinear upscales → physical pixels (e.g. 300 physical for a 3× display)
```

Result: visible blur on hi-res sprites — they end up roughly as sharp as a normal indexed sprite at the same game-pixel size, defeating the purpose of having the high-resolution PNGs.

The fix is to make Display itself a physical-resolution RGBA buffer, so a hi-res sprite downscales **once** (source → physical) and lands in its final pixels with no further filtering.

This was the original [PURE_RGBA_DISPLAY_PLAN.md](PURE_RGBA_DISPLAY_PLAN.md) intent — the plan said "Display::_data is RGBA at physical resolution". The shipped refactor put Display at game res because that was a smaller change (every widget call uses game coords; physical-res Display means every primitive scales). This doc plans the rest of the way.

## Constraint: no overlay layer

Two paths were considered and rejected:

- **Overlay** (separate physical-res RGBA buffer just for hi-res content, composited on top via `SDL_BLENDMODE_BLEND`). Implemented and reverted twice. Failure modes:
  - Hi-res content always renders **on top** of indexed content. Widgets that paint over a hi-res region with normal sprites still see the hi-res through them. This is the same Z-order coupling the original WriteHook bug demonstrated, just inverted.
  - If the overlay clears per-frame, hi-res content flickers (widgets only repaint on interaction, not every frame).
  - If the overlay persists, integrating it with `ImageRestorer` to clear correctly when dialogs close adds back the dual-buffer complexity the pure-RGBA refactor was meant to eliminate.

- **Engine-side draw-list** (renderHiResMonsterPortrait records `{portrait*, gameRect, frameId}` entries, engine RenderCopies them at physical pixels after uploading the framebuffer). Same flicker / persistence trade-off as the overlay.

The only path that is both flicker-free and Z-order-correct is making **Display itself physical-resolution**. One buffer, one upload, execution-order = paint-order.

## Architecture

`Display` continues to be an RGBA-format `Image`, but its backing buffer is sized at **physical** pixels (`gameW * scale`, `gameH * scale`). `Display::width()` and `Display::height()` keep reporting **game** dimensions, so widget code that reasons in game coords (centring windows, ROI math, hit testing) doesn't change.

The scaling lives inside drawing primitives. When a primitive writes to an Image whose internal buffer is larger than `(width(), height())` reports, each game pixel write expands to a physical-pixel block.

```
+--------------------------------------------+
|   Widget code: Blit(spr, display, gx, gy)  |
+--------------------------------------------+
                      │ display.width() = gameW (logical)
                      │ display.bufferStride() = gameW * scale (physical)
                      │ display.physicalScale() = scale
                      ▼
+--------------------------------------------+
|   Primitive (BlitIndexedToRGBAOutput etc): |
|     for each game pixel (col, row):        |
|       compute physical block               |
|         pXStart = floor((gx+col) * scale)  |
|         pXEnd   = floor((gx+col+1)*scale)  |
|         pYStart = floor((gy+row) * scale)  |
|         pYEnd   = floor((gy+row+1)*scale)  |
|       fill out.image() in that block       |
|       using out.bufferStride() for rows    |
+--------------------------------------------+
                      │
                      ▼
+--------------------------------------------+
|   Display._data (physical-res RGBA bytes)  |
+--------------------------------------------+
                      │
                      ▼
+--------------------------------------------+
|   Engine: SDL_UpdateTexture at phys size   |
|           SDL_RenderCopy 1:1 to window     |
+--------------------------------------------+
```

The engine's `renderScreenRGBA` allocates `_screenTexture` at physical resolution, uploads `display.image()` (already physical-sized bytes), and `RenderCopy`s with `dst=nullptr`. `SDL_RenderSetLogicalSize` is left configured at game dimensions for the rest of the game (mouse coordinate translation, etc.) — but for the texture copy the renderer doesn't actually need to scale because the texture is already at physical size and the dst rect covers the full window.

## Implementation steps

Numbered for incremental landing — each step should leave the build green and the game runnable.

### Step 1 — `Image` virtuals for buffer size and scale

Add to [src/engine/image.h](src/engine/image.h):

```cpp
class Image {
    ...
    // Actual byte stride of the backing buffer in pixels. Default = width(); Display
    // overrides to return the physical-resolution width.
    virtual int32_t bufferStride() const { return _width; }

    // Actual number of rows in the backing buffer. Default = height(); Display
    // overrides to return the physical-resolution height.
    virtual int32_t bufferHeight() const { return _height; }

    // Ratio of physical pixels to logical (game) pixels for this image. Default = 1.0;
    // Display overrides to return its getPhysicalScale().
    virtual float physicalScale() const { return 1.0f; }
    ...
};
```

These are read-only accessors; no change to constructors or move/copy semantics. Cost is one v-table per Image — affordable since there are not many Image instances per frame on the hot path (most are stack-local Sprites, but Sprites already inherit from Image so they pay the cost; if profiling flags this, we can specialise).

### Step 2 — `Display` overrides + physical-sized buffer

In [src/engine/screen.h](src/engine/screen.h) and [src/engine/screen.cpp](src/engine/screen.cpp):

```cpp
class Display final : public Image {
    ...
    int32_t bufferStride() const override { return _physWidth; }
    int32_t bufferHeight() const override { return _physHeight; }
    float physicalScale() const override { return getPhysicalScale(); }
    ...
private:
    int32_t _physWidth{ 0 };
    int32_t _physHeight{ 0 };
};
```

`Display::setResolution` becomes:

```cpp
const float scale = ...; // computed from screenSize / gameSize, min(x, y), clamped >= 1
_physWidth  = static_cast<int32_t>(gameW * scale);
_physHeight = static_cast<int32_t>(gameH * scale);

// Move-assign a fresh Image at PHYSICAL dimensions, then patch _width/_height
// down to game dimensions (so width()/height() report game coords). The buffer
// remains physical-sized.
Image fresh(_physWidth, _physHeight, ImageFormat::RGBA_32BIT);
fresh.reset();
Image::operator=(std::move(fresh));
// _width/_height got set to physical via the move; we want game dims publicly:
// expose a friend or helper to set them directly without re-allocating.
_setLogicalDimensions(gameW, gameH);   // new private helper
```

The `_setLogicalDimensions` helper writes `_width` and `_height` directly without touching `_data`. (Friend access from Display, or a protected `Image::_setDimensions` method.)

### Step 3 — Image methods that touch the whole buffer

`Image::reset()`, `Image::fill()`, `Image::copy()` currently use `_width * _height` to compute byte counts. For Display these would be wrong (game W × game H, but the buffer is physical W × physical H). Update them to use `bufferStride() * bufferHeight()` for RGBA byte sizing:

```cpp
void Image::reset() {
    if (empty()) return;
    if (_format == ImageFormat::RGBA_32BIT) {
        const size_t total = static_cast<size_t>(bufferStride()) * bufferHeight();
        memset(image(), 0, total * 4);
    } else {
        ... // indexed path unchanged
    }
}
```

Same pattern for `fill()` and `copy()`. Existing call sites continue to work because for non-Display Images, `bufferStride() == width()`.

### Step 4 — RGBA-out primitives: scaling helper

Add a single helper at the top of [src/engine/image.cpp](src/engine/image.cpp) (anonymous namespace):

```cpp
struct PhysicalBlock {
    int32_t pXStart;
    int32_t pXEnd;
    int32_t pYStart;
    int32_t pYEnd;
};

inline PhysicalBlock toPhysicalBlock(int32_t gameX, int32_t gameY, float scale, int32_t bufW, int32_t bufH) {
    PhysicalBlock b;
    b.pXStart = std::max<int32_t>(0, static_cast<int32_t>(gameX * scale));
    b.pXEnd   = std::min<int32_t>(bufW, static_cast<int32_t>((gameX + 1) * scale));
    b.pYStart = std::max<int32_t>(0, static_cast<int32_t>(gameY * scale));
    b.pYEnd   = std::min<int32_t>(bufH, static_cast<int32_t>((gameY + 1) * scale));
    return b;
}
```

Then every RGBA-out path follows this template:

```cpp
const float scale = out.physicalScale();
const int32_t bufStride = out.bufferStride();
const int32_t bufHeight = out.bufferHeight();
uint8_t * outBase = out.image();

// Iterate in GAME pixel space (matches caller's coords + clipping)
for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
        // Decode source pixel (palette lookup, or RGBA copy) → r, g, b, [a]
        ...
        // Expand to physical block
        const PhysicalBlock pb = toPhysicalBlock(outX + col, outY + row, scale, bufStride, bufHeight);
        for (int32_t py = pb.pYStart; py < pb.pYEnd; ++py) {
            uint8_t * dstRow = outBase + (static_cast<ptrdiff_t>(py) * bufStride + pb.pXStart) * 4;
            for (int32_t px = pb.pXStart; px < pb.pXEnd; ++px, dstRow += 4) {
                dstRow[0] = r;
                dstRow[1] = g;
                dstRow[2] = b;
                dstRow[3] = a;  // or 255
            }
        }
    }
}
```

For non-Display targets, `scale == 1.0f`, `bufStride == width`, `pb` is a 1×1 block — same cost as today. For Display, each game pixel expands to a `scale × scale` block. At 3× scale that's 9 RGBA writes per game pixel, vs 1 today. Modern CPUs eat this fine for typical UI redraw rates; spell effects on the whole battle area might benefit from a fast-path memcpy when source and dst are both Display (no palette work, no per-pixel scaling).

Primitives to update (search image.cpp for "RGBA_32BIT" branches):

| Primitive helper                       | Used by                                   |
|----------------------------------------|-------------------------------------------|
| `BlitIndexedToRGBAOutput`              | `Blit`, `AlphaBlit` (alpha=255)           |
| `AlphaBlitIndexedToRGBAOutput`         | `AlphaBlit` (alpha < 255)                 |
| `BlitRGBAToRGBAOutput`                 | `Blit` (RGBA → RGBA)                      |
| `AlphaBlitRGBAToRGBAOutput`            | `AlphaBlit` (RGBA → RGBA, alpha < 255)    |
| `CopyIndexedToRGBAOutput`              | `Copy` (indexed → RGBA)                   |
| `CopyRGBAToRGBAOutput`                 | `Copy` (RGBA → RGBA)                      |
| Inline RGBA branches in `ApplyTransform`, `Fill`, `DrawBorder`, `DrawLine`, `SetPixel`, `Resize`, `SubpixelResize`, `Transpose`, `ReplaceColorId`, `Flip`, `ApplyRawPalette` | various |

Public RGBA helpers in image.cpp (operate directly on physical pixels when out is Display):

| Helper             | Treat coords as | Notes |
|--------------------|-----------------|-------|
| `BlitRGBAScaled`   | game coords on out side; pre-scale to physical for the destination rect; source rect is in source's own pixel space | This is the hi-res monster portrait fast path — single source PNG → physical pixel downscale |
| `BlitRGBAScaledAlpha` | same as above |
| `BlitRGBAAlpha`    | game coords; expand to physical blocks per game pixel |
| `DrawLineRGBA`     | game coords; expand each Bresenham pixel to a physical block |
| `CopyRGBA`         | game coords; physical-block expansion |
| `DimRGBA`          | game coords; iterate in physical pixels within each game pixel |

Land them one at a time, with a quick visual check after each. `Blit` first (most call sites), then `Copy`, `Fill`, `AlphaBlit`, then the long tail.

### Step 5 — `BlitRGBAToIndexed` and other RGBA-input cases

When `in` is Display (RGBA, physical-res buffer) and `out` is a normal indexed Image, we need to read FROM the physical buffer at scaled coords. Use `in.bufferStride()` for stride, and read `(gameX * in.physicalScale(), gameY * in.physicalScale())` as the source physical pixel. There's a sampling decision (nearest? average a block?) — nearest is fine for the use case (battle spell effect captures, which already accept some quality loss).

`BlitRGBAToIndexed` and `AlphaBlitRGBAToIndexed` follow the same template but with the source side scaled instead of the destination side.

### Step 6 — `ImageRestorer` for Display

`ImageRestorer(display, x, y, w, h)` currently does `Copy(_image, x, y, _copy, 0, 0, w, h)` where `_copy` is created at `(w, h)` — game dims. For Display this would either:

- Lose physical pixels (if Copy reads game-coord stride), or
- Read out of bounds (if Copy reads bufferStride and writes width-stride into _copy).

Fix: when `_image` is Display, create `_copy` at **physical** dimensions and use a direct byte memcpy:

```cpp
if (&_image == &Display::instance() && _image.format() == ImageFormat::RGBA_32BIT) {
    const float scale = static_cast<Display&>(_image).physicalScale();
    const int32_t physX = static_cast<int32_t>(_x * scale);
    const int32_t physY = static_cast<int32_t>(_y * scale);
    const int32_t physW = static_cast<int32_t>(_width * scale);
    const int32_t physH = static_cast<int32_t>(_height * scale);
    _copy = Image(physW, physH, ImageFormat::RGBA_32BIT);
    const uint8_t * srcBase = _image.image();
    const int32_t srcStride = _image.bufferStride();
    uint8_t * dstBase = _copy.image();
    for (int32_t y = 0; y < physH; ++y) {
        memcpy(dstBase + static_cast<ptrdiff_t>(y) * physW * 4,
               srcBase + (static_cast<ptrdiff_t>(physY + y) * srcStride + physX) * 4,
               static_cast<size_t>(physW) * 4);
    }
}
```

Restore is the inverse — memcpy from `_copy` back into Display's physical region at the same physical coords.

Same change for `update()`. Capture+restore cost grows by `scale²` (for 3× that's 9× the byte volume) — acceptable for dialogs that open/close occasionally.

### Step 7 — Battle spell-effect scratch buffers

Battle effects (`_redrawActionDeathWaveSpell`, `_redrawActionHolyShoutSpell`, `redrawActionEarthquakeSpellPart1`, Armageddon whitening, Lightning, mage missile beam) currently allocate Image scratch buffers at `(_battleAreaWidthPx, _battleAreaHeightPx)` — game pixels — then `CopyRGBA(display, ..., scratch, ...)` and operate on game pixels.

After Display is physical-res, `CopyRGBA(display, ..., scratch, ...)` either:

- (a) Downscale to game res (information loss; the effect frame sequence will look as fuzzy as today's hi-res monsters), or
- (b) Allocate scratch at physical dims too (memory cost, but quality stays sharp).

Recommend (b). Update the scratch allocations to multiply by `display.physicalScale()`:

```cpp
const float scale = display.physicalScale();
const int32_t bufW = static_cast<int32_t>(_battleAreaWidthPx * scale);
const int32_t bufH = static_cast<int32_t>(_battleAreaHeightPx * scale);
fheroes2::Image battleFieldCopyRGBA(bufW, bufH, fheroes2::ImageFormat::RGBA_32BIT);
fheroes2::CopyRGBA(display, _interfacePosition.x, _interfacePosition.y,  // game coords
                   battleFieldCopyRGBA, 0, 0,  // physical coords on scratch
                   _battleAreaWidthPx, _battleAreaHeightPx);              // game coord size
```

`CopyRGBA` will need a "scaled" variant or the existing one needs to handle in/out scale mismatch. Cleanest: add `CopyRGBAScaled` for the cases where in.physicalScale() != out.physicalScale().

The effect math itself (DimRGBA, WhitenRGBA, BlitRGBAAlpha, CreateHolyShoutEffectRGBA, CreateDeathWaveEffectRGBA) operates on the scratch buffer's actual byte layout and doesn't need to know about scale — those just need their parameters in the scratch's own pixel space.

### Step 8 — `renderHiResMonsterPortrait`

Once Display is physical-res, this becomes:

```cpp
void renderHiResMonsterPortrait(const Image & portrait, int32_t gameX, int32_t gameY,
                                int32_t gameWidth, bool flip, uint8_t alpha)
{
    // Just call BlitRGBAScaled with game coords. The primitive scales the dst
    // rect to physical pixels internally and writes the source PNG → physical
    // in one downscale.
    Display & display = Display::instance();
    if (display.empty()) return;

    const int32_t dstH = static_cast<int32_t>((static_cast<int64_t>(portrait.height()) * gameWidth) / portrait.width());
    if (alpha >= 255) {
        BlitRGBAScaled(portrait, display, gameX, gameY, gameWidth, dstH, flip);
    } else {
        BlitRGBAScaledAlpha(portrait, display, gameX, gameY, gameWidth, dstH, alpha, flip);
    }
}
```

Source PNG → physical pixels in a single bilinear/nearest pass. No upscale by SDL afterwards; texture upload is 1:1.

### Step 9 — Engine: physical-res streaming texture

`renderScreenRGBA` already uploads `display.image()` to `_screenTexture`. Update the texture to be sized at Display's *physical* dims (use `display.bufferStride()`/`display.bufferHeight()`):

```cpp
const int32_t w = display.bufferStride();
const int32_t h = display.bufferHeight();
if (w != _screenTextureW || h != _screenTextureH) {
    SDL_DestroyTexture(_screenTexture);
    _screenTexture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    ...
}
SDL_UpdateTexture(_screenTexture, nullptr, display.image(), w * 4);

// Disable logical-size scaling for this RenderCopy so the physical-res texture lands 1:1
// on the physical window. Re-enable game-coord logical size after present so mouse events
// continue to translate to game coords.
SDL_RenderSetLogicalSize(_renderer, 0, 0);
SDL_RenderClear(_renderer);
SDL_RenderCopy(_renderer, _screenTexture, nullptr, nullptr);
SDL_RenderPresent(_renderer);
SDL_RenderSetLogicalSize(_renderer, display.width(), display.height());
```

The window-size letterboxing (handled today by `SDL_RenderSetLogicalSize` + the renderer's own scaling) needs explicit dst-rect computation when logical-size is disabled — see the original `renderScreenRGBA` from before commit `43bd1053a` for that letterbox math; it was deleted along with the physical-res texture handling.

## Risks and gotchas

- **Per-pixel cost**: every primitive does N² writes per game pixel. At 3× this is 9× the per-primitive RGBA byte writes. Profile after step 4 lands; if hot, add SIMD or memcpy fast-paths for solid-fill cases.
- **Memory**: physical-res Display at 1920×1080 is 8 MB vs 1.2 MB at 640×480 game res. ImageRestorer captures grow proportionally — a full-screen restorer at 3× costs 9× memory. Profile dialog-heavy scenes.
- **Fractional scales**: `physicalScale = min(screenW/gameW, screenH/gameH)` is often non-integer (e.g. 1080/480 = 2.25). The `toPhysicalBlock` helper handles fractional via floor on each side — adjacent game pixels' blocks tile correctly without gaps, but the block sizes vary by 1 physical pixel. Visual artefacts should be minimal; verify with a checkerboard test pattern.
- **`Image::operator=` and `Image::copy`**: the existing copy logic uses `_width * _height` for sizing. For Display this would size by *game* pixels not the full physical buffer. A direct `Display = someOtherImage` assignment isn't called anywhere I can see, but worth searching — if anyone does `display = other`, that needs a Display-specific override.
- **Sprite caching**: ICN sprites stay indexed (small memory). The CHANGE is in their final blit destination, not the source. No cache changes needed.
- **`Sprite` inherits from Image**: Sprites get the `bufferStride()/physicalScale()` virtuals via inheritance. Default values match their dimensions — no behavioural change. v-table cost on stack-local Sprites is one extra word per instance; likely fine.
- **Battle `_battleGroundRGBA`**: this is a regular Image (RGBA at game res). Should it become physical-res too? Yes — `_copyFullSurface` pushes it to Display each frame, so a physical-res `_battleGroundRGBA` writes directly to physical pixels with no resampling. Bigger memory (battle area at 3× = ~3 MB), but better quality.
- **Color cycling postponement still applies**. This refactor doesn't change `changePalette`'s no-op behaviour for cycling animations.

## Verification

1. Build clean.
2. Main menu fades in cleanly with no dithering noise (regression test for the bug fixed in current commit).
3. Adventure map renders correctly. Status panel hi-res monsters (Thor, Succubus, Azure Dragon) **visibly sharper** than before — pixel-perfect in PNG details vs the bilinear-blur of the current state.
4. Open Select Monster dialog (BattleOnly) — Thor / Succubus / Dachshund portraits visible and sharp from the moment the dialog opens (regression test for the overlay-flicker bug).
5. Open every dialog; hi-res portraits appear correctly; close-and-reopen works.
6. Battle: enter combat, all spell effects visible at correct quality (Lightning, Death Wave, Holy Shout, Armageddon, Earthquake, Cold Ring).
7. Battle ↔ adventure round-trip — status panel portraits re-populate.
8. Fade transitions: battle entry/exit, main menu fade-in, scenario-info fade — smooth, no flashes of black, no dithering.
9. Letterbox / pillarbox solid black at the edges of the upscaled framebuffer.
10. Single SDL upload + RenderCopy per frame, no overlay/composite passes.
11. FPS doesn't regress meaningfully on the adventure map vs the current commit.

## Migration order recommendation

The safest order to land this:

1. Steps 1–3 (Image virtuals + Display physical buffer + buffer-aware reset/fill/copy). Display still works at game res since `physicalScale()` returns 1.0 when `_screenSize == game size`. Verify no regressions.
2. Step 9 (engine renderScreenRGBA uses bufferStride/bufferHeight). Still no visual change in the 1× case.
3. Set Display's `physicalScale()` to actually return `getPhysicalScale()`. **Now things break** — every primitive writes at 1× into a Nx-sized buffer. Game appears in a small corner.
4. Step 4 (RGBA-out primitives scale). One primitive at a time; after each, the corresponding parts of the UI render correctly. Land them in this order: `Blit`, `Copy`, `Fill`, `AlphaBlit`, `BlitRGBAScaled[Alpha]`, then the long tail.
5. Step 6 (`ImageRestorer` physical-pixel handling). Without this, dialogs corrupt on close.
6. Step 5 (RGBA-input cases for `BlitRGBAToIndexed`).
7. Steps 7–8 (battle spell effects + `renderHiResMonsterPortrait` simplification).

Estimated scope: ~600–1000 lines of changes, mostly mechanical, concentrated in `image.cpp` (~80%), with smaller edits to `screen.cpp`/`screen.h`, `image.h`, `agg_image.cpp`, and `battle_interface.cpp`.

## Build / debug quick-ref

```
# Release build
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Release /p:Platform=x64 /m

# Debug build (with PDB, assertions, JIT attach)
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Debug /p:Platform=x64 /m
```

MSBuild typically lives at:
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe`

Game executable: `build/Release/fheroes2.exe`.

## Reference: deleted overlay attempts

If revisiting an overlay approach (e.g. in a future shader-LUT world where palette cycling is solved), the relevant reverted code lived in:

- `Display::_hiResOverlay`, `acquireHiResOverlay()`, `clearHiResOverlayRect()` in `screen.h`/`screen.cpp`
- `_hiResOverlayTexture` in the SDL `RenderEngine` in `screen.cpp`
- `ImageRestorer::_overlayCopy`, `_captureOverlay()`, `_restoreOverlay()` in `image.h`/`image.cpp`
- The version of `renderHiResMonsterPortrait` that wrote to the overlay at physical coords in `agg_image.cpp`

Both attempts can be retrieved from this branch's reflog around commits before `43bd1053a`.
