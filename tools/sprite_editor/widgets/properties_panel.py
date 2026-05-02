"""Properties panel showing per-frame metadata, offset editor, and animation info."""

from PIL import Image
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QGroupBox, QFormLayout, QLabel, QSpinBox, QDoubleSpinBox,
    QScrollArea, QFrame,
)
from PySide6.QtCore import Signal, Qt, QRect
from PySide6.QtGui import QPainter, QPixmap, QColor, QPen, QBrush, QImage

from ..models.sprite_data import SpriteFrame, SpriteCollection
from ..models.bin_parser import MonsterAnimInfo
from ..tools import portrait_data


# STRIP[12] dimensions (the dialog's gold-frame outer container, from STRIP.ICN
# in HEROES2.AGG). Race-specific STRIPs (the mountain/swamp/cave landscapes
# behind the figure) are 82×93 and sit at (6,6) inside STRIP[12].
STRIP_FRAME_W = 94
STRIP_FRAME_H = 105
STRIP_INNER_INSET = 6   # race STRIP is blitted at (6,6) inside STRIP[12]
STRIP_INNER_W = 82
STRIP_INNER_H = 93

# MONH dimensions vary per monster — we look them up from the AGG when possible.
# This default approximates Gargoyle (40×71 at offset (20,18) inside STRIP[12])
# so the preview is roughly right even without AGG access.
DEFAULT_MONH_W = 40
DEFAULT_MONH_H = 71
DEFAULT_MONH_OFFSET_X = 20  # STRIP[12]-relative
DEFAULT_MONH_OFFSET_Y = 18

DEFAULT_PORTRAIT_PREVIEW_SCALE = 1  # 1 logical pixel = 1 game pixel — matches the
# toolbar canvas zoom default. The actual preview scale is bound to the canvas
# zoom spin box (set_preview_scale) so the preview tracks whatever in-game pixel
# size the user has the rest of the editor at.
PORTRAIT_BBOX_MARGIN = 4  # mirrors GetRGBACustomPortrait


def _crop_alpha_bbox(img: Image.Image, margin: int = PORTRAIT_BBOX_MARGIN) -> Image.Image | None:
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    bbox = img.getbbox()
    if bbox is None:
        return None
    minX, minY, maxX, maxY = bbox  # PIL maxX/maxY are exclusive
    minX = max(0, minX - margin)
    minY = max(0, minY - margin)
    maxX = min(img.width, maxX + margin)
    maxY = min(img.height, maxY + margin)
    if maxX <= minX or maxY <= minY:
        return None
    return img.crop((minX, minY, maxX, maxY))


def _pil_to_qpixmap(img: Image.Image) -> QPixmap:
    rgba = img.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    qimg = QImage(data, rgba.width, rgba.height, rgba.width * 4, QImage.Format.Format_RGBA8888)
    return QPixmap.fromImage(qimg.copy())


