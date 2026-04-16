"""Sprite sheet grid view for inspecting Gemini input/output sheets."""

from PySide6.QtWidgets import QWidget, QScrollArea, QVBoxLayout, QHBoxLayout, QPushButton, QLabel
from PySide6.QtCore import Qt, Signal, QRect, QPoint
from PySide6.QtGui import QPainter, QPixmap, QColor, QImage, QPen, QCursor, QWheelEvent

from PIL import Image
import numpy as np


class SpritesheetCanvas(QWidget):
    """Renders a sprite sheet with grid overlay, zoom, and pan.

    Controls:
        Mouse wheel: zoom in/out
        Middle-click drag or Ctrl+left-click drag: pan
        Double-click: fit to view
        Left-click: select cell
    """

    cell_clicked = Signal(int)  # cell index
    zoom_changed = Signal(float)  # current zoom level

    def __init__(self, parent=None):
        super().__init__(parent)
        self._pixmap: QPixmap | None = None
        self._cells: list[tuple[int, int, int, int]] = []
        self._highlighted: set[int] = set()
        self._zoom: float = 1.0
        self._min_zoom: float = 0.1
        self._max_zoom: float = 8.0

        # Pan state
        self._pan_offset = QPoint(0, 0)
        self._panning = False
        self._pan_start = QPoint(0, 0)
        self._pan_start_offset = QPoint(0, 0)

        self.setMinimumSize(200, 150)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def set_sheet(self, image: Image.Image, cells: list[tuple[int, int, int, int]]):
        img = image.convert("RGBA")
        data = img.tobytes("raw", "RGBA")
        qimg = QImage(data, img.width, img.height, img.width * 4, QImage.Format.Format_RGBA8888)
        self._pixmap = QPixmap.fromImage(qimg.copy())
        self._cells = cells
        self._pan_offset = QPoint(0, 0)
        self.update()

    def set_zoom(self, zoom: float):
        self._zoom = max(self._min_zoom, min(zoom, self._max_zoom))
        self.zoom_changed.emit(self._zoom)
        self.update()

    def fit_to_view(self):
        if not self._pixmap:
            return
        scale_x = self.width() / self._pixmap.width()
        scale_y = self.height() / self._pixmap.height()
        self.set_zoom(min(scale_x, scale_y) * 0.95)
        self._pan_offset = QPoint(0, 0)
        self.update()

    def set_highlighted(self, indices: set[int]):
        self._highlighted = indices
        self.update()

    def paintEvent(self, event):
        if not self._pixmap:
            return
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        painter.fillRect(self.rect(), QColor(30, 30, 30))

        z = self._zoom
        img_w = self._pixmap.width() * z
        img_h = self._pixmap.height() * z

        # Center image in widget + pan offset
        ox = (self.width() - img_w) / 2 + self._pan_offset.x()
        oy = (self.height() - img_h) / 2 + self._pan_offset.y()

        painter.drawPixmap(int(ox), int(oy), int(img_w), int(img_h), self._pixmap)

        # Draw cell outlines
        for i, (x, y, w, h) in enumerate(self._cells):
            if i in self._highlighted:
                painter.setPen(QPen(QColor(0, 255, 0, 200), 2))
            else:
                painter.setPen(QPen(QColor(255, 255, 0, 100), 1))
            painter.drawRect(int(ox + x * z), int(oy + y * z), int(w * z), int(h * z))

            painter.setPen(QColor(255, 255, 255, 180))
            painter.drawText(int(ox + x * z + 2), int(oy + y * z + 12), str(i + 1))

        painter.end()

    def wheelEvent(self, event: QWheelEvent):
        delta = event.angleDelta().y()
        factor = 1.15 if delta > 0 else 1 / 1.15
        self.set_zoom(self._zoom * factor)

    def mousePressEvent(self, event):
        if (event.button() == Qt.MouseButton.MiddleButton or
                (event.button() == Qt.MouseButton.LeftButton and
                 event.modifiers() & Qt.KeyboardModifier.ControlModifier)):
            self._panning = True
            self._pan_start = event.position().toPoint()
            self._pan_start_offset = QPoint(self._pan_offset)
            self.setCursor(QCursor(Qt.CursorShape.ClosedHandCursor))
            event.accept()
        elif event.button() == Qt.MouseButton.LeftButton and self._cells:
            # Cell click detection
            z = self._zoom
            ox = (self.width() - self._pixmap.width() * z) / 2 + self._pan_offset.x()
            oy = (self.height() - self._pixmap.height() * z) / 2 + self._pan_offset.y()
            mx = (event.position().x() - ox) / z
            my = (event.position().y() - oy) / z
            for i, (x, y, w, h) in enumerate(self._cells):
                if x <= mx <= x + w and y <= my <= y + h:
                    self.cell_clicked.emit(i)
                    return

    def mouseMoveEvent(self, event):
        if self._panning:
            delta = event.position().toPoint() - self._pan_start
            self._pan_offset = self._pan_start_offset + delta
            self.update()
            event.accept()

    def mouseReleaseEvent(self, event):
        if self._panning:
            self._panning = False
            self.setCursor(QCursor(Qt.CursorShape.ArrowCursor))
            event.accept()

    def mouseDoubleClickEvent(self, event):
        self.fit_to_view()
        event.accept()


class SpritesheetView(QWidget):
    """Sprite sheet panel with zoom/pan controls."""

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Controls
        ctrl_layout = QHBoxLayout()
        self._label = QLabel("No sheet loaded")
        ctrl_layout.addWidget(self._label)
        ctrl_layout.addStretch()

        self._zoom_label = QLabel("100%")
        ctrl_layout.addWidget(self._zoom_label)

        fit_btn = QPushButton("Fit")
        fit_btn.setFixedWidth(40)
        fit_btn.setToolTip("Fit sheet to view (or double-click)")
        fit_btn.clicked.connect(lambda: self._canvas.fit_to_view())
        ctrl_layout.addWidget(fit_btn)

        layout.addLayout(ctrl_layout)

        # Canvas (no scroll area needed — we handle pan internally)
        self._canvas = SpritesheetCanvas()
        self._canvas.zoom_changed.connect(self._on_zoom_changed)
        layout.addWidget(self._canvas, stretch=1)

    def set_sheet(self, image: Image.Image, cells: list[tuple[int, int, int, int]], label: str = ""):
        self._canvas.set_sheet(image, cells)
        self._canvas.fit_to_view()
        self._label.setText(label or f"Sheet: {image.width}x{image.height}, {len(cells)} cells")

    def set_highlighted(self, indices: set[int]):
        self._canvas.set_highlighted(indices)

    def _on_zoom_changed(self, zoom: float):
        self._zoom_label.setText(f"{zoom * 100:.0f}%")
