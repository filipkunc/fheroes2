"""Palette remap tables for custom monsters.

Ported from src/engine/pal.cpp. Each table is a 256-entry list mapping
old palette index → new palette index. Combined with KB.PAL (256 RGB triplets),
this lets us recolor base sprites to show what the palette-remapped monster looks like.
"""

import numpy as np
from pathlib import Path
from PIL import Image


# fmt: off
BLOOD_DRAGON_TABLE = [
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
    182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 192, 193, 194, 195, 196, 197, 197, 197, 197, 197, 197, 197, 197,
    182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 197,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 197, 197,
    182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 197, 197,
    175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
    221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243,
    244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
]

AVENGER_TABLE = [
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    108, 109, 110, 111, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 195, 196, 197, 197, 197, 36,
    37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107,
    198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 195, 196, 197, 197, 197, 197,
    131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174,
    175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
    221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243,
    244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
]
# fmt: on

REMAP_TABLES = {
    "blood_dragon": BLOOD_DRAGON_TABLE,
    "avenger": AVENGER_TABLE,
}


def load_palette(pal_path: Path) -> np.ndarray:
    """Load KB.PAL — 768 bytes = 256 RGB triplets. Returns (256, 3) uint8 array.

    The raw PAL file uses 6-bit VGA values (0-63). We scale to 8-bit (0-252)
    by multiplying by 4, matching what icn2img does when converting sprites.
    """
    data = pal_path.read_bytes()
    if len(data) != 768:
        raise ValueError(f"PAL file must be 768 bytes, got {len(data)}")
    pal = np.frombuffer(data, dtype=np.uint8).reshape(256, 3).copy()
    # Scale 6-bit VGA (0-63) to 8-bit (0-252)
    if pal.max() <= 63:
        pal = (pal.astype(np.uint16) * 4).clip(0, 255).astype(np.uint8)
    return pal


def build_rgb_remap(palette: np.ndarray, remap_table: list[int]) -> dict[tuple, tuple]:
    """Build an RGB→RGB mapping from palette + index remap table.

    Returns dict mapping (r,g,b) → (r,g,b) for every palette entry that changes.
    """
    mapping = {}
    for old_idx in range(256):
        new_idx = remap_table[old_idx]
        if old_idx != new_idx:
            old_rgb = tuple(palette[old_idx])
            new_rgb = tuple(palette[new_idx])
            mapping[old_rgb] = new_rgb
    return mapping


def apply_palette_remap(image: Image.Image, palette: np.ndarray, remap_table: list[int]) -> Image.Image:
    """Apply a palette remap to an RGBA image.

    For each pixel, finds the closest palette index, remaps it, and converts
    back to RGB. Preserves alpha channel.
    """
    img_array = np.array(image.convert("RGBA"))
    rgb = img_array[:, :, :3]
    alpha = img_array[:, :, 3]

    # Build a lookup: for each palette color, compute the remapped color
    # Then for each pixel, find nearest palette entry and replace
    remap_array = np.array(remap_table, dtype=np.uint8)
    remapped_palette = palette[remap_array]  # (256, 3) - remapped RGB values

    # Find nearest palette index for each pixel using vectorized distance
    # Reshape for broadcasting: pixels (H, W, 1, 3) vs palette (1, 1, 256, 3)
    h, w = rgb.shape[:2]
    pixels = rgb.reshape(h, w, 1, 3).astype(np.int16)
    pal_expanded = palette.reshape(1, 1, 256, 3).astype(np.int16)

    # Sum of squared differences
    diffs = np.sum((pixels - pal_expanded) ** 2, axis=3)  # (H, W, 256)
    nearest_idx = np.argmin(diffs, axis=2)  # (H, W)

    # Look up remapped colors
    remapped_idx = remap_array[nearest_idx]  # (H, W)
    new_rgb = palette[remapped_idx]  # (H, W, 3)

    # Compose result
    result = np.zeros_like(img_array)
    result[:, :, :3] = new_rgb
    result[:, :, 3] = alpha

    return Image.fromarray(result)


def apply_palette_remap_fast(image: Image.Image, palette: np.ndarray, remap_table: list[int]) -> Image.Image:
    """Fast palette remap using exact color matching only.

    Much faster than apply_palette_remap — only remaps pixels that exactly match
    a palette color. Pixels that don't match any palette color are left unchanged.
    """
    img_array = np.array(image.convert("RGBA"))
    rgb = img_array[:, :, :3].copy()

    remap_array = np.array(remap_table, dtype=np.uint8)

    # Build exact-match lookup from old RGB to new RGB
    for old_idx in range(256):
        new_idx = remap_array[old_idx]
        if old_idx == new_idx:
            continue
        old_color = palette[old_idx]
        new_color = palette[new_idx]
        # Find all pixels matching this exact color
        mask = np.all(rgb == old_color, axis=2)
        rgb[mask] = new_color

    result = img_array.copy()
    result[:, :, :3] = rgb
    return Image.fromarray(result)