class PortraitPreview(QWidget):
    """Renders the same composite the engine builds in MonsterDialogElement::draw.

    Layers (bottom to top):
        1. STRIP[12]: gold-frame outer border (transparent interior).
        2. STRIP[race]: race-specific landscape backdrop, at (6,6) inside STRIP[12].
        3. Hi-res portrait fitted into a box that expands from MONH toward the
           race-frame interior as portrait_zoom grows. Same math as the engine's
           ui_dialog.cpp:MonsterDialogElement::draw.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._cropped: QPixmap | None = None
        self._strip12_pix: QPixmap | None = None
        self._race_strip_pix: QPixmap | None = None
        self._monh_w = DEFAULT_MONH_W
        self._monh_h = DEFAULT_MONH_H
        self._monh_offset_x = DEFAULT_MONH_OFFSET_X
        self._monh_offset_y = DEFAULT_MONH_OFFSET_Y
        self._zoom: float = 1.0
        self._scale: int = DEFAULT_PORTRAIT_PREVIEW_SCALE
        self._apply_scale()

    def set_preview_scale(self, scale: int):
        """Sync preview's logical-pixel-per-game-pixel ratio with the canvas zoom."""
        scale = max(1, int(scale))
        if scale == self._scale:
            return
        self._scale = scale
        self._apply_scale()
        self.update()

    def _apply_scale(self):
        self.setFixedSize(
            STRIP_FRAME_W * self._scale + 2,
            STRIP_FRAME_H * self._scale + 2,
        )

    def set_portrait(self, frame: SpriteFrame | None, prefix: str | None = None):
        """Bind the portrait source. Also looks up race STRIP + MONH for `prefix`."""
        if frame is None or frame.is_placeholder:
            self._cropped = None
        else:
            cropped = _crop_alpha_bbox(frame.image)
            self._cropped = _pil_to_qpixmap(cropped) if cropped is not None else None

        self._refresh_engine_data(prefix)
        self.update()

    def set_zoom(self, zoom: float):
        self._zoom = max(0.1, float(zoom))
        self.update()

    def _refresh_engine_data(self, prefix: str | None):
        """Lazy-load STRIP[12], the race STRIP for `prefix`, and the MONH dims."""
        strip_frames = portrait_data.get_strip_frames()
        if strip_frames is not None and len(strip_frames) > 12:
            self._strip12_pix = _pil_to_qpixmap(strip_frames[12].image)
            race_idx = portrait_data.MONSTER_RACE_STRIP.get(prefix or "", 10)
            if race_idx < len(strip_frames):
                self._race_strip_pix = _pil_to_qpixmap(strip_frames[race_idx].image)
            else:
                self._race_strip_pix = None
        else:
            self._strip12_pix = None
            self._race_strip_pix = None

        monh_dims = portrait_data.peek_monh_dims(prefix or "")
        if monh_dims is not None:
            self._monh_w, self._monh_h, self._monh_offset_x, self._monh_offset_y = monh_dims
        else:
            self._monh_w = DEFAULT_MONH_W
            self._monh_h = DEFAULT_MONH_H
            self._monh_offset_x = DEFAULT_MONH_OFFSET_X
            self._monh_offset_y = DEFAULT_MONH_OFFSET_Y

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, True)

        s = self._scale
        canvasX = 1
        canvasY = 1
        canvasW = STRIP_FRAME_W * s
        canvasH = STRIP_FRAME_H * s

        # Race STRIP backdrop (mountains/cave/etc) at (6,6) inside STRIP[12].
        if self._race_strip_pix is not None and not self._race_strip_pix.isNull():
            painter.drawPixmap(
                canvasX + STRIP_INNER_INSET * s, canvasY + STRIP_INNER_INSET * s,
                STRIP_INNER_W * s, STRIP_INNER_H * s,
                self._race_strip_pix,
            )
        else:
            # Fallback when AGG isn't available — neutral brown.
            painter.fillRect(
                QRect(canvasX + STRIP_INNER_INSET * s, canvasY + STRIP_INNER_INSET * s,
                      STRIP_INNER_W * s, STRIP_INNER_H * s),
                QColor(96, 64, 32),
            )

        # Custom portrait: same box-expansion math as MonsterDialogElement::draw.
        if self._cropped is not None and not self._cropped.isNull():
            srcW = self._cropped.width()
            srcH = self._cropped.height()
            if srcW > 0 and srcH > 0:
                outerW = STRIP_FRAME_W - 2 * STRIP_INNER_INSET   # race-frame interior
                outerH = STRIP_FRAME_H - 2 * STRIP_INNER_INSET
                zoom = max(1.0, self._zoom)
                targetW = min(outerW, round(self._monh_w * zoom))
                targetH = min(outerH, round(self._monh_h * zoom))
                boxW = max(self._monh_w, targetW)
                boxH = max(self._monh_h, targetH)

                # MONH offsets are STRIP[12]-relative; rebase into race-frame coords.
                monhCentreXRel = self._monh_offset_x - STRIP_INNER_INSET + self._monh_w // 2
                monhBottomYRel = self._monh_offset_y - STRIP_INNER_INSET + self._monh_h
                boxX = monhCentreXRel - boxW // 2
                boxY = monhBottomYRel - boxH
                boxX = max(0, min(boxX, outerW - boxW))
                boxY = max(0, min(boxY, outerH - boxH))

                # Uniform fit inside box, bottom-anchored, horizontally centred.
                overlayW = boxW
                overlayH = (srcH * boxW) // srcW
                if overlayH > boxH:
                    overlayH = boxH
                    overlayW = (srcW * boxH) // srcH
                overlayX = boxX + (boxW - overlayW) // 2
                overlayY = boxY + (boxH - overlayH)

                # Translate to canvas coords (race-frame origin = canvas + (6,6)).
                drawX = canvasX + (STRIP_INNER_INSET + overlayX) * s
                drawY = canvasY + (STRIP_INNER_INSET + overlayY) * s
                painter.drawPixmap(drawX, drawY, overlayW * s, overlayH * s, self._cropped)

        # Gold-frame STRIP[12] on top. Its interior is transparent so the
        # backdrop + figure show through; the gold border masks anything that
        # leaks past STRIP[12]'s outer rect.
        if self._strip12_pix is not None and not self._strip12_pix.isNull():
            painter.drawPixmap(canvasX, canvasY, canvasW, canvasH, self._strip12_pix)


