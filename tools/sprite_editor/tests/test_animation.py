"""Tests for animation player — verifies playback, looping, and signal behavior."""

import struct
import pytest
from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QApplication

from tools.sprite_editor.models.animation import AnimationPlayer
from tools.sprite_editor.models.bin_parser import parse_bin, AnimType, MonsterAnimInfo, CORRECT_FRM_LENGTH


@pytest.fixture(scope="session")
def qapp():
    """Create a QApplication for the test session (required for QTimer/signals)."""
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    return app


def _make_anim_info(*anims: tuple[AnimType, list[int]]) -> MonsterAnimInfo:
    """Create a MonsterAnimInfo with specific animation sequences."""
    data = bytearray(CORRECT_FRM_LENGTH)
    # Set reasonable speeds
    struct.pack_into("<III", data, 162, 480, 200, 100)  # move, shoot, flight
    for anim_type, frames in anims:
        idx = int(anim_type)
        data[243 + idx] = len(frames)
        for i, f in enumerate(frames):
            data[277 + idx * 16 + i] = f
    return parse_bin(bytes(data))


class TestAnimationPlayerSetup:
    def test_initial_state(self, qapp):
        player = AnimationPlayer()
        assert player.is_playing is False
        assert player.current_frame_index == 0
        assert player.current_position == 0
        assert player.sequence_length == 0

    def test_set_raw_sequence(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([5, 10, 15, 20])
        assert player.sequence_length == 4
        assert player.current_frame_index == 5
        assert player.current_position == 0

    def test_set_empty_sequence(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([])
        assert player.sequence_length == 0
        assert player.current_frame_index == 0

    def test_set_animation_from_bin(self, qapp):
        info = _make_anim_info(
            (AnimType.DEATH, [35, 36, 37, 38, 39]),
        )
        player = AnimationPlayer()
        player.set_animation(info, "Death")
        assert player.sequence_length == 5
        assert player.current_frame_index == 35


class TestAnimationPlayerStepping:
    def test_step_forward(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        player.step_forward()
        assert player.current_frame_index == 20
        assert player.current_position == 1

    def test_step_forward_wraps(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        player.step_forward()  # → 20
        player.step_forward()  # → 30
        player.step_forward()  # → wraps to 10
        assert player.current_frame_index == 10
        assert player.current_position == 0

    def test_step_backward(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        player.step_forward()  # → 20
        player.step_backward()  # → 10
        assert player.current_frame_index == 10

    def test_step_backward_wraps(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        player.step_backward()  # wraps to 30
        assert player.current_frame_index == 30
        assert player.current_position == 2

    def test_seek(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30, 40, 50])

        player.seek(3)
        assert player.current_frame_index == 40
        assert player.current_position == 3

    def test_seek_clamps(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        player.seek(100)
        assert player.current_position == 2

        player.seek(-5)
        assert player.current_position == 0

    def test_step_empty_sequence_no_crash(self, qapp):
        player = AnimationPlayer()
        player.step_forward()  # should not crash
        player.step_backward()
        player.seek(5)


class TestAnimationPlayerSignals:
    def test_frame_changed_on_step(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        received = []
        player.frame_changed.connect(received.append)

        player.step_forward()
        assert received == [20]

    def test_frame_changed_on_set_sequence(self, qapp):
        player = AnimationPlayer()
        received = []
        player.frame_changed.connect(received.append)

        player.set_raw_sequence([42, 43, 44])
        assert received == [42]

    def test_frame_changed_on_seek(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([10, 20, 30])

        received = []
        player.frame_changed.connect(received.append)

        player.seek(2)
        assert received == [30]


class TestAnimationPlayerPlayback:
    def test_play_stop(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([1, 2, 3])

        player.play()
        assert player.is_playing is True

        player.stop()
        assert player.is_playing is False

    def test_toggle(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([1, 2, 3])

        player.toggle()
        assert player.is_playing is True

        player.toggle()
        assert player.is_playing is False

    def test_play_empty_does_nothing(self, qapp):
        player = AnimationPlayer()
        player.play()
        assert player.is_playing is False

    def test_set_speed(self, qapp):
        player = AnimationPlayer()
        player.set_speed(200)
        assert player.interval_ms == 200

    def test_set_speed_minimum(self, qapp):
        player = AnimationPlayer()
        player.set_speed(5)
        assert player.interval_ms == 20  # clamped

    def test_set_animation_resets_playback(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([1, 2, 3])
        player.play()
        assert player.is_playing is True

        player.set_raw_sequence([10, 20])
        assert player.is_playing is False
        assert player.current_position == 0

    def test_non_looping_stops_at_end(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([1, 2, 3])
        player.set_looping(False)

        finished = []
        player.animation_finished.connect(lambda: finished.append(True))

        # Manually advance to end (simulating timer)
        player._advance()  # pos 1
        player._advance()  # pos 2
        player._advance()  # past end → should stop

        assert player.is_playing is False
        assert len(finished) == 1

    def test_looping_wraps_around(self, qapp):
        player = AnimationPlayer()
        player.set_raw_sequence([1, 2, 3])
        player.set_looping(True)

        player._advance()  # pos 1
        player._advance()  # pos 2
        player._advance()  # wraps to pos 0

        assert player.current_position == 0
        assert player.current_frame_index == 1


class TestCompositeAnimationSpeed:
    def test_move_uses_move_speed(self, qapp):
        info = _make_anim_info(
            (AnimType.MOVE_START, [2, 3]),
            (AnimType.MOVE_MAIN, [4, 5, 6]),
            (AnimType.MOVE_STOP, [7, 8]),
        )
        player = AnimationPlayer()
        player.set_animation(info, "Move")
        # 480ms move speed / 7 frames ≈ 68ms, clamped to ≥40
        assert player.interval_ms >= 40

    def test_death_uses_default_speed(self, qapp):
        info = _make_anim_info(
            (AnimType.DEATH, [35, 36, 37]),
        )
        player = AnimationPlayer()
        player.set_animation(info, "Death")
        assert player.interval_ms == 120  # default
