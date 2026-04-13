"""Animation sequence player using BIN animation data."""

from PySide6.QtCore import QObject, Signal, QTimer

from .bin_parser import MonsterAnimInfo, AnimType, COMPOSITE_ANIMATIONS


class AnimationPlayer(QObject):
    """Drives animation playback by emitting frame index changes."""

    frame_changed = Signal(int)  # emits current frame index
    animation_finished = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance)

        self._frame_sequence: list[int] = []
        self._current_pos: int = 0
        self._looping: bool = True
        self._interval_ms: int = 120  # default frame interval
        self._playing: bool = False

    @property
    def current_frame_index(self) -> int:
        if not self._frame_sequence:
            return 0
        return self._frame_sequence[self._current_pos]

    @property
    def current_position(self) -> int:
        return self._current_pos

    @property
    def sequence_length(self) -> int:
        return len(self._frame_sequence)

    @property
    def is_playing(self) -> bool:
        return self._playing

    @property
    def interval_ms(self) -> int:
        return self._interval_ms

    def set_animation(self, anim_info: MonsterAnimInfo, anim_name: str):
        """Set the animation sequence from a composite animation name."""
        self.stop()
        self._frame_sequence = anim_info.get_composite_frames(anim_name)
        self._current_pos = 0

        # Use appropriate speed from BIN data
        if "Shoot" in anim_name and anim_info.shoot_speed > 0:
            self._interval_ms = max(40, anim_info.shoot_speed // max(1, len(self._frame_sequence)))
        elif "Move" in anim_name and anim_info.move_speed > 0:
            self._interval_ms = max(40, anim_info.move_speed // max(1, len(self._frame_sequence)))
        else:
            self._interval_ms = 120

        if self._frame_sequence:
            self.frame_changed.emit(self._frame_sequence[0])

    def set_raw_sequence(self, frames: list[int]):
        """Set an explicit frame sequence (for single-anim or custom playback)."""
        self.stop()
        self._frame_sequence = frames
        self._current_pos = 0
        if frames:
            self.frame_changed.emit(frames[0])

    def set_speed(self, interval_ms: int):
        self._interval_ms = max(20, interval_ms)
        if self._playing:
            self._timer.setInterval(self._interval_ms)

    def set_looping(self, loop: bool):
        self._looping = loop

    def play(self):
        if not self._frame_sequence:
            return
        self._playing = True
        self._timer.start(self._interval_ms)

    def stop(self):
        self._playing = False
        self._timer.stop()

    def toggle(self):
        if self._playing:
            self.stop()
        else:
            self.play()

    def step_forward(self):
        """Advance one frame manually."""
        if not self._frame_sequence:
            return
        self._current_pos = (self._current_pos + 1) % len(self._frame_sequence)
        self.frame_changed.emit(self._frame_sequence[self._current_pos])

    def step_backward(self):
        """Go back one frame manually."""
        if not self._frame_sequence:
            return
        self._current_pos = (self._current_pos - 1) % len(self._frame_sequence)
        self.frame_changed.emit(self._frame_sequence[self._current_pos])

    def seek(self, position: int):
        """Jump to a specific position in the sequence."""
        if not self._frame_sequence:
            return
        self._current_pos = max(0, min(position, len(self._frame_sequence) - 1))
        self.frame_changed.emit(self._frame_sequence[self._current_pos])

    def _advance(self):
        if not self._frame_sequence:
            self.stop()
            return

        self._current_pos += 1
        if self._current_pos >= len(self._frame_sequence):
            if self._looping:
                self._current_pos = 0
            else:
                self._current_pos = len(self._frame_sequence) - 1
                self.stop()
                self.animation_finished.emit()
                return

        self.frame_changed.emit(self._frame_sequence[self._current_pos])
