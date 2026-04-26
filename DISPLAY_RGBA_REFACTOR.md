# Display = single RGBA surface

## Goal

`Display` owns one RGBA framebuffer at physical scale. Every drawing call to
`Display` (palette `Blit`, `Fill`, `ApplyAlpha`, hi-res `BlitRGBA*`, …) writes
into that one surface, in document order. `Display::render()` is a single SDL
upload + present — nothing else.

No registration. No buffer-paint pool. No forwarding stack. No scope keys.
No per-scope RGBA targets. No palette mirror pass. No Z-order filter.
Order of execution **is** order of pixel writes; later writes cover earlier
writes — that's the painter algorithm.

## What this deletes

The accumulated workarounds, in one block:

- `_rgbaBufferPaints` pool (`screen.h`, `screen.cpp`)
- `RGBABufferPaint` struct, `registerRGBABufferPaint`,
  `removeRGBABufferPaintAt`, `removeRGBABufferPaintsForScope`,
  `removeRGBABufferPaintsForTarget`, `removeRGBABufferPaintsInRect`
- `Image::DialogForwardingFrame`, the forwarding stack, `pushDialogForwarding`,
  `popDialogForwarding`, `getActiveDialogForwarding`, `getDialogFwdDepth`,
  `getDialogFwdStack`, `setDialogForwarding`, `clearDialogForwarding`,
  `suspendDialogForwarding`, `resumeDialogForwarding`,
  `_dialogFwdTarget`, `_dialogFwdSuspendDepth`, `ScopedDialogForwarding`
- `paintPaletteToScreenRGBA` (the per-frame palette → `_screenRGBA` mirror loop)
- All 13 forwarding-guard structs (`AdventureMapForwardingGuard`,
  `CastleDialogForwardingGuard`, `KingdomOverviewForwardingGuard`,
  `MeetingForwardingGuard`, `HeroDialogForwardingGuard`,
  `DialogSurfaceGuard` ×4, `BattleOnlyForwardingGuard`,
  `SummaryForwardingGuard`, `CastleDetailsForwardingGuard`)
- The `BlitIndexedToRGBAScaledRegion` snapshot at every dialog open
- Battle's `_mainSurfaceRGBA` member and `_rgbaScale` (battle draws into
  `Display::screenRGBA()` directly with an offset, no per-scope buffer)
- All `removeRGBABufferPaintsInRect` / `ForTarget` calls in widgets
  (`ArmyBar::Redraw`, `Dialog::ArmyInfo::DrawMonster`,
  `SelectEnumMonster::Redraw`, `drawMiniMonsters`,
  `Battle::Interface::redrawPreRender`, `Battle::Interface::~Interface`)

## Architecture

`Display` keeps an indexed backing store (the existing 8-bit buffer is what
hundreds of `Blit(src, display, x, y)` call sites work against) **plus** an
`_screenRGBA` at physical scale. The trick is: **every write into `Display`'s
indexed buffer also mirrors into `_screenRGBA` immediately, at scaled coords**.
There is no separate mirror pass.

Mechanism: a write hook on `Image`.

```cpp
class Image {
    using WriteHook = std::function<void(int32_t x, int32_t y, int32_t w, int32_t h)>;
    WriteHook _onWrite;  // null for normal images; Display sets it.
};
```

Every drawing primitive that mutates `out._data` calls `out._onWrite(roi)` at
the end. Display sets `_onWrite` to mirror the just-written rect from indexed
→ `_screenRGBA` at physical scale. Sprites have null hook — zero overhead.

### Hi-res RGBA writes

`renderHiResMonsterPortrait` (or its replacement) becomes a direct
`BlitRGBAScaled` into `Display::screenRGBA()` at `gameX*scale, gameY*scale`,
called at the same point in widget code where the indexed mini-monster
sprite would have been blitted. Whatever palette was there is overwritten —
correct order by construction. There is no registration; no pool; nothing
to clean up.

### Color cycling

`Display::changePalette` triggers a full re-mirror of indexed → `_screenRGBA`
once per palette change (not per frame). Implementation: walk the indexed
buffer, write RGBA into `_screenRGBA` using the new palette table. This is
the same loop body as the deleted `paintPaletteToScreenRGBA`, just gated on
palette change instead of running every frame.

Color cycling fires roughly once per ~150ms (`AGG::ApplyICNCycling`). The
re-mirror cost is amortised vs. the current per-frame loop — net win.

### Fade transitions

`fadeDisplay` in `ui_tool.cpp` already calls `Fill` + `ApplyAlpha` per frame
— each call hits the hook, `_screenRGBA` updates incrementally. Nothing
special.

Battle's fade-in/out (`Battle::Interface`) walks frames calling `ApplyAlpha`
on the palette buffer. Same — hook mirrors each step.

