# Pure RGBA Display — eliminate the indexed buffer

## Why

The current Display owns *two* buffers: an 8-bit indexed framebuffer (`Image::_data`)
and an RGBA presentation surface (`_screenRGBA`). The two are kept in sync by
`Image::WriteHook`, which fires after every drawing primitive and re-mirrors the
just-written rect into `_screenRGBA` at physical scale.

This works, but the painter algorithm is fragile by design:

- The hook fires on the **bounding rect** of every primitive, not on the actually-modified
  pixels. A `Blit` of a sprite with a transparent center re-mirrors the unchanged center
  pixels of `display.image()` into `_screenRGBA`. If a hi-res RGBA paint
  (`renderHiResMonsterPortrait`, battle scene RGBA-direct write) had landed there
  earlier in the frame, it gets erased by the re-mirror.
- The "skip index 0" carve-out in `mirrorIndexedRectToScreenRGBA` is the only thing
  that protects RGBA-direct content where palette content happens to be index 0
  (battle area). Anywhere else with hi-res content is at the mercy of three loose
  contracts:
    1. Hook skips index 0.
    2. `ImageRestorer` snapshots `screenRGBA`.
    3. **Widgets must call `renderHiResMonsterPortrait` last** in their redraw.
- Contract 3 is convention, not invariant. `ArmyBar::RedrawItem`'s `spcursor.show()`
  after the hi-res portrait is the canonical violation: the cursor-border `Blit`'s
  bounding rect covers the entire slot, hook re-mirrors, hi-res erased.

A **pure RGBA Display** removes the conflict by construction. There is one buffer,
one write target, one Z-order — execution order. No hook, no skip, no snapshot,
no convention.

The cost is solving color cycling without an indexed buffer to swap the palette table
under. The recommended path: shader-based palette LUT (this plan), with per-sprite
re-render as a fallback for SDL2-without-shaders builds.

## Final state

```
+----------------------------------+         +-------------------+
|   Drawing primitives (RGBA)      |  -->    |  Display._data    |   --> SDL_UpdateTexture
|   Blit, Fill, AlphaBlit, ...     |         |  RGBA32 @ phys px |       SDL_RenderCopy
+----------------------------------+         +-------------------+

Sprite caches stay 8-bit indexed (cheap memory).
Sprite -> Display blit performs palette[index] -> RGBA per pixel.
```

`Display::_data` is RGBA at physical resolution. There is no `_screenRGBA`, no
WriteHook, no mirror, no ownership mask. ImageRestorer saves/restores a single
RGBA `_copy`.

## What gets deleted

- `Image::_onWrite` / `_setWriteHook` / `_notifyWrite` and every call site.
- `mirrorIndexedRectToScreenRGBA` and the lambda installed in `setResolution`.
- `Display::_screenRGBA` and `screenRGBA()` — Display's framebuffer is its own `_data`.
- `paintPaletteToScreenRGBA` (only remaining use was `changePalette` re-mirror;
  replaced by shader LUT or per-sprite re-render).
- `BlitIndexedToRGBAScaled`, `BlitIndexedToRGBAScaledAlpha`, `BlitIndexedToRGBAScaledRegion`
  — replaced by `Blit`/`AlphaBlit`/`Copy` to RGBA Display.
- The "skip index 0" semantics throughout.
- `ImageRestorer::_rgbaCopy` and `_captureRGBA`/`_restoreRGBA`.
- Battle's `_battleGroundRGBA` separate cache (just paint to Display directly).

## What gets added or rewritten

### 1. RGBA paths in every drawing primitive (`src/engine/image.cpp`)

Each primitive that today writes to an indexed `Image` needs an RGBA path for when
`out.format() == RGBA_32BIT`. Pseudocode pattern:

```cpp
void Blit(in, ..., out, ...) {
    if (out.format() == RGBA_32BIT) {
        if (in.format() == RGBA_32BIT) {
            BlitRGBAToRGBA(...);   // memcpy + alpha blend
        } else {
            BlitIndexedToRGBA(...); // palette[index] -> RGBA per pixel
        }
        return;
    }
    // existing indexed path stays (sprite-to-sprite blits still happen)
}
```

Primitives needing RGBA paths:
- `Blit` (5 overloads + 4 helpers)
- `AlphaBlit` (3 overloads)
- `ApplyAlpha`
- `Copy` (3 overloads)
- `Fill` (already has RGBA path — reuse)
- `Flip` (writing overload)
- `DrawLine`, `DrawRect`, `DrawBorder`
- `SetPixel` (2 overloads)
- `Resize`, `SubpixelResize`
- `Transpose`
- `ReplaceColorId`, `ReplaceColorIdByTransformId`, `ReplaceTransformIdByColorId`
- `addGradientShadow`, `addGradientShadowForArea` (translate transform-layer
  shadow semantics to RGBA darken)
