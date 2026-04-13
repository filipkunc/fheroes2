"""QPainter-based animation canvas with side-by-side comparison, pan, and zoom."""

from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QRect, QPoint, Signal
from PySide6.QtGui import QPainter, QPixmap, QColor, QBrush, QPen, QImage, QCursor

from ..models.sprite_data import SpriteCollection, SpriteFrame

# Checkerboard tile size for transparency visualization
CHECKER_SIZE = 8
CHECKER_LIGHT = QColor(200, 200, 200)
CHECKER_DARK = QColor(160, 160, 160)


class AnimationCanvas(QWidget):
    """Renders sprite frames with correct offsets, supports side-by-side mode.

    Controls:
        Mouse wheel: zoom in/out
        Middle-click drag or Ctrl+left-click drag: pan
        Double-click: reset pan to center
    """

    frame_clicked = Signal(int, int)  # x, y in sprite coordinates

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(300, 300)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        self._left_sprites: SpriteCollection | None = None
        self._right_sprites: SpriteCollection | None = None
        self._current_frame_index: int = 0
        self._zoom: int = 3  # display scale factor
        self._side_by_side: bool = False
        self._show_offsets: bool = False
        self._bg_color = QColor(40, 40, 40)

        # Pan state
        self._pan_offset = QPoint(0, 0)
        self._panning = False
        self._pan_start = QPoint(0, 0)
        self._pan_start_offset = QPoint(0, 0)

        # Pre-build checkerboard tile
        self._checker_tile = QPixmap(CHECKER_SIZE * 2, CHECKER_SIZE * 2)
        p = QPainter(self._checker_tile)
        p.fillRect(0, 0, CHECKER_SIZE, CHECKER_SIZE, CHECKER_LIGHT)
        p.fillRect(CHECKER_SIZE, 0, CHECKER_SIZE, CHECKER_SIZE, CHECKER_DARK)
        p.fillRect(0, CHECKER_SIZE, CHECKER_SIZE, CHECKER_SIZE, CHECKER_DARK)
        p.fillRect(CHECKER_SIZE, CHECKER_SIZE, CHECKER_SIZE, CHECKER_SIZE, CHECKER_LIGHT)
        p.end()

    def set_sprites(self, left: SpriteCollection | None, right: SpriteCollection | None = None):
        self._left_sprites = left
        self._right_sprites = right
        self._side_by_side = right is not None
        self.update()

    def set_frame_index(self, index: int):
        self._current_frame_index = index
        self.update()

    def set_zoom(self, zoom: int):
        self._zoom = max(1, min(zoom, 10))
        self.update()

    def set_show_offsets(self, show: bool):
        self._show_offsets = show
        self.update()

    def reset_pan(self):
        self._pan_offset = QPoint(0, 0)
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)

        # Dark background
        painter.fillRect(self.rect(), self._bg_color)

        if self._side_by_side and self._left_sprites and self._right_sprites:
            mid_x = self.width() // 2
            self._draw_sprite_panel(painter, self._left_sprites, QRect(0, 0, mid_x, self.height()), "Base")
            # Divider line
            painter.setPen(QPen(QColor(80, 80, 80), 1))
            painter.drawLine(mid_x, 0, mid_x, self.height())
            self._draw_sprite_panel(painter, self._right_sprites, QRect(mid_x, 0, mid_x, self.height()), "Custom")
        elif self._left_sprites:
            self._draw_sprite_panel(painter, self._left_sprites, self.rect(), "")
        elif self._right_sprites:
            self._draw_sprite_panel(painter, self._right_sprites, self.rect(), "")

        painter.end()

    def _draw_sprite_panel(self, painter: QPainter, sprites: SpriteCollection, rect: QRect, label: str):
        """Draw a single sprite panel within the given rectangle."""
        frame = sprites.get_frame(self._current_frame_index)
        if frame is None or frame.is_placeholder:
            painter.setPen(QColor(100, 100, 100))
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, f"Frame {self._current_frame_index}\n(no data)")
            return

        zoom = self._zoom
        img = frame.image
        display_w = img.width * zoom
        display_h = img.height * zoom

        # Center the sprite in the panel, plus pan offset
        cx = rect.x() + rect.width() // 2 + self._pan_offset.x()
        cy = rect.y() + rect.height() // 2 + self._pan_offset.y()

        # Apply sprite offset for positioning
        draw_x = cx - display_w // 2 + frame.offset_x * zoom
        draw_y = cy - display_h // 2 + frame.offset_y * zoom

        # Draw checkerboard behind the sprite area
        checker_rect = QRect(draw_x, draw_y, display_w, display_h)
        painter.save()
        painter.setClipRect(checker_rect.intersected(rect))
        brush = QBrush(self._checker_tile)
        painter.fillRect(checker_rect, brush)
        painter.restore()

        # Convert to QPixmap and draw scaled
        painter.save()
        painter.setClipRect(rect)
        pixmap = frame.to_qpixmap()
        painter.drawPixmap(draw_x, draw_y, display_w, display_h, pixmap)
        painter.restore()

        # Offset crosshair (at the anchor point, not the sprite corner)
        if self._show_offsets:
            painter.setPen(QPen(QColor(255, 0, 0, 128), 1, Qt.PenStyle.DashLine))
            painter.drawLine(cx, rect.y(), cx, rect.y() + rect.height())
            painter.drawLine(rect.x(), cy, rect.x() + rect.width(), cy)

        # Label
        if label:
            painter.setPen(QColor(180, 180, 180))
            painter.drawText(rect.x() + 8, rect.y() + 18, label)

        # Frame info
        painter.setPen(QColor(120, 120, 120))
        info = f"#{self._current_frame_index}  {img.width}x{img.height}  zoom:{zoom}x"
        if frame.offset_x != 0 or frame.offset_y != 0:
            info += f"  offset({frame.offset_x}, {frame.offset_y})"
        painter.drawText(rect.x() + 8, rect.y() + rect.height() - 8, info)

    # --- Input handling ---

    def wheelEvent(self, event):
        delta = event.angleDelta().y()
        if delta > 0:
            self.set_zoom(self._zoom + 1)
        elif delta < 0:
            self.set_zoom(self._zoom - 1)

    def mousePressEvent(self, event):
        # Middle-click or Ctrl+left-click starts panning
        if (event.button() == Qt.MouseButton.MiddleButton or
                (event.button() == Qt.MouseButton.LeftButton and
                 event.modifiers() & Qt.KeyboardModifier.ControlModifier)):
            self._panning = True
            self._pan_start = event.position().toPoint()
            self._pan_start_offset = QPoint(self._pan_offset)
            self.setCursor(QCursor(Qt.CursorShape.ClosedHandCursor))
            event.accept()

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
        # Double-click resets pan
        self.reset_pan()
        event.accept()
