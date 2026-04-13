"""Sprite sheet grid view for inspecting Gemini input/output sheets."""

from PySide6.QtWidgets import QWidget, QScrollArea, QVBoxLayout, QHBoxLayout, QPushButton, QLabel
from PySide6.QtCore import Qt, Signal, QRect
from PySide6.QtGui import QPainter, QPixmap, QColor, QImage, QPen

from PIL import Image
import numpy as np


class SpritesheetCanvas(QWidget):
    """Renders a sprite sheet with grid overlay and cell highlighting."""

    cell_clicked = Signal(int)  # cell index

    def __init__(self, parent=None):
        super().__init__(parent)
        self._pixmap: QPixmap | None = None
        self._cells: list[tuple[int, int, int, int]] = []  # (x, y, w, h) per cell
        self._highlighted: set[int] = set()
        self._zoom: int = 1

    def set_sheet(self, image: Image.Image, cells: list[tuple[int, int, int, int]]):
        """Set the sprite sheet image and cell layout."""
        img = image.convert("RGBA")
        data = img.tobytes("raw", "RGBA")
        qimg = QImage(data, img.width, img.height, img.width * 4, QImage.Format.Format_RGBA8888)
        self._pixmap = QPixmap.fromImage(qimg.copy())
        self._cells = cells
        self._update_size()
        self.update()

    def set_zoom(self, zoom: int):
        self._zoom = max(1, min(zoom, 8))
        self._update_size()
        self.update()

    def set_highlighted(self, indices: set[int]):
        self._highlighted = indices
        self.update()

    def _update_size(self):
        if self._pixmap:
            self.setFixedSize(self._pixmap.width() * self._zoom, self._pixmap.height() * self._zoom)

    def paintEvent(self, event):
        if not self._pixmap:
            return
        painter = QPainter(self)
        z = self._zoom
        painter.drawPixmap(0, 0, self._pixmap.width() * z, self._pixmap.height() * z, self._pixmap)

        # Draw cell outlines
        for i, (x, y, w, h) in enumerate(self._cells):
            if i in self._highlighted:
                painter.setPen(QPen(QColor(0, 255, 0, 200), 2))
            else:
                painter.setPen(QPen(QColor(255, 255, 0, 100), 1))
            painter.drawRect(x * z, y * z, w * z, h * z)

            # Cell index label
            painter.setPen(QColor(255, 255, 255, 180))
            painter.drawText(x * z + 2, y * z + 12, str(i))

        painter.end()

    def mousePressEvent(self, event):
        if not self._cells:
            return
        z = self._zoom
        mx, my = event.position().x() / z, event.position().y() / z
        for i, (x, y, w, h) in enumerate(self._cells):
            if x <= mx <= x + w and y <= my <= y + h:
                self.cell_clicked.emit(i)
                return


class SpritesheetView(QWidget):
    """Collapsible sprite sheet panel with input/output tabs."""

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Controls
        ctrl_layout = QHBoxLayout()
        self._label = QLabel("No sheet loaded")
        ctrl_layout.addWidget(self._label)
        ctrl_layout.addStretch()

        zoom_in = QPushButton("+")
        zoom_in.setFixedWidth(30)
        zoom_out = QPushButton("-")
        zoom_out.setFixedWidth(30)
        zoom_in.clicked.connect(lambda: self._canvas.set_zoom(self._canvas._zoom + 1))
        zoom_out.clicked.connect(lambda: self._canvas.set_zoom(max(1, self._canvas._zoom - 1)))
        ctrl_layout.addWidget(zoom_out)
        ctrl_layout.addWidget(zoom_in)
        layout.addLayout(ctrl_layout)

        # Scrollable canvas
        scroll = QScrollArea()
        self._canvas = SpritesheetCanvas()
        scroll.setWidget(self._canvas)
        scroll.setWidgetResizable(False)
        layout.addWidget(scroll)

    def set_sheet(self, image: Image.Image, cells: list[tuple[int, int, int, int]], label: str = ""):
        self._canvas.set_sheet(image, cells)
        self._label.setText(label or f"Sheet: {image.width}x{image.height}, {len(cells)} cells")

    def set_highlighted(self, indices: set[int]):
        self._canvas.set_highlighted(indices)
