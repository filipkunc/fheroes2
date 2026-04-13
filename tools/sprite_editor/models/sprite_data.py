"""Sprite frame loading and offset management.

Loads PNG frames from files/data/sprites/{prefix}_NNN.png and
base ICN sprites extracted via icn2img.
"""

from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image
from PySide6.QtGui import QImage, QPixmap


@dataclass
class SpriteFrame:
    index: int
    image: Image.Image  # PIL RGBA image
    offset_x: int = 0
    offset_y: int = 0
    is_placeholder: bool = False

    def to_qpixmap(self) -> QPixmap:
        """Convert PIL image to QPixmap for display."""
        if self.is_placeholder:
            return QPixmap(1, 1)
        img = self.image.convert("RGBA")
        data = img.tobytes("raw", "RGBA")
        qimg = QImage(data, img.width, img.height, img.width * 4, QImage.Format.Format_RGBA8888)
        # QImage doesn't own the data, so we must copy
        return QPixmap.fromImage(qimg.copy())


@dataclass
class SpriteCollection:
    prefix: str
    frames: list[SpriteFrame] = field(default_factory=list)
    source_dir: Path | None = None

    @property
    def frame_count(self) -> int:
        return len(self.frames)

    def get_frame(self, index: int) -> SpriteFrame | None:
        for f in self.frames:
            if f.index == index:
                return f
        return None

    def max_frame_size(self) -> tuple[int, int]:
        """Return (max_width, max_height) across all non-placeholder frames."""
        max_w, max_h = 1, 1
        for f in self.frames:
            if not f.is_placeholder:
                max_w = max(max_w, f.image.width)
                max_h = max(max_h, f.image.height)
        return max_w, max_h


def load_png_sprites(sprite_dir: Path, prefix: str, frame_count: int) -> SpriteCollection:
    """Load custom PNG sprites from files/data/sprites/{prefix}_NNN.png."""
    collection = SpriteCollection(prefix=prefix, source_dir=sprite_dir)

    for i in range(frame_count):
        png_path = sprite_dir / f"{prefix}_{i:03d}.png"
        if png_path.exists():
            img = Image.open(png_path).convert("RGBA")
            is_placeholder = img.width <= 1 or img.height <= 1
            collection.frames.append(SpriteFrame(
                index=i,
                image=img,
                is_placeholder=is_placeholder,
            ))
        else:
            # Missing frame - create placeholder
            collection.frames.append(SpriteFrame(
                index=i,
                image=Image.new("RGBA", (1, 1), (0, 0, 0, 0)),
                is_placeholder=True,
            ))

    return collection


def load_bmp_sprites(sprite_dir: Path) -> SpriteCollection:
    """Load base ICN sprites extracted by icn2img (BMP files with numeric names).

    icn2img outputs files like: 000.bmp, 001.bmp, ...
    The BMP files have the sprite on a magenta (255,0,255) or gray background.
    """
    bmp_files = sorted(sprite_dir.glob("*.bmp"), key=lambda p: int(p.stem))
    if not bmp_files:
        # Try PNG too (in case extracted differently)
        bmp_files = sorted(sprite_dir.glob("*.png"), key=lambda p: int(p.stem))

    collection = SpriteCollection(prefix="base", source_dir=sprite_dir)

    for f in bmp_files:
        idx = int(f.stem)
        img = Image.open(f).convert("RGBA")
        is_placeholder = img.width <= 1 or img.height <= 1
        collection.frames.append(SpriteFrame(
            index=idx,
            image=img,
            is_placeholder=is_placeholder,
        ))

    return collection


def apply_offsets_from_base(custom: SpriteCollection, base: SpriteCollection):
    """Copy x/y offsets from base sprite collection to custom sprites.

    This mirrors what loadCustomSpritesFromPNG does in agg_image.cpp.
    """
    for custom_frame in custom.frames:
        base_frame = base.get_frame(custom_frame.index)
        if base_frame is not None:
            custom_frame.offset_x = base_frame.offset_x
            custom_frame.offset_y = base_frame.offset_y
