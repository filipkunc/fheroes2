"""Python ICN sprite parser — reads ICN binary format and produces RGBA sprites.

Ported from src/engine/image_tool.cpp:decodeICNSprite (lines 539-697).
Uses the transform layer for proper per-pixel transparency instead of
naive flood fill from corners.

ICN file layout:
    uint16 LE  sprite_count
    uint32 LE  total_data_size
    N × 13 bytes  ICNHeaders
    variable      compressed sprite data

ICNHeader (13 bytes):
    int16 LE   offsetX
    int16 LE   offsetY
    uint16 LE  width
    uint16 LE  height
    uint8      animationFrames (bit 5 = monochromatic)
    uint32 LE  offsetData (from start of data section)

Transform layer values:
    0     = opaque pixel
    1     = fully transparent
    2-5   = shadow (darkening levels, 2=strongest)
    6-10  = lightening effects
"""

import struct
from dataclasses import dataclass

import numpy as np
from PIL import Image

from ..models.sprite_data import SpriteFrame


@dataclass
class ICNHeader:
    offset_x: int
    offset_y: int
    width: int
    height: int
    animation_frames: int
    offset_data: int

    @property
    def is_monochromatic(self) -> bool:
        return bool(self.animation_frames & 0x20)


ICN_HEADER_SIZE = 13  # 2+2+2+2+1+4 bytes


def _parse_icn_header(data: bytes, offset: int) -> ICNHeader:
    ox, oy, w, h, anim, odata = struct.unpack_from("<hhHHBI", data, offset)
    return ICNHeader(ox, oy, w, h, anim, odata)


def _decode_sprite(data: bytes, header: ICNHeader) -> tuple[np.ndarray, np.ndarray]:
    """Decode RLE-compressed ICN sprite data.

    Returns (pixels, transform) where:
        pixels: uint8 array (h, w) — palette indices
        transform: uint8 array (h, w) — 0=opaque, 1=transparent, 2-5=shadow
    """
    w, h = header.width, header.height
    pixels = np.zeros((h, w), dtype=np.uint8)
    transform = np.ones((h, w), dtype=np.uint8)  # default: transparent

    pos_x = 0
    row = 0
    i = 0

    if header.is_monochromatic:
        while i < len(data) and row < h:
            b = data[i]
            if b == 0x00:
                # End of row
                row += 1
                pos_x = 0
                i += 1
            elif b < 0x80:
                # N black pixels (palette index 0, transform=0)
                count = b
                end = min(pos_x + count, w)
                transform[row, pos_x:end] = 0
                pos_x = end
                i += 1
            elif b == 0x80:
                # End of image
                break
            else:
                # Skip (N - 0x80) transparent pixels
                pos_x += b - 0x80
                i += 1
    else:
        while i < len(data) and row < h:
            b = data[i]
            if b == 0x00:
                # End of row
                row += 1
                pos_x = 0
                i += 1
            elif b < 0x80:
                # N literal opaque pixels
                count = b
                i += 1
                end = min(pos_x + count, w)
                actual = end - pos_x
                if i + actual > len(data):
                    break
                pixels[row, pos_x:end] = np.frombuffer(data[i:i + actual], dtype=np.uint8)
                transform[row, pos_x:end] = 0
                i += actual
                pos_x = end
            elif b == 0x80:
                # End of image
                break
            elif b < 0xC0:
                # Skip (N - 0x80) transparent pixels
                pos_x += b - 0x80
                i += 1
            elif b == 0xC0:
                # Transform layer block
                i += 1
                if i >= len(data):
                    break
                transform_value = data[i]
                count_value = transform_value & 0x03
                if count_value != 0:
                    pixel_count = count_value
                else:
                    i += 1
                    if i >= len(data):
                        break
                    pixel_count = data[i]

                if transform_value & 0x40:
                    transform_type = ((transform_value & 0x3C) >> 2) + 2
                    if transform_type < 16:
                        end = min(pos_x + pixel_count, w)
                        transform[row, pos_x:end] = transform_type

                pos_x += pixel_count
                i += 1
            else:
                # 0xC1-0xFF: N pixels of same color
                if b == 0xC1:
                    i += 1
                    if i >= len(data):
                        break
                    pixel_count = data[i]
                else:
                    pixel_count = b - 0xC0
                i += 1
                if i >= len(data):
                    break
                color = data[i]
                end = min(pos_x + pixel_count, w)
                pixels[row, pos_x:end] = color
                transform[row, pos_x:end] = 0
                pos_x = end
                i += 1

    return pixels, transform


def _indexed_to_rgba(pixels: np.ndarray, transform: np.ndarray,
                     palette: np.ndarray) -> np.ndarray:
    """Convert indexed pixels + transform layer to RGBA.

    Transform=0 → opaque (palette color, alpha=255)
    Transform≥1 → transparent (alpha=0)
    """
    h, w = pixels.shape
    rgba = np.zeros((h, w, 4), dtype=np.uint8)

    opaque = transform == 0
    indices = pixels[opaque]
    rgba[opaque, 0] = palette[indices, 0]
    rgba[opaque, 1] = palette[indices, 1]
    rgba[opaque, 2] = palette[indices, 2]
    rgba[opaque, 3] = 255

    return rgba


def parse_icn(icn_data: bytes, palette: np.ndarray) -> list[SpriteFrame]:
    """Parse a complete ICN file into a list of SpriteFrames with proper RGBA alpha.

    Args:
        icn_data: Raw ICN file bytes
        palette: 256×3 uint8 array (8-bit RGB values from load_palette)

    Returns:
        List of SpriteFrame with RGBA images and correct offsets
    """
    if len(icn_data) < 6:
        return []

    sprite_count, total_size = struct.unpack_from("<HI", icn_data, 0)
    headers_start = 6
    # offsetData is relative to byte 6 (beginPos in icn2img.cpp:123),
    # NOT relative to end of headers. This matches:
    #   inputStream.seek( beginPos + header.offsetData )
    begin_pos = 6

    frames = []
    for idx in range(sprite_count):
        header = _parse_icn_header(icn_data, headers_start + idx * ICN_HEADER_SIZE)

        if header.width == 0 or header.height == 0:
            frames.append(SpriteFrame(
                index=idx,
                image=Image.new("RGBA", (1, 1), (0, 0, 0, 0)),
                offset_x=header.offset_x,
                offset_y=header.offset_y,
                is_placeholder=True,
            ))
            continue

        # Sprite data at beginPos + header.offset_data
        sprite_data_offset = begin_pos + header.offset_data

        # Data size: next sprite's offset minus this one, or remaining file
        if idx + 1 < sprite_count:
            next_header = _parse_icn_header(icn_data, headers_start + (idx + 1) * ICN_HEADER_SIZE)
            data_size = next_header.offset_data - header.offset_data
        else:
            data_size = total_size - header.offset_data
        sprite_data_end = sprite_data_offset + data_size

        sprite_bytes = icn_data[sprite_data_offset:sprite_data_end]
        pixels, transform = _decode_sprite(sprite_bytes, header)
        rgba = _indexed_to_rgba(pixels, transform, palette)
        img = Image.fromarray(rgba, mode="RGBA")

        is_placeholder = img.width <= 1 or img.height <= 1
        frames.append(SpriteFrame(
            index=idx,
            image=img,
            offset_x=header.offset_x,
            offset_y=header.offset_y,
            is_placeholder=is_placeholder,
        ))

    return frames
