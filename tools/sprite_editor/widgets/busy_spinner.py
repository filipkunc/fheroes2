"""Small animated busy spinner — a rotating arc, no GIF/asset dependency.

Designed to sit inline next to the Generate button so the user can tell at a
glance that a Gemini request is in flight (the status-label text below was easy
to miss). Default size is 18 px; call ``start()`` when work begins and
``stop()`` when it ends. ``stop()`` also hides the widget so it doesn't leave a
gap in the action row.
"""

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QWidget


class BusySpinner(QWidget):
    def __init__(self, parent=None, size: int = 18, color: QColor | None = None):
        super().__init__(parent)
        self._angle = 0
        self._size = size
        self._color = color or QColor(255, 220, 80)
        self.setFixedSize(size, size)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        # Hidden by default — start() makes it visible, stop() hides it again.
        self.hide()

    def start(self):
        if not self._timer.isActive():
            self._timer.start(60)
        self.show()

    def stop(self):
        if self._timer.isActive():
            self._timer.stop()
        self.hide()

    def _tick(self):
        self._angle = (self._angle + 30) % 360
        self.update()

    def paintEvent(self, _evt):
        if not self._timer.isActive():
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Pen sized so the arc reads at small widget sizes (~18 px).
        pen_width = max(2, self._size // 6)
        rect = self.rect().adjusted(pen_width, pen_width, -pen_width, -pen_width)

        # Faint background ring so the spinner is visible even between sweeps.
        bg = QColor(self._color)
        bg.setAlpha(60)
        painter.setPen(QPen(bg, pen_width, Qt.PenStyle.SolidLine,
                            Qt.PenCapStyle.RoundCap))
        painter.drawArc(rect, 0, 360 * 16)

        # Main rotating arc — Qt's angle units are 1/16th of a degree.
        painter.setPen(QPen(self._color, pen_width, Qt.PenStyle.SolidLine,
                            Qt.PenCapStyle.RoundCap))
        # Sweep ~120° anchored on the current angle, rotating clockwise so it
        # reads as forward progress rather than spinning in place.
        start_angle = (-self._angle) * 16
        sweep = 120 * 16
        painter.drawArc(rect, start_angle, sweep)