- `CreateDitheringTransition`
- `ApplyPalette` family — see below

Many of these already have partial RGBA support (`BlitRGBAToIndexed`,
`BlitIndexedToRGBAOutput`, `CopyIndexedToRGBAOutput`, the `BlitIndexedToRGBAScaled*`
family). Consolidate into a uniform "write to RGBA out" interface.

### 2. Display becomes RGBA-format

```cpp
Display::Display()
    : _engine(...), _cursor(...)
{
    _format = ImageFormat::RGBA_32BIT;
    _singleLayer = true;
}
```

`Image::resize` already allocates `size * 4` bytes for RGBA format. `Image::image()`
returns `_data.get()` which is now RGBA bytes (4 bytes/pixel stride).

`Image::reset()` needs to zero the buffer (already does for RGBA).
`Image::fill(value)` for RGBA — interprets `value` as a palette index, paints
opaque palette[index] across the buffer.

### 3. Transform layer semantics for RGBA

The transform layer (`Image::transform()`) carries:
- 0: opaque pixel — copy as-is.
- 1: transparent — skip.
- 2..5: shadow strength — `transformTable[transformId][destIndex]` darkens the
  destination palette index.

For RGBA destinations:
- 0: copy palette[srcIndex] -> RGBA(r, g, b, 255).
- 1: skip (don't write).
- 2..5: read destination RGBA, darken RGB by precomputed factor (e.g., `0.75^N`),
  keep alpha. Write back.

Precompute the darken factors from the existing `transformTable[2..5]` so the
visual result matches the palette path.

### 4. Color cycling — shader LUT (preferred)

Replace the indexed buffer's "swap palette table → re-mirror" trick with a shader
that does the palette lookup at upload time.

Two-channel approach:
- The "RGBA" framebuffer encodes per pixel: `(paletteIndex, 0, 0, alpha)` for
  cycling pixels, `(R, G, B, 255)` with R/G/B bit 7 cleared for non-cycling.
  Detect "is this a cycling pixel" by a flag.
- Cleaner: a parallel 1-byte/pixel "cycling index" buffer. Shader samples both
  the RGBA framebuffer AND the cycling-index buffer + a palette LUT uniform.
  For cycling pixels (cycling-index != 0), output = palette_lut[cycling-index].
  For non-cycling, output = RGBA framebuffer pixel.

`changePalette` updates the shader's palette uniform — no re-render needed.
Color cycling at 60 FPS becomes free.

Implementation: SDL2 with the OpenGL backend supports custom shaders via
`SDL_RenderGeometry` + `SDL_RenderTarget`. Or migrate to SDL3 which has
proper shader support.

### 5. Color cycling — per-sprite re-render (fallback)

For builds without shader support: maintain a list of "active cycling regions"
on Display. Each entry: `(rect, sprite *, frame_index)` for sprites painted
with cycling colors. On `changePalette`:
- For each entry: re-blit the sprite at its rect with the new palette.

Cost: O(cycling_sprites * sprite_size) per palette change. Cycling fires every
~150 ms (`AGG::ApplyICNCycling`), so amortised cost is low.

Most sprites don't use cycling indexes (214–217, 218–221, 231–235, 238–241).
Detect at sprite-load time and tag.

### 6. Battle drawing simplifies

Battle's helpers (`_blitOnSurface`, `_alphaBlitOnSurface`, `_copyOnSurface`,
`_copyFullSurface`) become thin wrappers over `Blit`/`AlphaBlit`/`Copy`/etc.
into Display directly. The `BlitIndexedToRGBAScaled*` family disappears.

The `_battleGroundRGBA` cache can stay (faster than re-painting from sprites
each frame), or be replaced by re-rendering the battlefield each frame at
Display resolution — depends on perf measurement.

`_battleAreaWidthPx`, `_battleAreaHeightPx` go away (Display is already at
physical resolution; just use `_interfacePosition` rect directly with
appropriate scaling).

The `Fill(0)` clear in `redrawPreRender` goes away — it was a WriteHook
transparency gate, meaningless without the hook.

### 7. ImageRestorer simplifies

```cpp
class ImageRestorer {
    Image & _image;
    Image _copy;        // RGBA when _image is RGBA-format Display
    int32_t _x, _y, _width, _height;
    bool _isRestored;
};
```

No `_rgbaCopy`, no `_captureRGBA`, no `_restoreRGBA`. The single `Copy()` call
saves/restores RGBA bytes.

### 8. Engine upload path

