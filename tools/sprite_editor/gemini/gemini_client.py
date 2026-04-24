"""Gemini API wrapper for sprite sheet generation.

Reuses the sheet building/slicing logic from reskin_spritesheet.py.
"""

import colorsys
import io
import math
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

from google import genai
from google.genai import types

from ..models.sprite_data import SpriteFrame


GRID_GAP = 2  # pixels between frames (at native resolution)
DEFAULT_BG_COLOR = (255, 0, 255)  # Magenta — distinct from any game sprite color
DEFAULT_UPSCALE = 4
DEFAULT_MARGIN = 4  # pixels of padding around each sprite (at native resolution)


def load_api_key() -> str | None:
    """Load Gemini API key from standard location."""
    key_file = Path("~/.config/gemini/api_key").expanduser()
    if key_file.exists():
        return key_file.read_text().strip()
    return None


def _replace_bg_with_color(img: Image.Image, bg_color: tuple[int, int, int]) -> Image.Image:
    """Composite sprite onto bg_color background using alpha channel.

    Base ICN sprites now have proper alpha from the ICN parser (transform layer),
    and custom PNGs have alpha from the PNG file. In both cases, transparent
    pixels (alpha=0) become bg_color, and semi-transparent pixels are blended.
    """
    pixels = np.array(img.convert("RGBA"))
    alpha = pixels[:, :, 3:4].astype(np.float32) / 255.0
    bg = np.array(bg_color, dtype=np.float32).reshape(1, 1, 3)
    rgb = pixels[:, :, :3].astype(np.float32)
    composited = (rgb * alpha + bg * (1.0 - alpha)).clip(0, 255).astype(np.uint8)
    result = np.zeros((pixels.shape[0], pixels.shape[1], 4), dtype=np.uint8)
    result[:, :, :3] = composited
    result[:, :, 3] = 255
    return Image.fromarray(result)


