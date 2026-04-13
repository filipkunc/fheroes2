"""Tests for sprite data loading and management."""

import tempfile
from pathlib import Path

import pytest
from PIL import Image

from tools.sprite_editor.models.sprite_data import (
    SpriteFrame, SpriteCollection, load_png_sprites,
    load_bmp_sprites, apply_offsets_from_base,
)


def _create_test_sprite(width: int = 32, height: int = 32) -> Image.Image:
    """Create a simple test RGBA sprite."""
    img = Image.new("RGBA", (width, height), (100, 150, 200, 255))
    return img


def _save_test_sprites(tmpdir: Path, prefix: str, count: int, placeholder_indices: set[int] | None = None):
    """Create numbered PNG sprite files in a directory."""
    placeholder_indices = placeholder_indices or set()
    for i in range(count):
        if i in placeholder_indices:
            img = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
        else:
            img = _create_test_sprite(32 + i, 40 + i)
        img.save(tmpdir / f"{prefix}_{i:03d}.png")


class TestSpriteFrame:
    def test_basic_frame(self):
        img = _create_test_sprite()
        frame = SpriteFrame(index=5, image=img)
        assert frame.index == 5
        assert frame.offset_x == 0
        assert frame.offset_y == 0
        assert frame.is_placeholder is False

    def test_placeholder_frame(self):
        img = Image.new("RGBA", (1, 1))
        frame = SpriteFrame(index=3, image=img, is_placeholder=True)
        assert frame.is_placeholder is True

    def test_to_qpixmap(self):
        img = _create_test_sprite(16, 16)
        frame = SpriteFrame(index=0, image=img)
        pixmap = frame.to_qpixmap()
        assert pixmap.width() == 16
        assert pixmap.height() == 16

    def test_placeholder_qpixmap(self):
        img = Image.new("RGBA", (1, 1))
        frame = SpriteFrame(index=0, image=img, is_placeholder=True)
        pixmap = frame.to_qpixmap()
        assert pixmap.width() == 1


class TestSpriteCollection:
    def test_frame_count(self):
        col = SpriteCollection(prefix="test")
        col.frames = [
            SpriteFrame(0, _create_test_sprite()),
            SpriteFrame(1, _create_test_sprite()),
        ]
        assert col.frame_count == 2

    def test_get_frame(self):
        col = SpriteCollection(prefix="test")
        col.frames = [
            SpriteFrame(0, _create_test_sprite()),
            SpriteFrame(5, _create_test_sprite()),
        ]
        assert col.get_frame(5) is not None
        assert col.get_frame(5).index == 5
        assert col.get_frame(3) is None

    def test_max_frame_size(self):
        col = SpriteCollection(prefix="test")
        col.frames = [
            SpriteFrame(0, _create_test_sprite(20, 30)),
            SpriteFrame(1, _create_test_sprite(50, 10)),
            SpriteFrame(2, Image.new("RGBA", (1, 1)), is_placeholder=True),
        ]
        assert col.max_frame_size() == (50, 30)


class TestLoadPngSprites:
    def test_loads_all_frames(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)
            _save_test_sprites(tmpdir, "dragon", 5)

            col = load_png_sprites(tmpdir, "dragon", 5)
            assert col.frame_count == 5
            assert col.prefix == "dragon"

    def test_detects_placeholders(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)
            _save_test_sprites(tmpdir, "dragon", 5, placeholder_indices={2, 4})

            col = load_png_sprites(tmpdir, "dragon", 5)
            assert col.get_frame(2).is_placeholder is True
            assert col.get_frame(3).is_placeholder is False
            assert col.get_frame(4).is_placeholder is True

    def test_missing_files_become_placeholders(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)
            # Only create frames 0 and 1, but ask for 4
            _save_test_sprites(tmpdir, "dragon", 2)

            col = load_png_sprites(tmpdir, "dragon", 4)
            assert col.frame_count == 4
            assert col.get_frame(0).is_placeholder is False
            assert col.get_frame(2).is_placeholder is True
            assert col.get_frame(3).is_placeholder is True

    def test_frame_dimensions(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)
            _save_test_sprites(tmpdir, "test", 3)

            col = load_png_sprites(tmpdir, "test", 3)
            f0 = col.get_frame(0)
            assert f0.image.width == 32  # 32 + 0
            assert f0.image.height == 40  # 40 + 0
            f2 = col.get_frame(2)
            assert f2.image.width == 34  # 32 + 2
            assert f2.image.height == 42  # 40 + 2


class TestApplyOffsets:
    def test_copies_offsets(self):
        base = SpriteCollection(prefix="base")
        base.frames = [
            SpriteFrame(0, _create_test_sprite(), offset_x=-32, offset_y=-60),
            SpriteFrame(1, _create_test_sprite(), offset_x=-28, offset_y=-55),
        ]

        custom = SpriteCollection(prefix="custom")
        custom.frames = [
            SpriteFrame(0, _create_test_sprite()),
            SpriteFrame(1, _create_test_sprite()),
        ]

        apply_offsets_from_base(custom, base)
        assert custom.get_frame(0).offset_x == -32
        assert custom.get_frame(0).offset_y == -60
        assert custom.get_frame(1).offset_x == -28

    def test_missing_base_frame_leaves_default(self):
        base = SpriteCollection(prefix="base")
        base.frames = [
            SpriteFrame(0, _create_test_sprite(), offset_x=-10, offset_y=-20),
        ]

        custom = SpriteCollection(prefix="custom")
        custom.frames = [
            SpriteFrame(0, _create_test_sprite()),
            SpriteFrame(1, _create_test_sprite()),  # no base frame 1
        ]

        apply_offsets_from_base(custom, base)
        assert custom.get_frame(0).offset_x == -10
        assert custom.get_frame(1).offset_x == 0  # unchanged
