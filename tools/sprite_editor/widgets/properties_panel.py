"""Properties panel showing per-frame metadata, offset editor, and animation info."""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QGroupBox, QFormLayout, QLabel, QSpinBox
)
from PySide6.QtCore import Signal

from ..models.sprite_data import SpriteFrame
from ..models.bin_parser import MonsterAnimInfo


class PropertiesPanel(QWidget):
    """Displays and edits properties of the currently selected frame."""

    offset_changed = Signal(int, int, int)  # frame_index, new_offset_x, new_offset_y

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(180)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Frame info group
        frame_group = QGroupBox("Frame")
        frame_layout = QFormLayout()
        self._frame_index_label = QLabel("-")
        self._frame_size_label = QLabel("-")
        self._frame_placeholder_label = QLabel("-")
        frame_layout.addRow("Index:", self._frame_index_label)
        frame_layout.addRow("Size:", self._frame_size_label)
        frame_layout.addRow("Status:", self._frame_placeholder_label)
        frame_group.setLayout(frame_layout)
        layout.addWidget(frame_group)

        # Offset group
        offset_group = QGroupBox("Offset")
        offset_layout = QFormLayout()
        self._offset_x_spin = QSpinBox()
        self._offset_x_spin.setRange(-500, 500)
        self._offset_y_spin = QSpinBox()
        self._offset_y_spin.setRange(-500, 500)
        offset_layout.addRow("X:", self._offset_x_spin)
        offset_layout.addRow("Y:", self._offset_y_spin)
        offset_group.setLayout(offset_layout)
        layout.addWidget(offset_group)

        # Animation info group
        anim_group = QGroupBox("Animation")
        anim_layout = QFormLayout()
        self._anim_name_label = QLabel("-")
        self._anim_frames_label = QLabel("-")
        self._anim_speed_label = QLabel("-")
        self._anim_pos_label = QLabel("-")
        anim_layout.addRow("Type:", self._anim_name_label)
        anim_layout.addRow("Frames:", self._anim_frames_label)
        anim_layout.addRow("Speed:", self._anim_speed_label)
        anim_layout.addRow("Position:", self._anim_pos_label)
        anim_group.setLayout(anim_layout)
        layout.addWidget(anim_group)

        # Selection info group
        sel_group = QGroupBox("Selection")
        sel_layout = QFormLayout()
        self._selection_count_label = QLabel("0")
        sel_layout.addRow("Selected:", self._selection_count_label)
        sel_group.setLayout(sel_layout)
        layout.addWidget(sel_group)

        layout.addStretch()

        self._current_frame_index = -1
        self._updating = False

        self._offset_x_spin.valueChanged.connect(self._on_offset_changed)
        self._offset_y_spin.valueChanged.connect(self._on_offset_changed)

    def set_frame(self, frame: SpriteFrame | None):
        """Update panel to show info for the given frame."""
        self._updating = True
        if frame is None:
            self._frame_index_label.setText("-")
            self._frame_size_label.setText("-")
            self._frame_placeholder_label.setText("-")
            self._offset_x_spin.setValue(0)
            self._offset_y_spin.setValue(0)
            self._current_frame_index = -1
        else:
            self._current_frame_index = frame.index
            self._frame_index_label.setText(str(frame.index))
            if frame.is_placeholder:
                self._frame_size_label.setText("1x1")
                self._frame_placeholder_label.setText("Placeholder")
            else:
                self._frame_size_label.setText(f"{frame.image.width}x{frame.image.height}")
                self._frame_placeholder_label.setText("OK")
            self._offset_x_spin.setValue(frame.offset_x)
            self._offset_y_spin.setValue(frame.offset_y)
        self._updating = False

    def set_animation_info(self, name: str, frame_sequence: list[int], speed_ms: int, position: int):
        """Update animation info display."""
        self._anim_name_label.setText(name)
        self._anim_frames_label.setText(f"{len(frame_sequence)} frames")
        self._anim_speed_label.setText(f"{speed_ms} ms/frame")
        self._anim_pos_label.setText(f"{position + 1}/{len(frame_sequence)}")

    def set_selection_count(self, count: int):
        self._selection_count_label.setText(str(count))

    def _on_offset_changed(self):
        if self._updating or self._current_frame_index < 0:
            return
        self.offset_changed.emit(
            self._current_frame_index,
            self._offset_x_spin.value(),
            self._offset_y_spin.value(),
        )