def build_sheet(frames: list[SpriteFrame], upscale: int = DEFAULT_UPSCALE,
                margin: int = DEFAULT_MARGIN,
                bg_color: tuple[int, int, int] = DEFAULT_BG_COLOR,
                cols: int | None = None) -> tuple[Image.Image, list[tuple[int, int, int, int] | None]]:
    """Pack sprite frames into a grid sheet, upscaled for Gemini.

    Each frame gets `margin` pixels of padding (at native resolution) on all sides,
    giving Gemini room to add details (capes, wings, etc.) beyond the original silhouette.

    Returns (sheet_image, layout) where layout[i] = (x, y, w, h) is the FULL CELL
    bounds at upscaled resolution (including margin), or None for placeholder frames.
    """
    real_frames = [(i, f) for i, f in enumerate(frames) if not f.is_placeholder]
    if not real_frames:
        return Image.new("RGB", (1, 1), bg_color), [None] * len(frames)

    # Cell size based on base ICN dimensions (the logical game size).
    # For frames with base_width set, use that. Otherwise use image size.
    # This ensures consistent cell sizes and upscaling across all frames.
    max_w = max((f.base_width if f.base_width > 0 else f.image.width) for _, f in real_frames)
    max_h = max((f.base_height if f.base_height > 0 else f.image.height) for _, f in real_frames)

    n = len(real_frames)
    if cols is None:
        cols = max(1, int(math.ceil(math.sqrt(n))))
    rows = max(1, (n + cols - 1) // cols)

    gap = GRID_GAP * upscale
    margin_px = margin * upscale
    cell_w = (max_w + margin * 2) * upscale + gap
    cell_h = (max_h + margin * 2) * upscale + gap
    sheet_w = cols * cell_w - gap
    sheet_h = rows * cell_h - gap

    sheet = Image.new("RGB", (sheet_w, sheet_h), bg_color)
    layout: list[tuple[int, int, int, int] | None] = [None] * len(frames)

    real_idx = 0
    for list_idx, frame in real_frames:
        col = real_idx % cols
        row = real_idx // cols
        cell_x = col * cell_w
        cell_y = row * cell_h
        # Use base ICN dimensions for consistent grid cell sizing.
        # The image (possibly hi-res) gets scaled to base size first,
        # then upscaled with NEAREST for pixel-perfect Gemini input.
        base_w = frame.base_width if frame.base_width > 0 else frame.image.width
        base_h = frame.base_height if frame.base_height > 0 else frame.image.height

        display_img = frame.image.convert("RGBA")
        if display_img.width != base_w or display_img.height != base_h:
            display_img = display_img.resize((base_w, base_h), Image.Resampling.LANCZOS)

        display_img = _replace_bg_with_color(display_img, bg_color)

        upscaled = display_img.convert("RGB").resize(
            (base_w * upscale, base_h * upscale), Image.Resampling.NEAREST,
        )

        # Paste sprite centered in the cell (margin on all sides)
        paste_x = cell_x + margin_px
        paste_y = cell_y + margin_px
        sheet.paste(upscaled, (paste_x, paste_y))

        # Layout stores the full cell bounds (including margin), and the base dimensions
        layout[list_idx] = (cell_x, cell_y, cell_w - gap, cell_h - gap)
        real_idx += 1

    return sheet, layout


@dataclass
class SlicedFrame:
    """A sliced frame from a Gemini output sheet."""
    image: Image.Image
    frame_index: int
    offset_x: int  # adjusted offset accounting for margin
    offset_y: int
    display_width: int = 0   # logical game size (base + 2*margin)
    display_height: int = 0  # 0 = use image size


def _compute_bg_mask(cell_pixels: np.ndarray, bg_color: tuple[int, int, int]) -> np.ndarray:
    """Return a boolean mask of pixels that match bg_color (background + blended fringe).

    Uses hue-window detection when bg is saturated (catches Gemini's blended edges
    that drift towards the bg hue), falls back to RGB distance for low-saturation
    backgrounds (grey/white/black) where hue is unstable.
    """
    r, g, b = bg_color
    bg_h, bg_s, _bg_v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)

    if bg_s < 0.15:
        # Low-saturation bg: hue is undefined, use RGB distance instead.
        rgb = cell_pixels[:, :, :3].astype(np.int16)
        target = np.array(bg_color, dtype=np.int16).reshape(1, 1, 3)
        return np.max(np.abs(rgb - target), axis=2) < 40

    rgb_img = Image.fromarray(cell_pixels[:, :, :3], "RGB")
    hsv = np.array(rgb_img.convert("HSV")).transpose(2, 0, 1).astype(np.float32)
    hue_deg = hsv[0] / 255.0 * 360.0
    sat = hsv[1] / 255.0

    target_deg = bg_h * 360.0
    diff = np.abs(hue_deg - target_deg)
    diff = np.minimum(diff, 360.0 - diff)  # circular distance
    return (diff < 30.0) & (sat > 0.2)


def slice_sheet(output_sheet: Image.Image, layout: list[tuple[int, int, int, int] | None],
                original_frames: list[SpriteFrame], upscale: int = DEFAULT_UPSCALE,
                margin: int = DEFAULT_MARGIN,
                use_silhouette_mask: bool = False,
                bg_color: tuple[int, int, int] = DEFAULT_BG_COLOR) -> list[SlicedFrame]:
    """Slice a Gemini output sheet back into individual RGBA frames.

    Crops the full cell (including margin) so new content like capes and wings
    beyond the original silhouette is preserved. Adjusts offsets to account for
    the margin so the sprite renders at the correct position on the hex grid.

    Returns list of SlicedFrame with adjusted offsets.
    """
    results = []
    for i, frame in enumerate(original_frames):
        if frame.is_placeholder or layout[i] is None:
            results.append(SlicedFrame(
                image=Image.new("RGBA", (1, 1), (0, 0, 0, 0)),
                frame_index=frame.index,
                offset_x=frame.offset_x,
                offset_y=frame.offset_y,
            ))
            continue

        x, y, w, h = layout[i]

        # Crop the full cell (including margin) to preserve new content
        cell = output_sheet.crop((x, y, x + w, y + h)).convert("RGBA")

        # Remove background around the user-chosen bg_color (catches solid bg
        # and Gemini's blended fringe pixels).
        cell_pixels = np.array(cell)
        cell_pixels[_compute_bg_mask(cell_pixels, bg_color), 3] = 0
        result = Image.fromarray(cell_pixels)

        # Adjust offsets: the margin adds pixels on all sides, shifting the
        # sprite's anchor point. Subtract margin from offsets to compensate.
        # Offsets are in game pixels (not upscaled).
        adjusted_x = frame.offset_x - margin
        adjusted_y = frame.offset_y - margin

        # Logical display size = cell size / upscale (same for all frames in batch).
        # This matches the cell aspect ratio so no distortion occurs.
        cell_display_w = w // upscale
        cell_display_h = h // upscale

        results.append(SlicedFrame(
            image=result,
            frame_index=frame.index,
            offset_x=adjusted_x,
            offset_y=adjusted_y,
            display_width=cell_display_w,
            display_height=cell_display_h,
        ))

    return results