`_engine->renderScreenRGBA(*this)` becomes `_engine->renderDisplay(*this)` (or
just stays — name is already accurate). The engine reads `display.image()`
(RGBA bytes) and uploads via `SDL_UpdateTexture`. Color cycling shader sits
between the texture and the final output.

### 9. SMK video decoder

`smk_decoder` writes raw 8-bit indexed bytes to `image.image()`. With RGBA
Display, video frames need palette → RGBA conversion. Two options:
- Modify the decoder to write RGBA when target format is RGBA (palette lookup
  per pixel). Cleaner but adds RGBA awareness to the decoder.
- Decode to a private indexed buffer, then `Blit` (with palette conversion)
  to Display. Simpler.

The fade-on-end (`colorFade`) uses palette swap during video playback. With
shader LUT, this is just a uniform update per fade frame.

## Migration approach

Per-primitive, with feature flag. Until all primitives have RGBA paths, Display
stays indexed and the WriteHook stays. Switch primitive by primitive:

1. Pick a primitive (start with the most-used: `Blit`, `Fill`, `Copy`).
2. Add its RGBA path. Verify visually with a test where Display is set to
   RGBA format temporarily.
3. Once all primitives have RGBA paths, flip Display's format to RGBA.
4. Delete the WriteHook, `_screenRGBA`, mirror functions.

Estimated scope: 2-3 weeks of focused work. The primitives are mostly mechanical
adaptations of existing logic. The shader for color cycling is the largest
unknown.

## Risks and gotchas

- **Performance**: every Blit converts palette → RGBA per pixel. Caching
  RGBA versions of frequently-used sprites (in addition to or instead of
  indexed source) could mitigate. Measure first.
- **Memory**: Display's framebuffer at physical resolution. 1920x1080 = 8 MB
  RGBA vs 2 MB indexed. Acceptable on modern machines, possibly concerning
  on embedded targets (PS Vita, Android low-end).
- **Transform-layer-using sprites**: shadow rendering in particular. The RGB
  darken approximation should be visually indistinguishable but won't match
  palette result bit-exact. Verify with side-by-side screenshots.
- **`ApplyPalette`**: used for grayed-out / mirror-image / stoned units.
  Original was a palette index remap (`output_index = remap[input_index]`).
  RGBA equivalent: ?
    - Option A: pre-compute and cache RGBA versions of all sprites under each
      palette transform. ~5 transforms × all-sprites = manageable cache.
    - Option B: detect "this RGB came from palette index N" via reverse-lookup
      table, then remap. Lossy if the source RGBA didn't come from a palette.
    - Option C: keep a parallel "palette index" buffer per sprite. At blit
      time, palette → RGBA happens after any ApplyPalette transform.
    - Recommend A: pre-compute. Memory cost vs runtime simplicity.
- **`ImageRestorer` cost**: bytes/snapshot grows 4×. Most ImageRestorer
  uses are small dialog rects; total memory probably fine. Profile.
- **Color cycling correctness**: every ICN that cycles needs to be tagged
  correctly. Audit `AGG::ApplyICNCycling` to see which ICNs are involved.
- **Embedded platform shader support**: PS Vita backend uses `vita2d`, not
  SDL_Renderer. Color cycling there needs the per-sprite re-render fallback.
- **Cursor**: software cursor is currently `BlitIndexedToRGBAScaled` into
  `_screenRGBA`. Becomes `Blit` indexed → RGBA Display.

## Verification

1. Build clean.
2. Adventure map renders correctly. Status panel hi-res monsters appear
   (Thor, Succubus, Azure Dragon).
3. Color cycling animations (gold, water, lava) update on screen at correct
   cadence (~6 FPS).
4. Open every dialog; hi-res portraits appear; sub-modals work.
5. **Z-order regression test**: Open BattleOnly setup, click slots, hover
   slots — hi-res minis stay visible (the failing case from the previous
   architecture).
6. Battle: enter combat with custom monsters, fight, all spell effects
   visible (lightning, death wave, holy shout, armageddon, earthquake,
   cold ring).
7. Battle ↔ adventure round trip — status panel portraits re-populate.
8. Fade transitions: battle entry/exit, main menu fade-in, scenario-info
   fade — smooth, no flashes of black.
9. Letterbox / pillarbox solid black.
10. Single SDL upload + RenderCopy per frame.
11. **Initial videos play correctly** (the failing case after the previous
    refactor).

## Build / debug quick-ref

```
# Release
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Release /p:Platform=x64 /m

# Debug (with PDB, assertions, JIT attach)
MSBuild build/fheroes2.sln /t:fheroes2 /p:Configuration=Debug /p:Platform=x64 /m
```
