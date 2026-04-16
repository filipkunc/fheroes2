"""Integration tests that verify end-to-end data flow.

These tests use real game data when available (Azure Dragon sprites, BIN files),
and skip gracefully when data is missing.
"""

import struct
from pathlib import Path

import pytest
from PySide6.QtWidgets import QApplication

from tools.sprite_editor.models.bin_parser import parse_bin, AnimType, CORRECT_FRM_LENGTH
from tools.sprite_editor.models.monster_config import load_manifest
from tools.sprite_editor.models.sprite_data import load_png_sprites
from tools.sprite_editor.models.animation import AnimationPlayer


SPRITES_DIR = Path(__file__).parent.parent.parent.parent / "files" / "data" / "sprites"
RESOURCES_DIR = Path(__file__).parent.parent / "resources"


@pytest.fixture(scope="session")
def qapp():
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    return app


class TestManifestIntegrity:
    def test_manifest_loads(self):
        configs = load_manifest()
        assert len(configs) >= 4

    def test_all_configs_have_required_fields(self):
        for name, cfg in load_manifest().items():
            assert cfg.prefix, f"{name} missing prefix"
            assert cfg.base_icn, f"{name} missing base_icn"
            assert cfg.bin_file, f"{name} missing bin_file"
            # frame_count is auto-detected from ICN when 0; only required
            # for palette_remap monsters (where it's a fixed game value)
            if cfg.palette_remap:
                assert cfg.frame_count > 0, f"{name} has zero frames"

    def test_known_monsters(self):
        configs = load_manifest()
        assert "azure_dragon" in configs
        assert "thor" in configs
        assert "blood_dragon" in configs
        assert "avenger" in configs


@pytest.mark.skipif(
    not (SPRITES_DIR / "azure_dragon_000.png").exists(),
    reason="Azure Dragon sprites not present",
)
class TestAzureDragonSprites:
    def test_loads_54_frames(self):
        col = load_png_sprites(SPRITES_DIR, "azure_dragon", 54)
        assert col.frame_count == 54

    def test_has_real_frames(self):
        col = load_png_sprites(SPRITES_DIR, "azure_dragon", 54)
        real = [f for f in col.frames if not f.is_placeholder]
        assert len(real) >= 50  # most frames should be real

    def test_frame_one_reasonable_size(self):
        """Frame 0 is a 1x1 placeholder in the dragon ICN format; frame 1 is the static pose."""
        col = load_png_sprites(SPRITES_DIR, "azure_dragon", 54)
        f1 = col.get_frame(1)
        assert not f1.is_placeholder
        assert f1.image.width > 10
        assert f1.image.height > 10

    def test_frames_are_rgba(self):
        col = load_png_sprites(SPRITES_DIR, "azure_dragon", 54)
        for f in col.frames:
            if not f.is_placeholder:
                assert f.image.mode == "RGBA"


@pytest.mark.skipif(
    not (SPRITES_DIR / "thor_000.png").exists(),
    reason="Thor sprites not present",
)
class TestThorSprites:
    def test_loads_56_frames(self):
        col = load_png_sprites(SPRITES_DIR, "thor", 56)
        assert col.frame_count == 56