class GeminiClient:
    """Wraps google-genai for sprite sheet generation."""

    def __init__(self, api_key: str | None = None, model: str = "gemini-2.0-flash-preview-image-generation"):
        if api_key is None:
            api_key = load_api_key()
        if not api_key:
            raise ValueError("No Gemini API key found. Set ~/.config/gemini/api_key")
        self._client = genai.Client(api_key=api_key)
        self.model = model

    def send_sheet(self, sheet: Image.Image, prompt: str,
                   system_instruction: str | None = None,
                   reference: Image.Image | None = None,
                   max_retries: int = 3) -> Image.Image | None:
        """Send a sprite sheet to Gemini and return the transformed sheet.

        Returns the output image or None on failure.
        """
        size_prompt = (
            f"{prompt} "
            f"Keep the exact same layout, spacing, and frame positions. "
            f"The output must be exactly {sheet.width}x{sheet.height} pixels."
        )

        # Loosen safety filters — fantasy monster sprites (skeletons, demons,
        # dragons) trip the default thresholds too aggressively.
        safety_settings = [
            types.SafetySetting(category=c, threshold="BLOCK_NONE")
            for c in (
                "HARM_CATEGORY_HARASSMENT",
                "HARM_CATEGORY_HATE_SPEECH",
                "HARM_CATEGORY_SEXUALLY_EXPLICIT",
                "HARM_CATEGORY_DANGEROUS_CONTENT",
                "HARM_CATEGORY_CIVIC_INTEGRITY",
            )
        ]

        config = types.GenerateContentConfig(
            response_modalities=["IMAGE"],
            safety_settings=safety_settings,
        )
        if system_instruction:
            config.system_instruction = system_instruction

        if reference:
            contents = [
                "Reference image showing target style:",
                reference,
                size_prompt,
                sheet,
            ]
        else:
            contents = [size_prompt, sheet]

        for attempt in range(max_retries):
            try:
                response = self._client.models.generate_content(
                    model=self.model,
                    contents=contents,
                    config=config,
                )

                if not response.candidates:
                    raise RuntimeError("Gemini returned no candidates — the request may have been blocked by safety filters")

                parts = response.candidates[0].content.parts if response.candidates[0].content else None
                if not parts:
                    raise RuntimeError(f"Gemini returned empty response. Finish reason: {response.candidates[0].finish_reason}")

                for part in parts:
                    if part.inline_data:
                        raw_bytes = part.inline_data.data
                        result = Image.open(io.BytesIO(raw_bytes)).convert("RGB")

                        # Resize if Gemini changed dimensions
                        if result.size != sheet.size:
                            result = result.resize(sheet.size, Image.Resampling.BOX)

                        return result

                # Response had parts but none with image data
                text_parts = [p.text for p in parts if hasattr(p, 'text') and p.text]
                raise RuntimeError(f"Gemini returned no image. Response text: {'; '.join(text_parts) if text_parts else 'none'}")

            except Exception as e:
                if "429" in str(e) and attempt < max_retries - 1:
                    wait = 10 * (attempt + 1) * 2
                    time.sleep(wait)
                else:
                    raise

        return None
