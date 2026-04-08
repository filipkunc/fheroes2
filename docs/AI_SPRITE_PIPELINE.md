# AI Sprite Generation Pipeline

This document describes the pipeline for generating custom monster battle sprites using AI image-to-image transformation with the Gemini API.

## Overview

Custom monsters (Azure Dragon, Blood Dragon, Thor, Avenger) use existing HoMM2 sprites as a base and transform them via the Gemini API to create new art while preserving animation frames, offsets, and pixel art style.

The pipeline:
1. Extract base monster sprites from the game's AGG archive
2. Pack frames into sprite sheets (6 per batch, 4x NEAREST upscale) for consistency
3. Transform via Gemini API with text prompt + reference image
4. Slice results back, apply background removal from Gemini output
5. Load in-game via PNG sprite loading path — engine handles resize and offset copying

## Prerequisites

- Python 3.12+ with `uv` package manager
- ComfyUI venv at `~/Projects/comfyui/.venv/` (shared for scripts)
- Gemini API key with billing enabled (stored at `~/.config/gemini/api_key`)
- Build tools: `extractor`, `icn2img` (built from fheroes2)
- Game data: `HEROES2.AGG`

### Install dependencies

```bash
cd ~/Projects/comfyui
uv pip install -U google-genai Pillow numpy
```

## Step 1: Extract base sprites

Extract the AGG contents and the base monster ICN sprites:

```bash
# Extract AGG
mkdir -p /tmp/homm2_sprites
./build/extractor /tmp/homm2_sprites "path/to/HEROES2.AGG"

# Extract Green Dragon (base for Azure Dragon)
mkdir -p /tmp/dragon_sprites/draggree
./build/icn2img /tmp/dragon_sprites/draggree \
  /tmp/homm2_sprites/HEROES2/kb.pal \
  /tmp/homm2_sprites/HEROES2/draggree.icn
```

Base monsters for each custom monster:

| Custom Monster | Base Monster | ICN File | Frame Count |
|---------------|-------------|----------|-------------|
| Azure Dragon  | Green Dragon | `draggree.icn` | 54 |
| Blood Dragon  | Bone Dragon  | `dragbone.icn` | 54 |
| Thor          | Titan        | `titanblu.icn` | TBD |
| Avenger       | Crusader     | `paladin2.icn` | TBD |

## Step 2: Generate sprites with Gemini

### Recommended: Sprite sheet approach (`reskin_spritesheet.py`)

Batches multiple frames into a single sprite sheet for visual consistency and fewer API calls:

```bash
cd ~/Projects/comfyui

.venv/bin/python scripts/reskin_spritesheet.py \
  --input /tmp/dragon_sprites/draggree/draggree/ \
  --prompt "Transform this sprite sheet. Change green to azure blue. Replace fire with lightning." \
  --system "You are a specialized pixel art editor. Gold #DAA520, silver #C0C0C0..." \
  --reference ./reskinned/some_good_output_sheet.png \
  --output ./reskinned/gemini_azure/ \
  --model gemini-3-pro-image-preview \
  --batch-size 6 --upscale 4 --raw
```

### Key parameters

| Parameter | Description |
|-----------|-------------|
| `--prompt` | What to change in each frame |
| `--system` | System instruction for consistency (hex colors, style rules) |
| `--reference` | Path to a previously generated good sheet — ensures color consistency |
| `--model` | `gemini-3-pro-image-preview` (fewer rate limits) or `gemini-3.1-flash-image-preview` |
| `--batch-size` | Frames per sprite sheet (default: 6, sweet spot for quality) |
| `--upscale` | Power-of-2 upscale factor (default: 8, use 4 for best quality/size ratio) |
| `--delay` | Seconds between API calls (default: 10.0) |
| `--raw` | Save raw Gemini output and input/output sheets for debugging |
| `--start`/`--end` | Process a subset of frames |

### Alternative: Single-frame approach (`reskin_gemini.py`)

For simpler transformations (e.g., Azure Dragon recolor):

```bash
.venv/bin/python scripts/reskin_gemini.py \
  --input /tmp/dragon_sprites/draggree/draggree/ \
  --prompt "Transform this pixel art sprite..." \
  --system "You are a specialized pixel art editor..." \
  --output ./reskinned/gemini_azure/ \
  --model gemini-3.1-flash-image-preview --raw
```

### Fixing inconsistent batches

