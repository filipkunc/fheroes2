"""QPainter-based animation canvas with side-by-side comparison, pan, zoom, and hex grid."""

from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QRect, QPoint, QPointF, Signal
from PySide6.QtGui import (
    QPainter, QPixmap, QColor, QBrush, QPen, QImage, QCursor,
    QPainterPath, QPolygonF,
)

from ..models.sprite_data import SpriteCollection, SpriteFrame

# Checkerboard tile size for transparency visualization
CHECKER_SIZE = 8
CHECKER_LIGHT = QColor(200, 200, 200)
CHECKER_DARK = QColor(160, 160, 160)

# Battle hex grid constants (from battle_cell.h, battle_cell.cpp, battle_interface.cpp)
HEX_CELL_W = 44       # cell width in pixels
HEX_CELL_H = 52       # cell height in pixels
HEX_CELL_VER = 32     # vertical side of hexagon
HEX_CELL_INSET = (HEX_CELL_H - HEX_CELL_VER) // 2  # = 10, top/bottom inset
HEX_ROW_STEP = HEX_CELL_H - HEX_CELL_INSET          # = 42, vertical step between rows
HEX_CELL_Y_OFFSET = -9  # sprite Y adjustment (battle_interface.cpp line 95)
HEX_GRID_COLS = 5      # number of hex columns to draw
HEX_GRID_ROWS = 5      # number of hex rows to draw


def _hex_polygon(x: float, y: float) -> QPolygonF:
    """Return a hexagon polygon for a cell at (x, y) in pixel coordinates."""
    inset = HEX_CELL_INSET
    w = HEX_CELL_W
    h = HEX_CELL_H
    mid_x = w / 2.0
    return QPolygonF([
        QPointF(x, y + inset),            # top-left
        QPointF(x + mid_x, y),            # top-center
        QPointF(x + w, y + inset),        # top-right
        QPointF(x + w, y + h - inset),    # bottom-right
        QPointF(x + mid_x, y + h),        # bottom-center
        QPointF(x, y + h - inset),        # bottom-left
    ])


