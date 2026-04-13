"""Tests for BIN file parser."""

import struct
import pytest
from tools.sprite_editor.models.bin_parser import (
    parse_bin, AnimType, MonsterAnimInfo, CORRECT_FRM_LENGTH,
    ANIM_DISPLAY_NAMES, COMPOSITE_ANIMATIONS,
)


def _make_bin(overrides: dict[int, int] | None = None) -> bytes:
    """Create a valid 821-byte BIN with known values."""
    data = bytearray(CORRECT_FRM_LENGTH)
    if overrides:
        for offset, value in overrides.items():
            data[offset] = value
    return bytes(data)


def _make_bin_with_anim(anim_type: AnimType, frames: list[int]) -> bytes:
    """Create BIN with a specific animation sequence populated."""
    data = bytearray(CORRECT_FRM_LENGTH)
    idx = int(anim_type)
    data[243 + idx] = len(frames)
    for i, f in enumerate(frames):
        data[277 + idx * 16 + i] = f
    return bytes(data)


class TestBinParserBasics:
    def test_rejects_wrong_size(self):
        with pytest.raises(ValueError, match="821"):
            parse_bin(b"too short")

    def test_rejects_empty(self):
        with pytest.raises(ValueError):
            parse_bin(b"")

    def test_parses_correct_size(self):
        info = parse_bin(_make_bin())
        assert isinstance(info, MonsterAnimInfo)

    def test_eye_position(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        struct.pack_into("<hh", data, 1, 44, -83)
        info = parse_bin(bytes(data))
        assert info.eye_position == (44, -83)

    def test_eye_position_negative(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        struct.pack_into("<hh", data, 1, -10, -20)
        info = parse_bin(bytes(data))
        assert info.eye_position == (-10, -20)


class TestFrameXOffsets:
    def test_seven_move_types(self):
        info = parse_bin(_make_bin())
        assert len(info.frame_x_offsets) == 7

    def test_sixteen_frames_per_move(self):
        info = parse_bin(_make_bin())
        for offsets in info.frame_x_offsets:
            assert len(offsets) == 16

    def test_signed_offsets(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        data[5] = 0xFF  # -1 as signed int8
        data[6] = 5
        info = parse_bin(bytes(data))
        assert info.frame_x_offsets[0][0] == -1
        assert info.frame_x_offsets[0][1] == 5


class TestIdleData:
    def test_idle_count_zero(self):
        info = parse_bin(_make_bin())
        assert info.idle_count == 0
        assert info.idle_priorities == []

    def test_idle_count_capped_at_five(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        data[117] = 10  # exceeds max
        info = parse_bin(bytes(data))
        assert info.idle_count == 5

    def test_idle_priorities(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        data[117] = 2
        struct.pack_into("<f", data, 118, 0.7)
        struct.pack_into("<f", data, 122, 0.3)
        info = parse_bin(bytes(data))
        assert len(info.idle_priorities) == 2
        assert abs(info.idle_priorities[0] - 0.7) < 0.001
        assert abs(info.idle_priorities[1] - 0.3) < 0.001


class TestSpeedData:
    def test_default_speeds(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        struct.pack_into("<III", data, 162, 312, 150, 82)
        info = parse_bin(bytes(data))
        assert info.move_speed == 312
        assert info.shoot_speed == 150
        assert info.flight_speed == 82


class TestProjectiles:
    def test_projectile_offsets(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        struct.pack_into("<hh", data, 174, 10, -20)
        struct.pack_into("<hh", data, 178, 30, -40)
        struct.pack_into("<hh", data, 182, 50, -60)
        info = parse_bin(bytes(data))
        assert len(info.projectile_offsets) == 3
        assert info.projectile_offsets[0] == (10, -20)
        assert info.projectile_offsets[2] == (50, -60)

    def test_projectile_angles(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        data[186] = 3
        struct.pack_into("<f", data, 187, 0.5)
        struct.pack_into("<f", data, 191, 1.0)
        struct.pack_into("<f", data, 195, -0.5)
        info = parse_bin(bytes(data))
        assert len(info.projectile_angles) == 3
        assert abs(info.projectile_angles[2] - (-0.5)) < 0.001

    def test_projectile_count_capped(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        data[186] = 20  # exceeds max 12
        info = parse_bin(bytes(data))
        assert len(info.projectile_angles) == 12


class TestAnimationSequences:
    def test_total_anim_types(self):
        info = parse_bin(_make_bin())
        assert len(info.animation_frames) == 34  # MOVE_START through SHOOT3_END

    def test_empty_anims_by_default(self):
        info = parse_bin(_make_bin())
        for frames in info.animation_frames:
            assert frames == []

    def test_static_animation(self):
        info = parse_bin(_make_bin_with_anim(AnimType.STATIC, [1]))
        assert info.get_frames(AnimType.STATIC) == [1]

    def test_death_animation(self):
        death_frames = [35, 34, 36, 37, 38, 39, 40]
        info = parse_bin(_make_bin_with_anim(AnimType.DEATH, death_frames))
        assert info.get_frames(AnimType.DEATH) == death_frames

    def test_frame_count_capped_at_16(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        idx = int(AnimType.MOVE_MAIN)
        data[243 + idx] = 20  # exceeds max 16
        for i in range(20):
            if 277 + idx * 16 + i < CORRECT_FRM_LENGTH:
                data[277 + idx * 16 + i] = i
        info = parse_bin(bytes(data))
        assert len(info.get_frames(AnimType.MOVE_MAIN)) == 16

    def test_has_anim(self):
        info = parse_bin(_make_bin_with_anim(AnimType.ATTACK2, [2, 13, 20, 21]))
        assert info.has_anim(AnimType.ATTACK2) is True
        assert info.has_anim(AnimType.SHOOT1) is False

    def test_max_frame_index(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        # Put frame index 53 in DEATH animation
        idx = int(AnimType.DEATH)
        data[243 + idx] = 3
        data[277 + idx * 16] = 10
        data[277 + idx * 16 + 1] = 53
        data[277 + idx * 16 + 2] = 20
        info = parse_bin(bytes(data))
        assert info.max_frame_index() == 53


class TestCompositeAnimations:
    def test_composite_attack(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        # ATTACK2 = frames [20, 21, 22]
        idx = int(AnimType.ATTACK2)
        data[243 + idx] = 3
        data[277 + idx * 16] = 20
        data[277 + idx * 16 + 1] = 21
        data[277 + idx * 16 + 2] = 22
        # ATTACK2_END = frames [22, 2]
        idx2 = int(AnimType.ATTACK2_END)
        data[243 + idx2] = 2
        data[277 + idx2 * 16] = 22
        data[277 + idx2 * 16 + 1] = 2
        info = parse_bin(bytes(data))
        assert info.get_composite_frames("Attack Front") == [20, 21, 22, 22, 2]

    def test_all_frames_sorted_unique(self):
        data = bytearray(CORRECT_FRM_LENGTH)
        # Add frames to two different animations with overlapping indices
        for anim_type, frames in [(AnimType.STATIC, [1]), (AnimType.DEATH, [5, 3, 1])]:
            idx = int(anim_type)
            data[243 + idx] = len(frames)
            for i, f in enumerate(frames):
                data[277 + idx * 16 + i] = f
        info = parse_bin(bytes(data))
        all_frames = info.get_composite_frames("All Frames")
        assert all_frames == sorted(set(all_frames))

    def test_available_animations(self):
        info = parse_bin(_make_bin_with_anim(AnimType.DEATH, [35, 36, 37]))
        avail = info.available_animations()
        assert "All Frames" in avail
        assert "Death" in avail
        assert "Attack Top" not in avail


class TestAnimDisplayNames:
    def test_all_types_have_names(self):
        for anim_type in AnimType:
            assert anim_type in ANIM_DISPLAY_NAMES, f"Missing display name for {anim_type.name}"

    def test_name_count_matches(self):
        assert len(ANIM_DISPLAY_NAMES) == len(AnimType)
