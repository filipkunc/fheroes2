"""Frame list widget with thumbnails and multi-selection."""

from PySide6.QtWidgets import QListWidget, QListWidgetItem, QAbstractItemView
from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QIcon

from ..models.sprite_data import SpriteCollection


THUMB_SIZE = 48


class FrameListWidget(QListWidget):
    """Displays sprite frames as a thumbnail list with multi-select support."""

    frame_selected = Signal(int)  # single-click selection
    frames_selected = Signal(list)  # multi-select for batch operations

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setIconSize(QSize(THUMB_SIZE, THUMB_SIZE))
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.setMinimumWidth(140)
        self.setMaximumWidth(200)

        self.currentRowChanged.connect(self._on_row_changed)
        self.itemSelectionChanged.connect(self._on_selection_changed)

        self._sprites: SpriteCollection | None = None

    def set_sprites(self, sprites: SpriteCollection):
        self._sprites = sprites
        self.clear()

        for frame in sprites.frames:
            label = f"{frame.index:03d}"
            if frame.is_placeholder:
                label += " (empty)"

            item = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, frame.index)

            if not frame.is_placeholder:
                pixmap = frame.to_qpixmap()
                if not pixmap.isNull():
                    scaled = pixmap.scaled(
                        THUMB_SIZE, THUMB_SIZE,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.FastTransformation,
                    )
                    item.setIcon(QIcon(scaled))

            self.addItem(item)

    def select_frame(self, index: int):
        """Programmatically select a frame by index."""
        for row in range(self.count()):
            item = self.item(row)
            if item.data(Qt.ItemDataRole.UserRole) == index:
                self.setCurrentRow(row)
                break

    def selected_frame_indices(self) -> list[int]:
        """Return list of selected frame indices."""
        indices = []
        for item in self.selectedItems():
            indices.append(item.data(Qt.ItemDataRole.UserRole))
        return sorted(indices)

    def _on_row_changed(self, row: int):
        if row < 0:
            return
        item = self.item(row)
        if item:
            self.frame_selected.emit(item.data(Qt.ItemDataRole.UserRole))

    def _on_selection_changed(self):
        self.frames_selected.emit(self.selected_frame_indices())