class AnimationCanvas(QWidget):
    """Renders sprite frames with correct offsets, supports side-by-side mode.

    Controls:
        Mouse wheel: zoom in/out
        Middle-click drag or Ctrl+left-click drag: pan
        Shift+left-click drag: move sprite (edit offset)
        Double-click: reset pan to center
    """

    frame_clicked = Signal(int, int)  # x, y in sprite coordinates
    offset_changed = Signal(int, int, int)  # frame_index, offset_x, offset_y — emitted on drag release

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
        self._show_hex_grid: bool = False
        self._bg_color = QColor(40, 40, 40)

        # Pan state
        self._pan_offset = QPoint(0, 0)
        self._panning = False
        self._pan_start = QPoint(0, 0)
        self._pan_start_offset = QPoint(0, 0)

        # Offset-drag state (shift+left-drag moves sprite)
        self._dragging_offset = False
        self._drag_start_pos = QPoint(0, 0)
        self._drag_start_ox = 0
        self._drag_start_oy = 0

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

    def set_show_hex_grid(self, show: bool):
        self._show_hex_grid = show
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

        # Panel center with pan offset
        cx = rect.x() + rect.width() // 2 + self._pan_offset.x()
        cy = rect.y() + rect.height() // 2 + self._pan_offset.y()

        painter.save()
        painter.setClipRect(rect)

        if self._show_hex_grid:
            self._draw_hex_grid_and_sprite(painter, frame, cx, cy, zoom)
        else:
            self._draw_sprite_simple(painter, frame, cx, cy, zoom)

        painter.restore()

        # Offset crosshair
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
        dw, dh = frame.display_width, frame.display_height
        info = f"#{self._current_frame_index}  {img.width}x{img.height}"
        if dw != img.width or dh != img.height:
            info += f" (display:{dw}x{dh})"
        info += f"  zoom:{zoom}x"
        if frame.offset_x != 0 or frame.offset_y != 0:
            info += f"  offset({frame.offset_x}, {frame.offset_y})"
        painter.drawText(rect.x() + 8, rect.y() + rect.height() - 8, info)

    def _draw_sprite_simple(self, painter: QPainter, frame: SpriteFrame, cx: int, cy: int, zoom: int):
        """Draw sprite centered in panel with checkerboard (original mode).

        Uses display_width/display_height (base ICN dimensions) for positioning,
        but renders the hi-res image scaled to that footprint for quality.
        """
        # Display size = base ICN dimensions × zoom (matches game's BlitRGBAScaled)
        display_w = frame.display_width * zoom
        display_h = frame.display_height * zoom
        draw_x = cx - display_w // 2 + frame.offset_x * zoom
        draw_y = cy - display_h // 2 + frame.offset_y * zoom

        # Checkerboard
        checker_rect = QRect(draw_x, draw_y, display_w, display_h)
        painter.save()
        painter.setClipRect(checker_rect)
        painter.fillRect(checker_rect, QBrush(self._checker_tile))
        painter.restore()

        # Render hi-res image scaled to the display footprint
        pixmap = frame.to_qpixmap()
        painter.drawPixmap(draw_x, draw_y, display_w, display_h, pixmap)

    def _draw_hex_grid_and_sprite(self, painter: QPainter, frame: SpriteFrame, cx: int, cy: int, zoom: int):
        """Draw hex grid with sprite positioned using battle engine formula."""
        z = zoom
        cell_w = HEX_CELL_W * z
        cell_h = HEX_CELL_H * z
        row_step = HEX_ROW_STEP * z
        inset = HEX_CELL_INSET * z

        # The "home" cell is at the center of our grid section.
        # We place row=2, col=2 of a 5x5 grid at the panel center.
        home_row = HEX_GRID_ROWS // 2
        home_col = HEX_GRID_COLS // 2

        # Home cell top-left position: center the grid on the panel
        home_x = cx - cell_w // 2
        home_y = cy - cell_h // 2

        # Draw all hex cells
        for row in range(HEX_GRID_ROWS):
            for col in range(HEX_GRID_COLS):
                # Stagger odd rows by half a cell width
                stagger = (cell_w // 2) if (row % 2) else 0
                hx = home_x + (col - home_col) * cell_w - stagger
                hy = home_y + (row - home_row) * row_step

                poly = _hex_polygon(hx / z, hy / z)
                # Scale polygon
                scaled_poly = QPolygonF([QPointF(p.x() * z, p.y() * z) for p in poly])

                is_home = (row == home_row and col == home_col)

                if is_home:
                    # Highlight home cell
                    painter.setBrush(QBrush(QColor(60, 90, 60, 80)))
                else:
                    painter.setBrush(Qt.BrushStyle.NoBrush)

                painter.setPen(QPen(QColor(80, 120, 80, 140), 1))
                painter.drawPolygon(scaled_poly)

        # Draw sprite using battle engine positioning formula:
        # draw_x = cell_x + (cell_w / 2) + sprite.offset_x
        # draw_y = cell_y + cell_h + sprite.offset_y + CELL_Y_OFFSET
        # = cell_y + 43 + sprite.offset_y  (at 1x zoom)
        draw_x = home_x + cell_w // 2 + frame.offset_x * z
        draw_y = home_y + cell_h + (frame.offset_y + HEX_CELL_Y_OFFSET) * z

        # Display size = base ICN dimensions × zoom (matches game's BlitRGBAScaled)
        # The hi-res PNG is scaled down to this footprint, preserving quality
        display_w = frame.display_width * z
        display_h = frame.display_height * z

        pixmap = frame.to_qpixmap()
        painter.drawPixmap(draw_x, draw_y, display_w, display_h, pixmap)

    # --- Input handling ---

    def _editable_sprites(self) -> SpriteCollection | None:
        """Which collection a drag on a given mouse position edits.

        In side-by-side mode, only drags on the right (custom) panel are editable.
        In single-panel mode, the sole visible collection is editable.
        """
        if self._side_by_side:
            return self._right_sprites
        return self._left_sprites or self._right_sprites

    def _point_in_editable_panel(self, pos: QPoint) -> bool:
        if not self._side_by_side:
            return True
        # Right panel starts at width/2
        return pos.x() >= self.width() // 2

    def wheelEvent(self, event):
        delta = event.angleDelta().y()
        if delta > 0:
            self.set_zoom(self._zoom + 1)
        elif delta < 0:
            self.set_zoom(self._zoom - 1)

    def mousePressEvent(self, event):
        # Shift+left = move sprite (edit offset)
        if (event.button() == Qt.MouseButton.LeftButton and
                event.modifiers() & Qt.KeyboardModifier.ShiftModifier):
            pos = event.position().toPoint()
            if not self._point_in_editable_panel(pos):
                return
            sprites = self._editable_sprites()
            frame = sprites.get_frame(self._current_frame_index) if sprites else None
            if frame and not frame.is_placeholder:
                self._dragging_offset = True
                self._drag_start_pos = pos
                self._drag_start_ox = frame.offset_x
                self._drag_start_oy = frame.offset_y
                self.setCursor(QCursor(Qt.CursorShape.SizeAllCursor))
                event.accept()
            return

        if (event.button() == Qt.MouseButton.MiddleButton or
                (event.button() == Qt.MouseButton.LeftButton and
                 event.modifiers() & Qt.KeyboardModifier.ControlModifier)):
            self._panning = True
            self._pan_start = event.position().toPoint()
            self._pan_start_offset = QPoint(self._pan_offset)
            self.setCursor(QCursor(Qt.CursorShape.ClosedHandCursor))
            event.accept()

    def mouseMoveEvent(self, event):
        if self._dragging_offset:
            delta = event.position().toPoint() - self._drag_start_pos
            sprites = self._editable_sprites()
            frame = sprites.get_frame(self._current_frame_index) if sprites else None
            if frame:
                frame.offset_x = self._drag_start_ox + delta.x() // self._zoom
                frame.offset_y = self._drag_start_oy + delta.y() // self._zoom
                self.update()
            event.accept()
            return

        if self._panning:
            delta = event.position().toPoint() - self._pan_start
            self._pan_offset = self._pan_start_offset + delta
            self.update()
            event.accept()

    def mouseReleaseEvent(self, event):
        if self._dragging_offset:
            self._dragging_offset = False
            self.setCursor(QCursor(Qt.CursorShape.ArrowCursor))
            sprites = self._editable_sprites()
            frame = sprites.get_frame(self._current_frame_index) if sprites else None
            if frame:
                self.offset_changed.emit(frame.index, frame.offset_x, frame.offset_y)
            event.accept()
            return

        if self._panning:
            self._panning = False
            self.setCursor(QCursor(Qt.CursorShape.ArrowCursor))
            event.accept()

    def mouseDoubleClickEvent(self, event):
        self.reset_pan()
        event.accept()