If some batches look different (wrong colors, flat style), regenerate just those frames using a good output sheet as `--reference`:

```bash
.venv/bin/python scripts/reskin_spritesheet.py \
  --reference ./reskinned/good_batch/sheets/output_batch_000.png \
  --start 7 --end 13 ...
```

### Post-processing

The sprite sheet script automatically:
1. Packs frames into a grid with 4x NEAREST upscale (pixel-perfect)
2. Sends the sheet + reference image to Gemini
3. Slices results back at original frame boundaries
4. Removes background using corner pixel detection
5. Saves as RGBA PNGs

## Step 3: Deploy to game

Copy the generated frames to the sprites directory:

```bash
for i in $(seq 0 53); do
  src="./reskinned/gemini_azure/$(printf '%03d' $i).png"
  dst="../fheroes2/files/data/sprites/azure_dragon_$(printf '%03d' $i).png"
  [ -f "$src" ] && cp "$src" "$dst"
done
```

The sprites directory is `files/data/sprites/` and the naming convention is `<prefix>_NNN.png` (e.g. `azure_dragon_000.png` through `azure_dragon_053.png`).

## Step 4: Build and run

The game must be built with PNG support:

```bash
cd build
cmake .. -DENABLE_IMAGE=ON
cmake --build . -j$(nproc)
./fheroes2
```

`ENABLE_IMAGE=ON` requires `SDL2_image-devel` and `libpng-devel` packages.

## How it works in the engine

### PNG sprite loading (`agg_image.cpp`)

The function `loadCustomSpritesFromPNG()` in `agg_image.cpp`:

1. Looks for PNG files in `files/data/sprites/<prefix>_NNN.png`
2. Loads each frame via `fheroes2::Load()` which handles RGBA-to-palette conversion
3. If loaded sprite size differs from base ICN (high-res), resizes using `fheroes2::Resize()`
4. Copies x/y position offsets from the base ICN sprite
5. For same-size sprites: copies base transform layer (preserves shadows)
6. For resized sprites: keeps transform layer from PNG alpha channel
7. Placeholder frames (1x1) fall back to the base ICN sprite

### Loading priority

For each custom monster, sprites are loaded in this order:
1. **PNG files** from `files/data/sprites/` (AI-generated)
2. **H2D entries** from `resurrection.h2d` (hand-crafted)
3. **Palette remap** from the base monster ICN (fallback)

### Important: frame counts

The sprite count must cover ALL animation frames including ranged attacks:

| Monster | Base ICN | Frame Count | Notes |
|---------|----------|-------------|-------|
| Azure Dragon | DRAGGREE | 54 | |
| Blood Dragon | DRAGBONE | TBD | |
| Thor | TITANBLA | 56 | Ranged frames go up to 53, plus extras |
| Avenger | PALADIN2 | TBD | |

**Always check the AnimationReference `_ranged` array** in the debugger to find the highest frame index used. Setting the count too low causes units to vanish during shooting animations.

## Prompt engineering tips

### System instruction

Use a system instruction with specific hex color codes to ensure consistency across all 54 frames:

```
You are a specialized pixel art editor. Your job is to transform [base] sprites
into [target] with [effects]. You must maintain the exact pixel-perfect silhouette
and pose of the input. Use the hex code #XXXXXX for the primary color and #YYYYYY
for the effect color to ensure color consistency across multiple frames.
```

### Frame prompt

Be specific about what to change and what to preserve:

```
Transform this pixel art sprite.
Change the base color from [original] to [target].
Replace [original effect] with [new effect].
Maintain the 16-bit aesthetic and original silhouette.
Do not change the background color or the pose of the character.
```

### Testing

Always test with 2-3 frames first (`--start 1 --end 4`) before running all frames to verify the prompt produces the desired results.

## File locations

| Path | Description |
|------|-------------|
| `~/Projects/comfyui/scripts/reskin_gemini.py` | Main generation script |
| `~/Projects/comfyui/reskinned/` | Generated sprite output directories |
| `~/.config/gemini/api_key` | Gemini API key (not in repo) |
| `files/data/sprites/` | In-game sprite directory |
| `src/fheroes2/agg/agg_image.cpp` | Engine PNG loading code |

## Cost

Gemini Flash image generation is very cheap. Processing 54 frames costs approximately $0.01-0.05 USD with pay-as-you-go billing.
