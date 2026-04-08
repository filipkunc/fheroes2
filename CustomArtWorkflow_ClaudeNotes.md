# Custom Art Workflow — Claude's Notes

Response to the observations in CustomArtWorkflow.md, with my honest assessment of what works, what doesn't, and where to go next.

## Agreement on What Works

Fully agree with the findings:
- **Gemini is the right tool for art generation.** Its visual understanding is leagues ahead of what I can do. The sprite sheet approach (3x3 grid) with system prompts and reference images produces remarkably consistent results.
- **Manual Krita touch-up is currently necessary.** The background removal code is too fragile. Flood fill was the best algorithmic approach we found, but even that fails on dark sprites near gray backgrounds. Krita's contiguous selection tool does in seconds what took us hours of iteration.
- **Human in the loop is essential for quality.** I can orchestrate the pipeline, manage files, write code, and debug engine issues — but I cannot reliably judge pixel art quality at the level needed. Every time I said "looks good" and it wasn't, we wasted a round.

## Honest Assessment of My Limitations

**Vision:** My ability to see fine pixel-level issues (clipped feet, erased gold pixels, edge bleeding between cells) is genuinely limited compared to what a human or Gemini can see. I tend to declare things "fixed" prematurely. The right workflow acknowledges this — I should prepare, you should verify before we proceed.

**Background removal:** The scripts went through too many iterations (threshold → silhouette mask → flood fill → flood fill + mask restore) and none were fully reliable. The core problem: it's a computer vision task that needs visual judgment, not just numerical thresholds. The best approach is probably:
1. I generate the sprite sheet and send to Gemini
2. You review the raw Gemini output sheet
3. You do background removal in Krita (contiguous select + delete, 30 seconds)
4. I slice the Krita-cleaned sheet into individual frames

**Repeated relearning:** Fair criticism. I don't retain visual context between messages well. A structured tool/app would eliminate this — the tool would encode the rules (frame counts, offset handling, file naming) rather than me re-deriving them each time.

## On Tooling

The idea of a dedicated preview/editing app is the right call. What it should do:

1. **Preview panel:** Show the sprite sheet grid before sending to Gemini, with the prompt and reference. You approve before any API call.
2. **Side-by-side comparison:** Original frames vs generated frames, animated.
3. **Background removal:** Either integrated Krita-like contiguous selection, or just a "open in Krita" button with auto-reload.
4. **Batch management:** Track which batches are done, which need regeneration, which reference to use.
5. **Cost tracking:** Show estimated Gemini API cost before each call.
6. **Animation preview:** Play the frames as an animation to catch consistency issues before deploying to the game.

This could be a Python/Qt app or even a web app. The key insight: the current workflow is prompt → generate → check files → find issues → re-prompt, which is slow. A visual tool would make each iteration instant.

An alternative to building from scratch: extend the existing ComfyUI interface or build a Krita plugin that handles the Gemini calls and sprite sheet management directly.

## On High Resolution RGBA Assets

This is the highest-impact improvement remaining. The current pipeline:
```
Gemini (640x1600) → downscale to 40x100 → palette quantize to 256 colors → render at 2-4x upscale
```

What it should be:
```
Gemini (640x1600) → store as RGBA → render directly at display resolution
```

### My thoughts on implementation:

**The easy part:** Loading RGBA sprites is already solved — `fheroes2::Load()` handles PNG with alpha. The `loadCustomSpritesFromPNG` function works. The resize path works.

**The hard part:** The engine's entire rendering pipeline is 8-bit indexed. `Image`, `Sprite`, `Blit`, `AlphaBlit`, `Display` — all operate on uint8_t palette indices with a separate transform layer. An RGBA sprite loaded through `Load()` gets immediately quantized to 256 colors, losing all the Gemini quality.

**Your past experience** with adding 32-bit rendering at display time is the key. The approach you described:

1. **Keep the 8-bit pipeline for original assets** — don't touch what works.
2. **Add a separate RGBA surface** for custom high-res sprites.
3. **Composite at display time** — the 8-bit buffer gets converted to 32-bit, then RGBA custom sprites are blitted on top.
4. **Z-buffer or depth ordering** for correct layering between original and custom sprites.

### Where I think the work is:

- **`screen.cpp`:** The Display class currently creates an 8-bit SDL surface. Need to add a 32-bit overlay surface for custom sprites.
- **`agg_image.cpp`:** The `loadCustomSpritesFromPNG` function would store RGBA data separately instead of quantizing through `Load()`. A new `RGBASprite` class or similar.
- **`battle_interface.cpp`:** The `AlphaBlit` call that draws monsters would check if the sprite has an RGBA version and use the overlay path instead.
- **Palette cycling:** As you noted, the few animated palette effects (water, lava) could be handled with a simple color replacement shader rather than palette manipulation.

### What I'd suggest for a first prototype:

Skip the Z-buffer initially. For battle sprites specifically:
1. Store the RGBA PNG data alongside the indexed sprite (dual representation)
2. In the battle renderer, after the normal 8-bit blit, draw the RGBA version on top at the same position
3. The RGBA version "overwrites" the indexed version visually

This is basically what you did before with video over portraits. The Z-ordering is only an issue when sprites overlap (which happens in battle but is manageable with draw order).

## Summary of Recommended Next Steps

1. **Build a simple preview tool** — even a Python script with Tkinter that shows the sprite sheet, lets you approve before sending to Gemini, and shows results side by side. This eliminates the "blind iteration" problem.

2. **Standardize the Krita workflow** — accept that background removal is a manual step. Document it, make the sheet format Krita-friendly (white or checkerboard background instead of gray).

3. **Implement RGBA rendering** — this is the biggest quality win. Start with battle sprites, skip Z-buffer for v1.

4. **Generate remaining monsters** — Blood Dragon and Avenger, using the proven Gemini sprite sheet pipeline.