class PropertiesPanel(QWidget):
    """Displays and edits properties of the currently selected frame."""

    offset_changed = Signal(int, int, int)  # frame_index, new_offset_x, new_offset_y
    portrait_zoom_changed = Signal(float)   # new portrait zoom multiplier

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(220)

        # Wrap all groups in a QScrollArea so the panel never clips its contents
        # on smaller windows / high-DPI displays.
        outer_layout = QVBoxLayout(self)
        outer_layout.setContentsMargins(0, 0, 0, 0)
        scroll = QScrollArea(self)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        outer_layout.addWidget(scroll)

        content = QWidget()
        scroll.setWidget(content)
        layout = QVBoxLayout(content)
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

        # Portrait group — controls the in-game Set Count / army info portrait.
        portrait_group = QGroupBox("Portrait (Set Count dialog)")
        portrait_layout = QVBoxLayout()
        zoom_form = QFormLayout()
        self._portrait_zoom_spin = QDoubleSpinBox()
        self._portrait_zoom_spin.setRange(1.0, 3.0)
        self._portrait_zoom_spin.setSingleStep(0.05)
        self._portrait_zoom_spin.setDecimals(2)
        self._portrait_zoom_spin.setValue(1.0)
        zoom_form.addRow("Zoom:", self._portrait_zoom_spin)
        portrait_layout.addLayout(zoom_form)
        self._portrait_preview = PortraitPreview()
        portrait_layout.addWidget(self._portrait_preview, alignment=Qt.AlignmentFlag.AlignCenter)
        portrait_group.setLayout(portrait_layout)
        layout.addWidget(portrait_group)

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
        self._portrait_zoom_spin.valueChanged.connect(self._on_portrait_zoom_changed)

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

    def set_portrait_source(self, collection: SpriteCollection | None):
        """Bind the portrait preview to a sprite collection.

        Mirrors the engine: prefers frame 1 (idle pose), falls back to the
        first non-placeholder frame so the preview still renders for sets
        whose frame 1 is the placeholder.
        """
        self._updating = True
        if collection is None:
            self._portrait_zoom_spin.setValue(1.0)
            self._portrait_preview.set_portrait(None, None)
            self._portrait_preview.set_zoom(1.0)
            self._updating = False
            return

        self._portrait_zoom_spin.setValue(collection.portrait_zoom)
        self._portrait_preview.set_zoom(collection.portrait_zoom)

        portrait_frame = collection.get_frame(1)
        if portrait_frame is None or portrait_frame.is_placeholder:
            portrait_frame = next(
                (f for f in collection.frames if not f.is_placeholder),
                None,
            )
        self._portrait_preview.set_portrait(portrait_frame, collection.prefix)
        self._updating = False

    def set_animation_info(self, name: str, frame_sequence: list[int], speed_ms: int, position: int):
        """Update animation info display."""
        self._anim_name_label.setText(name)
        self._anim_frames_label.setText(f"{len(frame_sequence)} frames")
        self._anim_speed_label.setText(f"{speed_ms} ms/frame")
        self._anim_pos_label.setText(f"{position + 1}/{len(frame_sequence)}")

    def set_selection_count(self, count: int):
        self._selection_count_label.setText(str(count))

    def set_preview_scale(self, scale: int):
        """Forward canvas zoom to the portrait preview so both share a pixel scale."""
        self._portrait_preview.set_preview_scale(scale)

    def _on_offset_changed(self):
        if self._updating or self._current_frame_index < 0:
            return
        self.offset_changed.emit(
            self._current_frame_index,
            self._offset_x_spin.value(),
            self._offset_y_spin.value(),
        )

    def _on_portrait_zoom_changed(self, value: float):
        self._portrait_preview.set_zoom(value)
        if self._updating:
            return
        self.portrait_zoom_changed.emit(float(value))
