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
    image: Image.Image  # PIL RGBA image (possibly hi-res)
    offset_x: int = 0
    offset_y: int = 0
    is_placeholder: bool = False
    # Base ICN dimensions — the logical size on the hex grid.
    # When set, the canvas renders the hi-res image scaled to these dimensions × zoom,
    # matching the game's RGBA render path (BlitRGBAScaled).
    base_width: int = 0
    base_height: int = 0

    @property
    def display_width(self) -> int:
        """Logical width for hex grid positioning (base ICN size if set, else image size)."""
        return self.base_width if self.base_width > 0 else self.image.width

    @property
    def display_height(self) -> int:
        """Logical height for hex grid positioning (base ICN size if set, else image size)."""
        return self.base_height if self.base_height > 0 else self.image.height

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
    # Multiplier applied to the in-game portrait (Set Count, army info, ...).
    # Stored as a metadata line in {prefix}_offsets.jsonl: {"portrait_zoom": 1.4}.
    portrait_zoom: float = 1.0

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


def _parse_offsets_txt(sprite_dir: Path) -> dict[int, tuple[int, int]]:
    """Parse offsets.txt produced by icn2img: lines like '001 [-15, -97]'."""
    offsets_file = sprite_dir / "offsets.txt"
    if not offsets_file.exists():
        return {}

    offsets = {}
    for line in offsets_file.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        # Format: "NNN [x, y]"
        parts = line.split(" ", 1)
        if len(parts) != 2:
            continue
        try:
            idx = int(parts[0])
            # Parse "[x, y]"
            coords = parts[1].strip("[] ")
            x_str, y_str = coords.split(",")
            offsets[idx] = (int(x_str.strip()), int(y_str.strip()))
        except (ValueError, IndexError):
            continue
    return offsets


def load_bmp_sprites(sprite_dir: Path) -> SpriteCollection:
    """Load base ICN sprites extracted by icn2img.

    icn2img outputs numbered PNG files (000.png, 001.png, ...) and an
    offsets.txt with per-frame x/y positioning data.
    """
    bmp_files = sorted(sprite_dir.glob("*.bmp"), key=lambda p: int(p.stem))
    if not bmp_files:
        bmp_files = sorted(sprite_dir.glob("*.png"), key=lambda p: int(p.stem))

    offsets = _parse_offsets_txt(sprite_dir)
    collection = SpriteCollection(prefix="base", source_dir=sprite_dir)

    for f in bmp_files:
        idx = int(f.stem)
        img = Image.open(f).convert("RGBA")
        is_placeholder = img.width <= 1 or img.height <= 1
        ox, oy = offsets.get(idx, (0, 0))
        collection.frames.append(SpriteFrame(
            index=idx,
            image=img,
            offset_x=ox,
            offset_y=oy,
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


def load_custom_offsets(sprite_dir: Path, prefix: str, collection: SpriteCollection):
    """Load per-frame offset overrides from {prefix}_offsets.jsonl.

    JSONL format: one JSON object per line, e.g.:
        {"frame": 45, "offset_x": -37, "offset_y": -119}

    Shared between the sprite editor and the game engine.
    """
    import json
    offsets_path = sprite_dir / f"{prefix}_offsets.jsonl"
    if not offsets_path.exists():
        return

    for line in offsets_path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            continue
        if "portrait_zoom" in entry and "frame" not in entry:
            try:
                collection.portrait_zoom = float(entry["portrait_zoom"])
            except (TypeError, ValueError):
                pass
            continue
        frame_idx = entry.get("frame")
        if frame_idx is None:
            continue
        frame = collection.get_frame(frame_idx)
        if frame is not None:
            if "offset_x" in entry:
                frame.offset_x = entry["offset_x"]
            if "offset_y" in entry:
                frame.offset_y = entry["offset_y"]
            # Override base dimensions with the stored display size.
            # This is base ICN + 2*margin — the logical game footprint
            # for frames generated with extra space for capes/wings.
            if "display_width" in entry and "display_height" in entry:
                frame.base_width = entry["display_width"]
                frame.base_height = entry["display_height"]


@dataclass
class FrameOverride:
    offset_x: int
    offset_y: int
    display_width: int = 0
    display_height: int = 0


def _load_jsonl_entries(offsets_path: Path) -> tuple[dict[int, dict], dict | None]:
    """Read frame entries and the (optional) portrait metadata line from a JSONL file."""
    import json
    frames: dict[int, dict] = {}
    meta: dict | None = None
    if not offsets_path.exists():
        return frames, meta
    for line in offsets_path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            continue
        if "frame" in entry:
            frames[entry["frame"]] = entry
        elif "portrait_zoom" in entry:
            meta = entry
    return frames, meta


def _write_jsonl_entries(offsets_path: Path, frames: dict[int, dict], meta: dict | None):
    """Emit frame entries (sorted by index) followed by the metadata line, if any."""
    import json
    lines = [json.dumps(frames[k]) for k in sorted(frames)]
    if meta is not None:
        lines.append(json.dumps(meta))
    offsets_path.write_text("\n".join(lines) + "\n")


def save_custom_offsets(sprite_dir: Path, prefix: str, frames: dict[int, FrameOverride]):
    """Save per-frame offset overrides to {prefix}_offsets.jsonl.

    Merges with existing entries and preserves any non-frame metadata
    (e.g. portrait_zoom). JSONL format: one JSON object per line.
    Read by both the sprite editor and the game engine.
    """
    offsets_path = sprite_dir / f"{prefix}_offsets.jsonl"
    existing, meta = _load_jsonl_entries(offsets_path)

    for idx, override in frames.items():
        entry = {"frame": idx, "offset_x": override.offset_x, "offset_y": override.offset_y}
        if override.display_width > 0 and override.display_height > 0:
            entry["display_width"] = override.display_width
            entry["display_height"] = override.display_height
        existing[idx] = entry

    _write_jsonl_entries(offsets_path, existing, meta)


def save_portrait_zoom(sprite_dir: Path, prefix: str, zoom: float):
    """Update only the portrait_zoom metadata line, preserving all frame entries.

    Removes the line entirely if zoom is 1.0 (the engine default), so the
    sidecar file stays clean for monsters that don't need an override.
    """
    offsets_path = sprite_dir / f"{prefix}_offsets.jsonl"
    existing, _ = _load_jsonl_entries(offsets_path)
    meta: dict | None = None
    if abs(zoom - 1.0) > 1e-6:
        meta = {"portrait_zoom": round(float(zoom), 3)}
    _write_jsonl_entries(offsets_path, existing, meta)


def set_base_dimensions(custom: SpriteCollection, base: SpriteCollection):
    """Store base ICN dimensions on custom frames for correct display scaling.

    The game's RGBA render path (BlitRGBAScaled in battle_interface.cpp) renders
    hi-res PNGs scaled to the base ICN footprint × display scale. We store the
    base dimensions so the canvas can do the same — keeping the hi-res image
    data intact for quality.
    """
    for custom_frame in custom.frames:
        if custom_frame.is_placeholder:
            continue
        base_frame = base.get_frame(custom_frame.index)
        if base_frame is None or base_frame.is_placeholder:
            continue
        custom_frame.base_width = base_frame.image.width
        custom_frame.base_height = base_frame.image.height