class TestAnimationWithBinData:
    """Test animation player with synthetic but realistic BIN data."""

    def _dragon_bin(self) -> bytes:
        """Create BIN data mimicking a dragon's animation layout."""
        data = bytearray(CORRECT_FRM_LENGTH)
        struct.pack_into("<hh", data, 1, 44, -83)  # eye position
        struct.pack_into("<III", data, 162, 312, 0, 82)  # move/shoot/flight speed
        data[117] = 3  # 3 idle anims

        anims = {
            AnimType.STATIC: [1],
            AnimType.IDLE1: [2, 3, 4],
            AnimType.IDLE2: [5, 6, 7],
            AnimType.IDLE3: [8, 9],
            AnimType.MOVE_START: [2, 3, 4, 5, 6, 7],
            AnimType.MOVE_MAIN: [6, 5, 8, 9, 10, 11, 12],
            AnimType.MOVE_STOP: [8, 9, 10, 11, 12],
            AnimType.ATTACK1: [2, 13, 14, 15, 16],
            AnimType.ATTACK1_END: [16, 32, 2],
            AnimType.ATTACK2: [2, 13, 20, 21, 22],
            AnimType.ATTACK2_END: [22, 32, 2],
            AnimType.ATTACK3: [2, 13, 26, 27, 28],
            AnimType.ATTACK3_END: [28, 32, 2],
            AnimType.DEATH: [35, 34, 36, 37, 38, 39, 40],
            AnimType.WINCE_UP: [33, 34],
            AnimType.WINCE_END: [35],
        }
        for anim_type, frames in anims.items():
            idx = int(anim_type)
            data[243 + idx] = len(frames)
            for i, f in enumerate(frames):
                data[277 + idx * 16 + i] = f

        return bytes(data)

    def test_all_composite_anims_produce_frames(self, qapp):
        info = parse_bin(self._dragon_bin())
        player = AnimationPlayer()

        for name in info.available_animations():
            player.set_animation(info, name)
            assert player.sequence_length > 0, f"{name} produced empty sequence"

    def test_attack_composite_includes_start_and_end(self, qapp):
        info = parse_bin(self._dragon_bin())
        frames = info.get_composite_frames("Attack Front")
        # Should be ATTACK2 + ATTACK2_END
        assert frames == [2, 13, 20, 21, 22, 22, 32, 2]

    def test_move_composite(self, qapp):
        info = parse_bin(self._dragon_bin())
        frames = info.get_composite_frames("Move")
        # MOVE_START + MOVE_MAIN + MOVE_STOP
        assert len(frames) == 6 + 7 + 5

    def test_step_through_full_animation(self, qapp):
        info = parse_bin(self._dragon_bin())
        player = AnimationPlayer()
        player.set_animation(info, "Death")

        received_frames = [player.current_frame_index]
        player.frame_changed.connect(received_frames.append)

        for _ in range(player.sequence_length - 1):
            player.step_forward()

        # Should have received all death frames
        assert received_frames == [35, 34, 36, 37, 38, 39, 40]

    def test_playback_does_not_stop_unexpectedly(self, qapp):
        """Regression test: animation must not stop during normal playback.

        Previous bug: frame_list.select_frame triggered _on_frame_selected
        which called player.stop() during animated playback.
        """
        info = parse_bin(self._dragon_bin())
        player = AnimationPlayer()
        player.set_animation(info, "Death")
        player.play()

        # Simulate several timer ticks
        for _ in range(5):
            player._advance()
            assert player.is_playing is True, "Player stopped unexpectedly during playback"


class TestHexGridPositioning:
    """Verify hex cell position formula matches engine constants."""

    def test_cell_position_even_row(self):
        """Even row (row 0): no stagger."""
        from tools.sprite_editor.widgets.animation_canvas import (
            HEX_CELL_W, HEX_CELL_H, HEX_ROW_STEP, HEX_CELL_INSET,
        )
        assert HEX_CELL_W == 44
        assert HEX_CELL_H == 52
        assert HEX_ROW_STEP == 42  # 52 - 10
        assert HEX_CELL_INSET == 10  # (52 - 32) / 2

    def test_cell_position_odd_row_stagger(self):
        """Odd rows are staggered by half a cell width (22px)."""
        from tools.sprite_editor.widgets.animation_canvas import HEX_CELL_W
        stagger = HEX_CELL_W // 2
        assert stagger == 22

    def test_sprite_anchor_at_cell_bottom(self):
        """Sprite Y anchor is at cell bottom + cellYOffset (-9px)."""
        from tools.sprite_editor.widgets.animation_canvas import (
            HEX_CELL_H, HEX_CELL_Y_OFFSET,
        )
        # Engine formula: draw_y = cell_y + cell_h + sprite.offset_y + cellYOffset
        # = cell_y + 52 + offset_y - 9 = cell_y + 43 + offset_y
        assert HEX_CELL_H + HEX_CELL_Y_OFFSET == 43

    def test_hex_polygon_vertex_count(self):
        from tools.sprite_editor.widgets.animation_canvas import _hex_polygon
        poly = _hex_polygon(0, 0)
        assert poly.count() == 6