### Battle's `_mainSurfaceRGBA`

Today battle has its own `_mainSurfaceRGBA` and writes battle scene with
~60 direct `BlitRGBAScaled` / `BlitIndexedToRGBAScaledRegion` calls into it.
The whole thing then composites onto `Display::_screenRGBA` once per frame.

After the refactor: battle writes directly to `Display::screenRGBA()` at
`(_interfacePosition.x * scale, _interfacePosition.y * scale, …)`. The
`_mainSurfaceRGBA` member is gone; the existing `BlitRGBAScaled(...,
_mainSurfaceRGBA, ...)` call sites become `BlitRGBAScaled(..., screenRGBA,
absX, absY, ...)`. Mechanical sed-style replacement of ~60 sites; battle's
local helpers (`drawTroopRGBASprite`, `drawIndexedSprite*` wrappers in
`battle_interface.cpp:2293-2310`) take the offset once and pass through.

### Cursor

Cursor backup/restore over the indexed buffer disappears. Cursor is a
direct `BlitIndexedToRGBAScaled` into `_screenRGBA` after the last widget
draw — same as `renderHiResMonsterPortrait`. The hook mirror covers any
palette draws that landed earlier in the frame, so cursor lands on top.

To restore previous-frame `_screenRGBA` content under the cursor (so the
cursor doesn't leave a trail), the simplest path is: cursor blits into
`_screenRGBA` right before `Display::render()` presents; before the next
frame's first palette write hits the hook (at the relevant pixel), the
hook overwrites the cursor pixel with palette content. So no explicit
cursor-area undo is needed if every frame redraws the area under the cursor
via palette. If a frame doesn't redraw that area, residual cursor pixels
remain — same as the old palette-buffer cursor backup/restore problem,
solved the same way: the cursor area is part of `_prevRoi` and gets
redrawn in the next frame's drawing pass.

## Implementation steps

### Step 1 — add the write hook (additive)

`image.h` / `image.cpp`:
- Add `std::function<void(Rect)>` member `Image::_onWrite` and a setter.
- Add `Image::_notifyWrite(int32_t x, int32_t y, int32_t w, int32_t h)`
  helper that calls `_onWrite` if set.

Don't call it from anywhere yet. Build clean.

### Step 2 — wire the hook through every drawing primitive that writes to `out._data`

Each primitive ends with `out._notifyWrite(roi)`. Functions:
- `Blit` (5 overloads, line 378-384)
- `AlphaBlit` (3 overloads, 359-363)
- `ApplyPalette` (5 overloads, 365-371)
- `ApplyAlpha` (373)
- `Copy` (3 overloads, 386-388)
- `DrawBorder`, `DrawLine`, `DrawRect` (405, 408, 410)
- `Fill` (419)
- `Flip` (the writing overload, 428)
- `ReplaceColorId`, `ReplaceColorIdByTransformId`,
  `ReplaceTransformIdByColorId` (440-446)
- `Resize`, `SubpixelResize` (writing overloads, 448-468)
- `SetPixel` (2 overloads, 454-456)
- `Stretch` (461) — returns new Image, no hook on caller
- `Transpose` (468) — returns to `out`, hook
- `addGradientShadow`, `addGradientShadowForArea` (352-353)
- `CreateDitheringTransition` (399)

Skip primitives that only touch the transform layer or read state:
`copyTransformLayer`, `ApplyTransform`, `FillTransform`, `SetTransformPixel`,
`updateShadow`, `FilterOnePixelNoise` (returns new image), `Crop`,
`CreateContour` (return new image), `DivideImageBySquares` (analysis only).

Most primitives get a 1-line addition at the end. Build clean after each
batch of related ones.

### Step 3 — Display registers the mirror hook

In `Display::setResolution` / `resetRenderer`, after `_screenRGBA` is
allocated, set `_onWrite` to a lambda that mirrors the just-written area:

```cpp
_onWrite = [this](const Rect & roi) {
    BlitIndexedToRGBAScaledRegion(*this, roi.x, roi.y, roi.width, roi.height,
                                   _screenRGBA,
                                   static_cast<int32_t>(roi.x * physicalScale),
                                   static_cast<int32_t>(roi.y * physicalScale),
                                   physicalScale);
};
```

Now every `Blit` to `Display` writes both indexed and `_screenRGBA`. Verify
visually — the existing `paintPaletteToScreenRGBA` loop in `render()` is now
redundant. Remove it.

### Step 4 — replace `renderHiResMonsterPortrait` with direct blit

```cpp
void renderHiResMonsterPortrait(const RGBAImage & portrait, int32_t gameX,
                                 int32_t gameY, int32_t gameWidth, bool flip,
                                 uint8_t alpha) {
    Display & display = Display::instance();
    const float scale = display.getPhysicalScale();
    const int32_t srcW = portrait.width(), srcH = portrait.height();
    const int32_t dstW = static_cast<int32_t>(gameWidth * scale);
    const int32_t dstH = static_cast<int32_t>(static_cast<int64_t>(srcH) * dstW / srcW);
    const int32_t dstX = static_cast<int32_t>(gameX * scale);
    const int32_t dstY = static_cast<int32_t>(gameY * scale);
    if (alpha >= 255)
        BlitRGBAScaled(portrait, display.screenRGBA(), dstX, dstY, dstW, dstH, flip);
    else
        BlitRGBAScaledAlpha(portrait, display.screenRGBA(), dstX, dstY, dstW, dstH, alpha, flip);
}
```

No registration, no pool entry. Order of execution is order of pixel writes.

### Step 5 — battle uses `_screenRGBA` directly

`battle_interface.cpp` / `battle_interface.h`:
- Drop `_mainSurfaceRGBA`, `_rgbaScale` members.
- Replace `_mainSurfaceRGBA` references with `Display::instance().screenRGBA()`
  and add `_interfacePosition.{x,y} * scale` to coords. Already-converted
  helpers (`drawIndexedSprite*`, lines 2293-2310) just take an extra offset.
- Drop the `pushDialogForwarding` / `popDialogForwarding` calls in ctor/dtor.
- Drop the `removeRGBABufferPaintsForTarget(&_mainSurfaceRGBA)` calls.
- Drop `Fill(..., 0)` in `redrawPreRender` (`battle_interface.cpp:1750-1751`).
  No longer needed — the `_screenRGBA` area is overwritten by battle's own
  RGBA writes; no palette zero gate is needed.
- `_battleGroundRGBA` (a pre-converted background cache) keeps its purpose
  as a cached source; just blits to `screenRGBA()` at offset instead of to
  `_mainSurfaceRGBA`.

### Step 6 — handle palette change

`Display::changePalette`: walk the indexed buffer, re-render `_screenRGBA`
from scratch using the new palette. Same loop that used to live in
`paintPaletteToScreenRGBA`, called only on palette change.

### Step 7 — delete the workaround code

In one commit:
- Drop `_rgbaBufferPaints`, all `registerRGBABufferPaint*` /
  `removeRGBABufferPaints*` methods.
- Drop the entire forwarding stack API in `image.h` / `image.cpp`.
- Drop all 13 guard structs in their respective files.
- Drop `paintPaletteToScreenRGBA` from `screen.cpp`.
- `Display::render()` becomes:
  ```cpp
  void Display::render(const Rect & roi) {
      // _screenRGBA already current — every draw mirrored on the way in.
      _engine->renderScreenRGBA(*this);
      if (_postprocessing) _postprocessing();
      _prevRoi = getActiveArea(roi, width(), height());
  }
  ```

### Step 8 — update docs

`RGBA_MIGRATION_PLAN.md`, `CLAUDE.md` — rewrite the "Hi-res RGBA sprite
pipeline" sections to reflect the single-surface model.

## Risks and edge cases

- **Hook overhead on hot paths**: `Blit` is called heavily. The hook lambda
  has function-pointer indirection cost. If profiling shows this matters,
  switch from `std::function` to a raw function pointer or an inline check.
- **Drawing order in widgets**: hi-res blit must come *after* the palette
  draws it overlays. Already true for `ArmyBar`, `drawMiniMonsters`,
  `Dialog::ArmyInfo`, `MonsterDialogElement`, `SelectEnumMonster`,
  `Battle::TurnOrder`. Verify by reading each call site once.
- **Cursor**: software cursor is drawn directly into `_screenRGBA` last.
  No backup/restore on the indexed buffer needed (the area is redrawn
  next frame by palette draws hitting the hook).
- **Partial redraws using `_prevRoi`**: today `_prevRoi` tracks the indexed
  buffer's dirty rect. With the hook, `_screenRGBA` is always current, so
  partial-render logic is moot — `Display::render` just presents the whole
  surface. Any code that relied on `_prevRoi` to limit work is dead.
- **`Image` virtuality**: adding `_onWrite` to `Image` adds 32-ish bytes per
  Image instance. Sprites are kept around long-term; ~5000 sprites × 32 B
  = 160 KB. Negligible.

## Verification

1. Build clean after each step.
2. Adventure map: status panel custom monsters render once, no flicker.
3. Open every dialog with custom monsters in the army; the dialog's palette
   body covers parent portraits without scope filtering. Close dialog;
   parent portraits return on next adventure redraw.
4. Battle: turn-order portraits, hi-res unit sprites, fade in/out work.
5. Color cycling animations (gold, water, lava palette cycles) update on
   screen.
6. Single SDL_UpdateTexture + SDL_RenderCopy per frame (verify in debugger).
