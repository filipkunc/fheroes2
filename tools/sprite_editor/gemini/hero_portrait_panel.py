"""Hero portrait generation panel.

Two output targets, two flows:

* TARGET_BIG  → Gemini repaint of PORT0xxx, saved as hero_<name>_000.png.
  No background keying — Gemini's painted backdrop stays in the PNG; the engine
  composites the full RGBA on top of the indexed PORT slot.

* TARGET_SMALL → visual slice picker. The user drags an aspect-locked crop
  rectangle (matching MINIPORT's aspect ratio) over the already-generated big
  portrait, then accepts. The crop is saved as hero_<name>_small_000.png at full
  source resolution; the engine downscales at draw time. No Gemini call.
"""

from pathlib import Path

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QFormLayout,
    QTextEdit, QComboBox, QSpinBox, QPushButton, QLabel,
    QFileDialog, QMessageBox, QSplitter, QSizePolicy, QStackedWidget,
)
from PySide6.QtCore import Qt, QThread, Signal, QEvent, QRect, QPoint
from PySide6.QtGui import QImage, QPixmap, QPainter, QPen, QColor

from PIL import Image

from ..models.hero_config import HeroConfig, update_hero_manifest_entry
from .gemini_client import GeminiClient
from .cost_tracker import CostTracker


# Target identifiers — each maps to a different (input ICN, output filename, prompts) triple.
TARGET_BIG = "big"      # PORT0xxx → hero_<name>_000.png (full-frame portrait)
TARGET_SMALL = "small"  # Slice-from-big → hero_<name>_small_000.png (mini variant)


_DEFAULT_TRANSFORM_PROMPT = (
    "Repaint this hero portrait at high resolution while keeping the EXACT pose, framing, "
    "facial features, hair, clothing, and silhouette of the original. Sharpen details and "
    "add believable texture (skin, fabric, metal, hair strands). Do NOT change the head "
    "position, body angle, expression, or background composition. Improve the quality and "
    "realism."
)

_DEFAULT_SYSTEM_PROMPT = (
    "You are a high-resolution painter for fantasy game character portraits. You receive "
    "a small low-resolution portrait and must output a high-resolution version that "
    "preserves the subject's identity exactly — same face, same pose, same outfit, same "
    "composition. CRITICAL: do not crop, reframe, mirror, or change the camera angle. "
    "Aim for painterly fantasy-portrait realism — sharp eyes, natural skin tones, fabric "
    "and metal detail — without leaving the original silhouette."
)


class HeroPortraitWorker(QThread):
    """Background thread for the single-image Gemini call."""

    finished = Signal(object)  # PIL Image or None
    error = Signal(str)

    def __init__(self, client: GeminiClient, image: Image.Image, prompt: str,
                 system: str | None, reference: Image.Image | None):
        super().__init__()
        self._client = client
        self._image = image
        self._prompt = prompt
        self._system = system
        self._reference = reference
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def run(self):
        try:
            result = self._client.send_sheet(
                self._image, self._prompt,
                system_instruction=self._system,
                reference=self._reference,
                max_retries=1,
                should_cancel=lambda: self._cancelled,
            )
            if self._cancelled:
                self.error.emit("Cancelled by user")
                return
            self.finished.emit(result)
        except Exception as e:
            cause = getattr(e, "__cause__", None)
            if cause is not None and str(cause) and str(cause) not in str(e):
                self.error.emit(f"{e}\n\nUnderlying error:\n{type(cause).__name__}: {cause}")
            else:
                self.error.emit(f"{type(e).__name__}: {e}")


def _pil_to_qpixmap(img: Image.Image, max_w: int = 400, max_h: int = 400) -> QPixmap:
    rgba = img.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    qimg = QImage(data, rgba.width, rgba.height, rgba.width * 4, QImage.Format.Format_RGBA8888)
    pixmap = QPixmap.fromImage(qimg.copy())
    return pixmap.scaled(max_w, max_h, Qt.AspectRatioMode.KeepAspectRatio,
                         Qt.TransformationMode.SmoothTransformation)


def _sprites_dir() -> Path:
    return Path(__file__).parent.parent.parent.parent / "files" / "data" / "sprites"


# ── Slice canvas (small-portrait picker) ─────────────────────────────────────


class _SliceCanvas(QWidget):
    """Aspect-locked crop selector overlaid on a hi-res portrait.

    Drag the rect interior to translate it. Mouse wheel resizes around the
    cursor, preserving aspect. The rect is clamped to the image bounds.
    """

    cropChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._image: Image.Image | None = None
        self._aspect: float = 1.0  # crop_w / crop_h
        self._crop_norm: tuple[float, float, float, float] = (0.25, 0.05, 0.5, 0.5)
        self._drag_anchor: QPoint | None = None
        self._drag_initial_norm = (0.0, 0.0, 0.0, 0.0)
        self.setMinimumHeight(220)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setMouseTracking(False)
        self.setFocusPolicy(Qt.FocusPolicy.WheelFocus)

    # ── public API ────────────────────────────────────────────────────────

    def set_image(self, img: Image.Image | None):
        self._image = img
        if img is not None:
            self._reset_crop()
        self.update()
        self.cropChanged.emit()

    def set_aspect(self, aspect: float):
        if aspect > 0 and abs(aspect - self._aspect) > 1e-4:
            self._aspect = aspect
            if self._image is not None:
                self._reset_crop()
                self.update()
                self.cropChanged.emit()

    def crop_image_coords(self) -> tuple[int, int, int, int]:
        if self._image is None:
            return (0, 0, 0, 0)
        iw, ih = self._image.size
        x, y, w, h = self._crop_norm
        return (int(round(x * iw)), int(round(y * ih)),
                max(1, int(round(w * iw))), max(1, int(round(h * ih))))

    def cropped_image(self) -> Image.Image | None:
        if self._image is None:
            return None
        x, y, w, h = self.crop_image_coords()
        return self._image.crop((x, y, x + w, y + h))

    # ── internals ────────────────────────────────────────────────────────

    def _reset_crop(self):
        """Default crop: centered horizontally, anchored near the top of the
        portrait (where heads usually sit), sized to fill ~70% of image height."""
        if self._image is None:
            return
        iw, ih = self._image.size
        # Try height-bound first; fall back to width-bound if it overflows.
        h_norm = 0.7
        w_norm = h_norm * (ih / iw) * self._aspect
        if w_norm > 0.95:
            w_norm = 0.95
            h_norm = w_norm * (iw / ih) / self._aspect
        x_norm = (1.0 - w_norm) / 2
        y_norm = 0.02
        self._crop_norm = self._clamp(x_norm, y_norm, w_norm, h_norm)

    def _clamp(self, x: float, y: float, w: float, h: float) -> tuple[float, float, float, float]:
        w = max(0.02, min(1.0, w))
        h = max(0.02, min(1.0, h))
        x = max(0.0, min(1.0 - w, x))
        y = max(0.0, min(1.0 - h, y))
        return (x, y, w, h)

    def _display_rect(self) -> QRect:
        """Where the source image is drawn inside this widget (letterboxed)."""
        if self._image is None:
            return QRect()
        iw, ih = self._image.size
        ww, wh = self.width(), self.height()
        if iw <= 0 or ih <= 0 or ww <= 0 or wh <= 0:
            return QRect()
        scale = min(ww / iw, wh / ih)
        dw = max(1, int(iw * scale))
        dh = max(1, int(ih * scale))
        ox = (ww - dw) // 2
        oy = (wh - dh) // 2
        return QRect(ox, oy, dw, dh)

    def _crop_rect_display(self) -> QRect:
        disp = self._display_rect()
        if disp.isEmpty():
            return QRect()
        x, y, w, h = self._crop_norm
        return QRect(
            disp.x() + int(x * disp.width()),
            disp.y() + int(y * disp.height()),
            int(w * disp.width()),
            int(h * disp.height()),
        )

    # ── painting ─────────────────────────────────────────────────────────

    def paintEvent(self, _evt):
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(42, 42, 42))

        if self._image is None:
            painter.setPen(QColor(180, 180, 180))
            painter.drawText(
                self.rect(), Qt.AlignmentFlag.AlignCenter,
                "Generate the big portrait first.\nAccept it, then return here to slice the small variant.",
            )
            return

        disp = self._display_rect()
        if disp.isEmpty():
            return

        rgba = self._image.convert("RGBA")
        iw, ih = rgba.size
        qimg = QImage(rgba.tobytes("raw", "RGBA"), iw, ih, iw * 4,
                       QImage.Format.Format_RGBA8888).copy()
        painter.drawImage(disp, qimg)

        crop_rect = self._crop_rect_display()
        if crop_rect.isEmpty():
            return

        # Dim the area outside the crop so the user sees what would be discarded.
        dim = QColor(0, 0, 0, 140)
        painter.fillRect(QRect(disp.x(), disp.y(), disp.width(), crop_rect.y() - disp.y()), dim)
        painter.fillRect(QRect(disp.x(), crop_rect.bottom() + 1, disp.width(),
                                disp.bottom() - crop_rect.bottom()), dim)
        painter.fillRect(QRect(disp.x(), crop_rect.y(), crop_rect.x() - disp.x(),
                                crop_rect.height()), dim)
        painter.fillRect(QRect(crop_rect.right() + 1, crop_rect.y(),
                                disp.right() - crop_rect.right(), crop_rect.height()), dim)

        # Crop outline.
        painter.setPen(QPen(QColor(255, 220, 80), 2))
        painter.drawRect(crop_rect)

    # ── interaction ──────────────────────────────────────────────────────

    def mousePressEvent(self, event):
        if self._image is None or event.button() != Qt.MouseButton.LeftButton:
            return
        crop_rect = self._crop_rect_display()
        if crop_rect.contains(event.position().toPoint()):
            self._drag_anchor = event.position().toPoint()
            self._drag_initial_norm = self._crop_norm
        else:
            # Click outside: re-center the rect on the click position (clamped).
            disp = self._display_rect()
            if disp.isEmpty():
                return
            cx_norm = (event.position().x() - disp.x()) / max(1, disp.width())
            cy_norm = (event.position().y() - disp.y()) / max(1, disp.height())
            _, _, w, h = self._crop_norm
            self._crop_norm = self._clamp(cx_norm - w / 2, cy_norm - h / 2, w, h)
            self._drag_anchor = event.position().toPoint()
            self._drag_initial_norm = self._crop_norm
            self.update()
            self.cropChanged.emit()

    def mouseMoveEvent(self, event):
        if self._drag_anchor is None or self._image is None:
            return
        disp = self._display_rect()
        if disp.isEmpty():
            return
        delta = event.position().toPoint() - self._drag_anchor
        dx_norm = delta.x() / max(1, disp.width())
        dy_norm = delta.y() / max(1, disp.height())
        x0, y0, w, h = self._drag_initial_norm
        self._crop_norm = self._clamp(x0 + dx_norm, y0 + dy_norm, w, h)
        self.update()
        self.cropChanged.emit()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_anchor = None

    def wheelEvent(self, event):
        if self._image is None:
            return
        # Scroll up grows the rect, scroll down shrinks. Step ~10% per notch.
        notches = event.angleDelta().y() / 120.0
        if notches == 0:
            return
        scale = 1.0 + 0.1 * notches
        x, y, w, h = self._crop_norm
        cx, cy = x + w / 2, y + h / 2
        new_w = w * scale
        new_h = h * scale
        # Anchor on the cursor so the user can zoom into a specific feature.
        disp = self._display_rect()
        if not disp.isEmpty():
            cx = (event.position().x() - disp.x()) / max(1, disp.width())
            cy = (event.position().y() - disp.y()) / max(1, disp.height())
        self._crop_norm = self._clamp(cx - new_w / 2, cy - new_h / 2, new_w, new_h)
        self.update()
        self.cropChanged.emit()


# ── Panel ────────────────────────────────────────────────────────────────────


class HeroPortraitPanel(QWidget):
    """Hero portrait panel — Gemini repaint for the big portrait, visual slice
    picker for the small variant."""

    reference_changed = Signal(str)
    portrait_accepted = Signal(str)  # emits hero name when a new PNG is saved

    def __init__(self, parent=None):
        super().__init__(parent)

        self._config: HeroConfig | None = None
        self._target: str = TARGET_BIG

        self._base_image: Image.Image | None = None  # PORT0xxx for big target
        self._base_size: tuple[int, int] = (0, 0)
        self._upscaled_input: Image.Image | None = None

        self._output_image: Image.Image | None = None
        self._output_is_unsaved: bool = False

        self._reference_image: Image.Image | None = None
        self._reference_path: str = ""
        self._cost_tracker = CostTracker.load()
        self._worker: HeroPortraitWorker | None = None

        self._small_aspect: float = 1.0  # filled from MINIPORT[port_index]

        self._setup_ui()

        self._base_thumb.installEventFilter(self)
        self._out_thumb.installEventFilter(self)
        self._ref_thumb.installEventFilter(self)

    # ── Qt event handling ─────────────────────────────────────────────────────

    def eventFilter(self, obj, event):
        if event.type() == QEvent.Type.Resize:
            if obj is self._base_thumb and self._base_image is not None:
                self._base_thumb.setPixmap(self._fit_to_label(self._base_image, self._base_thumb))
            elif obj is self._out_thumb and self._output_image is not None:
                self._out_thumb.setPixmap(self._fit_to_label(self._output_image, self._out_thumb))
            elif obj is self._ref_thumb and self._reference_image is not None:
                self._ref_thumb.setPixmap(self._fit_to_label(self._reference_image, self._ref_thumb))
        return super().eventFilter(obj, event)

    @staticmethod
    def _fit_to_label(img: Image.Image, label: QLabel) -> QPixmap:
        w = max(64, label.width() - 8)
        h = max(64, label.height() - 8)
        return _pil_to_qpixmap(img, w, h)

    # ── UI ────────────────────────────────────────────────────────────────────

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Header: hero label + target selector
        header = QHBoxLayout()
        self._hero_label = QLabel("No hero selected")
        self._hero_label.setStyleSheet("font-weight: bold;")
        header.addWidget(self._hero_label, stretch=1)

        header.addWidget(QLabel("Target:"))
        self._target_combo = QComboBox()
        self._target_combo.addItem("Big Portrait (PORT0xxx)", TARGET_BIG)
        self._target_combo.addItem("Mini Portrait (slice from big)", TARGET_SMALL)
        self._target_combo.currentIndexChanged.connect(self._on_target_changed)
        header.addWidget(self._target_combo)

        layout.addLayout(header)

        # Stacked previews — Big shows base image; Small shows the slicer.
        self._preview_stack = QStackedWidget()

        # Big preview: side-by-side input/output thumbnails.
        big_split = QSplitter(Qt.Orientation.Horizontal)

        base_group = QGroupBox("Original PORT (input)")
        base_outer = QVBoxLayout(base_group)
        base_outer.setContentsMargins(6, 6, 6, 6)
        self._base_thumb = QLabel("Select a hero in the Heroes tab.")
        self._base_thumb.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._base_thumb.setMinimumHeight(180)
        self._base_thumb.setStyleSheet("background: #2a2a2a; color: #888;")
        self._base_thumb.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        base_outer.addWidget(self._base_thumb)
        big_split.addWidget(base_group)

        out_group = QGroupBox("Output (saved or Gemini)")
        out_outer = QVBoxLayout(out_group)
        out_outer.setContentsMargins(6, 6, 6, 6)
        self._out_thumb = QLabel("No output yet")
        self._out_thumb.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._out_thumb.setMinimumHeight(180)
        self._out_thumb.setStyleSheet("background: #2a2a2a; color: #888;")
        self._out_thumb.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        out_outer.addWidget(self._out_thumb)
        self._output_caption = QLabel("")
        self._output_caption.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._output_caption.setStyleSheet("color: #aaa;")
        out_outer.addWidget(self._output_caption)
        big_split.addWidget(out_group)
        big_split.setSizes([400, 400])
        self._preview_stack.addWidget(big_split)

        # Small preview: aspect-locked slice canvas + slice preview.
        small_split = QSplitter(Qt.Orientation.Horizontal)
        slicer_group = QGroupBox("Big portrait — drag to position, scroll to resize")
        slicer_outer = QVBoxLayout(slicer_group)
        slicer_outer.setContentsMargins(6, 6, 6, 6)
        self._slice_canvas = _SliceCanvas()
        self._slice_canvas.cropChanged.connect(self._refresh_slice_preview)
        slicer_outer.addWidget(self._slice_canvas, stretch=1)
        small_split.addWidget(slicer_group)

        slice_preview_group = QGroupBox("Mini portrait preview")
        slice_preview_outer = QVBoxLayout(slice_preview_group)
        slice_preview_outer.setContentsMargins(6, 6, 6, 6)
        self._slice_preview = QLabel("Generate the big portrait first.")
        self._slice_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._slice_preview.setMinimumHeight(180)
        self._slice_preview.setStyleSheet("background: #2a2a2a; color: #888;")
        self._slice_preview.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        slice_preview_outer.addWidget(self._slice_preview, stretch=1)
        self._slice_caption = QLabel("")
        self._slice_caption.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._slice_caption.setStyleSheet("color: #aaa;")
        slice_preview_outer.addWidget(self._slice_caption)
        small_split.addWidget(slice_preview_group)
        small_split.setSizes([500, 300])
        self._slice_preview.installEventFilter(self)
        self._preview_stack.addWidget(small_split)

        layout.addWidget(self._preview_stack, stretch=1)

        # Action buttons (shared)
        action_layout = QHBoxLayout()
        self._send_btn = QPushButton("Generate")
        self._send_btn.clicked.connect(self._send_to_gemini)
        self._send_btn.setEnabled(False)
        action_layout.addWidget(self._send_btn)

        self._cancel_btn = QPushButton("Cancel")
        self._cancel_btn.setToolTip(
            "Abort the current generation. The HTTP request itself completes in the "
            "background, but retry-backoff sleeps abort within 0.5s and the result is discarded."
        )
        self._cancel_btn.clicked.connect(self._cancel_generation)
        self._cancel_btn.setEnabled(False)
        action_layout.addWidget(self._cancel_btn)

        self._accept_btn = QPushButton("Accept (save PNG)")
        self._accept_btn.setToolTip(
            "Save the PNG, mark the hero as has_custom_sprites=true, and update the\n"
            "RGBA registry. Rebuild the engine to see the new portrait in-game."
        )
        self._accept_btn.clicked.connect(self._accept)
        self._accept_btn.setEnabled(False)
        action_layout.addWidget(self._accept_btn)

        self._reject_btn = QPushButton("Reject")
        self._reject_btn.clicked.connect(self._reject)
        self._reject_btn.setEnabled(False)
        action_layout.addWidget(self._reject_btn)

        self._delete_btn = QPushButton("Delete saved")
        self._delete_btn.setToolTip(
            "Remove the saved PNG and clear has_custom_sprites. Engine falls back to PORT00xx."
        )
        self._delete_btn.clicked.connect(self._delete_saved)
        self._delete_btn.setEnabled(False)
        action_layout.addWidget(self._delete_btn)

        action_layout.addStretch()

        self._cost_label = QLabel(f"Cost: {self._cost_tracker.summary()}")
        action_layout.addWidget(self._cost_label)

        layout.addLayout(action_layout)

        # Settings row (Gemini-only — hidden when target=small)
        self._settings_widget = QWidget()
        settings_layout = QHBoxLayout(self._settings_widget)
        settings_layout.setContentsMargins(0, 0, 0, 0)

        settings_layout.addWidget(QLabel("Model:"))
        self._model_combo = QComboBox()
        self._model_combo.addItems([
            "gemini-3-pro-image-preview",
            "gemini-3.1-flash-image-preview",
            "gemini-2.5-flash-image",
        ])
        self._model_combo.setCurrentIndex(0)
        settings_layout.addWidget(self._model_combo)

        settings_layout.addWidget(QLabel("Upscale:"))
        self._upscale_spin = QSpinBox()
        self._upscale_spin.setRange(1, 8)
        self._upscale_spin.setValue(4)
        self._upscale_spin.setToolTip(
            "Upscale factor for the input sent to Gemini. The output is saved at this "
            "high resolution; the engine's RGBA pipeline downscales to game resolution at draw time."
        )
        settings_layout.addWidget(self._upscale_spin)

        settings_layout.addStretch()
        layout.addWidget(self._settings_widget)

        # Prompt — collapsible (Gemini-only)
        self._prompt_group = QGroupBox("Prompt")
        self._prompt_group.setCheckable(True)
        self._prompt_group.setChecked(True)
        prompt_outer = QVBoxLayout(self._prompt_group)
        prompt_outer.setContentsMargins(6, 6, 6, 6)

        self._prompt_body = QWidget()
        prompt_body_layout = QFormLayout(self._prompt_body)
        prompt_body_layout.setContentsMargins(0, 0, 0, 0)

        self._prompt_edit = QTextEdit()
        self._prompt_edit.setMaximumHeight(72)
        prompt_body_layout.addRow("Transform:", self._prompt_edit)

        self._system_edit = QTextEdit()
        self._system_edit.setMaximumHeight(96)
        prompt_body_layout.addRow("System:", self._system_edit)

        prompt_outer.addWidget(self._prompt_body)
        self._prompt_group.toggled.connect(self._prompt_body.setVisible)
        layout.addWidget(self._prompt_group)

        # Reference image — collapsible AND a hard on/off switch (Gemini-only).
        self._ref_group = QGroupBox("Reference image (uncheck to disable)")
        self._ref_group.setCheckable(True)
        self._ref_group.setChecked(False)
        ref_outer = QVBoxLayout(self._ref_group)
        ref_outer.setContentsMargins(6, 6, 6, 6)

        self._ref_body = QWidget()
        self._ref_body.setVisible(False)
        ref_body_layout = QVBoxLayout(self._ref_body)
        ref_body_layout.setContentsMargins(0, 0, 0, 0)

        self._ref_thumb = QLabel("No reference image loaded")
        self._ref_thumb.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._ref_thumb.setMinimumHeight(100)
        self._ref_thumb.setStyleSheet("background: #2a2a2a; color: #888;")
        ref_body_layout.addWidget(self._ref_thumb)

        ref_row = QHBoxLayout()
        self._ref_label = QLabel("None")
        ref_browse_btn = QPushButton("Browse...")
        ref_browse_btn.setFixedWidth(80)
        ref_browse_btn.clicked.connect(self._browse_reference)
        ref_clear_btn = QPushButton("Clear")
        ref_clear_btn.setFixedWidth(60)
        ref_clear_btn.clicked.connect(self._clear_reference)
        ref_row.addWidget(self._ref_label, stretch=1)
        ref_row.addWidget(ref_browse_btn)
        ref_row.addWidget(ref_clear_btn)
        ref_body_layout.addLayout(ref_row)

        ref_outer.addWidget(self._ref_body)
        self._ref_group.toggled.connect(self._ref_body.setVisible)
        layout.addWidget(self._ref_group)

        # Status bar
        self._status_label = QLabel("")
        layout.addWidget(self._status_label)

        # Apply default prompts so they're populated even when no hero is bound.
        self._apply_default_prompts()
        self._apply_target_visibility()

    # ── public API ────────────────────────────────────────────────────────────

    def set_hero(self, config: HeroConfig | None):
        """Bind the panel to a hero and reload the base + saved PNG (if any)."""
        self._persist_current_prompt()
        self._config = config
        self._reset_output_state()

        if config is None:
            self._hero_label.setText("No hero selected")
            self._base_thumb.clear()
            self._base_thumb.setText("Select a hero in the Heroes tab.")
            self._send_btn.setEnabled(False)
            self._slice_canvas.set_image(None)
            return

        self._refresh_for_target()

    def _on_target_changed(self, _idx: int):
        self._persist_current_prompt()
        self._target = self._target_combo.currentData() or TARGET_BIG
        self._reset_output_state()
        self._apply_target_visibility()
        self._refresh_for_target()

    def _refresh_for_target(self):
        """Reload everything that depends on the (config, target) pair."""
        self._apply_prompt_for_hero()
        self._update_hero_label()
        if self._target == TARGET_BIG:
            self._load_base_big()
            self._load_saved_big()
        else:
            self._load_slice_source()
            self._load_saved_small()

    def _apply_target_visibility(self):
        """Show Gemini controls only when target=big; show slicer in small mode."""
        is_big = (self._target == TARGET_BIG)
        self._preview_stack.setCurrentIndex(0 if is_big else 1)
        self._send_btn.setVisible(is_big)
        self._cancel_btn.setVisible(is_big)
        self._settings_widget.setVisible(is_big)
        self._prompt_group.setVisible(is_big)
        self._ref_group.setVisible(is_big)

    def load_reference_image(self, path: str) -> bool:
        try:
            image = Image.open(path).convert("RGB")
        except (FileNotFoundError, OSError):
            self._clear_reference_state()
            return False
        self._reference_image = image
        self._reference_path = path
        self._ref_label.setText(f"{Path(path).name} ({image.width}x{image.height})")
        self._ref_thumb.setPixmap(self._fit_to_label(image, self._ref_thumb))
        return True

    # ── prompt management ────────────────────────────────────────────────────

    def _apply_default_prompts(self):
        self._prompt_edit.setPlainText(_DEFAULT_TRANSFORM_PROMPT)
        self._system_edit.setPlainText(_DEFAULT_SYSTEM_PROMPT)

    def _apply_prompt_for_hero(self):
        cfg = self._config
        if cfg is not None and cfg.portrait_prompt:
            self._prompt_edit.setPlainText(cfg.portrait_prompt)
        else:
            self._apply_default_prompts()

    def _persist_current_prompt(self):
        cfg = self._config
        if cfg is None or self._target != TARGET_BIG:
            return
        prompt = self._prompt_edit.toPlainText()
        if prompt == cfg.portrait_prompt:
            return
        cfg.portrait_prompt = prompt
        try:
            update_hero_manifest_entry(cfg.name, {"portrait_prompt": prompt})
        except OSError as e:
            self._status_label.setText(f"Could not save prompt: {e}")

    def _update_hero_label(self):
        cfg = self._config
        if cfg is None:
            self._hero_label.setText("No hero selected")
            return
        out_filename = self._output_filename()
        base_label = "MINIPORT slice" if self._target == TARGET_SMALL else f"PORT{cfg.port_index:04d}"
        self._hero_label.setText(
            f"{cfg.display_name}  |  {base_label}  |  output: files/data/sprites/{out_filename}"
        )

    # ── filename helpers ────────────────────────────────────────────────────

    def _output_filename(self) -> str:
        cfg = self._config
        if cfg is None:
            return ""
        if self._target == TARGET_SMALL:
            return f"{cfg.prefix}_small_000.png"
        return f"{cfg.prefix}_000.png"

    def _output_path(self) -> Path | None:
        cfg = self._config
        if cfg is None:
            return None
        return _sprites_dir() / self._output_filename()

    def _big_portrait_path(self) -> Path | None:
        cfg = self._config
        if cfg is None:
            return None
        return _sprites_dir() / f"{cfg.prefix}_000.png"

    # ── big target: base & saved loading ─────────────────────────────────────

    def _load_base_big(self):
        """Extract the PORT0xxx frame for the current hero."""
        from ..tools.icn_extractor import extract_icn

        cfg = self._config
        if cfg is None:
            return

        icn_name = f"PORT{cfg.port_index:04d}"
        self._status_label.setText(f"Extracting {icn_name} from AGG...")
        self.repaint()

        sprites = extract_icn(icn_name)
        if sprites is None or not sprites.frames:
            self._status_label.setText(f"Failed to extract {icn_name}.")
            self._send_btn.setEnabled(False)
            return

        frame = sprites.frames[0].image.convert("RGBA")
        self._base_image = frame
        self._base_size = (frame.width, frame.height)

        self._base_thumb.setPixmap(self._fit_to_label(frame, self._base_thumb))
        self._send_btn.setEnabled(True)
        self._status_label.setText(
            f"Loaded {icn_name} ({self._base_size[0]}x{self._base_size[1]})."
        )

    def _load_saved_big(self):
        path = self._output_path()
        if path is None or not path.exists():
            self._output_image = None
            self._output_is_unsaved = False
            self._out_thumb.clear()
            self._out_thumb.setText("No output yet")
            self._output_caption.setText("")
            self._delete_btn.setEnabled(False)
            return

        try:
            img = Image.open(path).convert("RGBA")
        except (FileNotFoundError, OSError) as e:
            self._status_label.setText(f"Could not load saved PNG: {e}")
            return

        self._output_image = img
        self._output_is_unsaved = False
        self._out_thumb.setPixmap(self._fit_to_label(img, self._out_thumb))
        self._output_caption.setText(
            f"Currently saved: {path.name}  ({img.width}x{img.height})"
        )
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(True)

    # ── small target: slicer ─────────────────────────────────────────────────

    def _load_slice_source(self):
        """Load the saved big portrait into the slice canvas, plus MINIPORT
        aspect ratio so the rect locks to the right shape."""
        cfg = self._config
        if cfg is None:
            return

        # Pull the MINIPORT aspect ratio so the crop rect matches the slot.
        aspect = self._fetch_miniport_aspect(cfg.port_index)
        self._small_aspect = aspect
        self._slice_canvas.set_aspect(aspect)

        big_path = self._big_portrait_path()
        if big_path is None or not big_path.exists():
            self._slice_canvas.set_image(None)
            self._status_label.setText(
                "Big portrait PNG not found. Switch to 'Big Portrait' and Generate + Accept first."
            )
            self._accept_btn.setEnabled(False)
            self._reject_btn.setEnabled(False)
            return

        try:
            img = Image.open(big_path).convert("RGBA")
        except (FileNotFoundError, OSError) as e:
            self._slice_canvas.set_image(None)
            self._status_label.setText(f"Could not load big portrait: {e}")
            return

        self._slice_canvas.set_image(img)
        self._status_label.setText(
            f"Loaded {big_path.name} ({img.width}x{img.height}). Drag to position, scroll to resize."
        )
        # Slicing produces an unsaved candidate immediately — enable Accept.
        self._accept_btn.setEnabled(True)
        self._reject_btn.setEnabled(False)

    def _load_saved_small(self):
        """Update preview caption / Delete button based on whether a small PNG exists."""
        path = self._output_path()
        if path is None or not path.exists():
            self._slice_caption.setText("")
            self._delete_btn.setEnabled(False)
            return
        try:
            img = Image.open(path).convert("RGBA")
        except (FileNotFoundError, OSError):
            self._slice_caption.setText("")
            self._delete_btn.setEnabled(False)
            return
        self._slice_caption.setText(
            f"Currently saved: {path.name}  ({img.width}x{img.height})"
        )
        self._delete_btn.setEnabled(True)

    def _fetch_miniport_aspect(self, port_index: int) -> float:
        from ..tools.icn_extractor import extract_icn
        try:
            mp = extract_icn("MINIPORT")
        except Exception:
            return 1.0
        if mp is None or not mp.frames or port_index >= len(mp.frames):
            return 1.0
        frame = mp.frames[port_index].image
        if frame.width <= 0 or frame.height <= 0:
            return 1.0
        return frame.width / frame.height

    def _refresh_slice_preview(self):
        cropped = self._slice_canvas.cropped_image()
        if cropped is None:
            self._slice_preview.clear()
            self._slice_preview.setText("Generate the big portrait first.")
            self._accept_btn.setEnabled(False)
            return
        self._slice_preview.setPixmap(self._fit_to_label(cropped, self._slice_preview))
        # Record this as the unsaved candidate so Accept can save it without
        # re-cropping. (cropped_image returns a fresh Image each call.)
        self._output_image = cropped
        self._output_is_unsaved = True
        self._accept_btn.setEnabled(True)

    def _reset_output_state(self):
        self._output_image = None
        self._output_is_unsaved = False
        self._upscaled_input = None
        self._out_thumb.clear()
        self._out_thumb.setText("No output yet")
        self._output_caption.setText("")
        self._slice_caption.setText("")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(False)

    # ── reference image ──────────────────────────────────────────────────────

    def _browse_reference(self):
        start_dir = str(Path(self._reference_path).parent) if self._reference_path else ""
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Reference Image", start_dir,
            "Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)",
        )
        if path:
            self.load_reference_image(path)
            self.reference_changed.emit(path)

    def _clear_reference(self):
        self._clear_reference_state()
        self.reference_changed.emit("")

    def _clear_reference_state(self):
        self._reference_image = None
        self._reference_path = ""
        self._ref_label.setText("None")
        self._ref_thumb.clear()
        self._ref_thumb.setText("No reference image loaded")

    # ── Gemini generation flow (big target only) ─────────────────────────────

    def _send_to_gemini(self):
        if self._target != TARGET_BIG:
            return
        if self._base_image is None or self._config is None:
            return

        prompt = self._prompt_edit.toPlainText().strip()
        if not prompt:
            QMessageBox.warning(self, "No Prompt", "Enter a transform prompt.")
            return

        self._persist_current_prompt()

        # Original PORT0xxx art is fully opaque, so RGB conversion is enough —
        # no BG keying or compositing needed (alpha 255 throughout).
        upscale = self._upscale_spin.value()
        rgb = self._base_image.convert("RGB")
        upscaled = rgb.resize(
            (rgb.width * upscale, rgb.height * upscale),
            Image.Resampling.NEAREST,
        )
        self._upscaled_input = upscaled

        system = self._system_edit.toPlainText().strip() or None
        model = self._model_combo.currentText()

        try:
            client = GeminiClient(model=model)
        except ValueError as e:
            QMessageBox.critical(self, "API Key Error", str(e))
            return

        self._send_btn.setEnabled(False)
        self._cancel_btn.setEnabled(True)
        self._status_label.setText("Sending to Gemini...")

        reference = self._reference_image if self._ref_group.isChecked() else None
        self._worker = HeroPortraitWorker(client, upscaled, prompt, system, reference)
        self._worker.finished.connect(self._on_result)
        self._worker.error.connect(self._on_error)
        self._worker.start()

    def _cancel_generation(self):
        if self._worker is not None and self._worker.isRunning():
            self._worker.cancel()
            self._cancel_btn.setEnabled(False)
            self._status_label.setText("Cancelling…")

    def _on_result(self, result: Image.Image | None):
        self._send_btn.setEnabled(True)
        self._cancel_btn.setEnabled(False)

        if result is None:
            self._status_label.setText("Gemini returned no image")
            self._cost_tracker.log_call(
                self._model_combo.currentText(), [0],
                self._upscaled_input.size if self._upscaled_input else (0, 0),
                success=False,
            )
            self._cost_label.setText(f"Cost: {self._cost_tracker.summary()}")
            return

        self._cost_tracker.log_call(
            self._model_combo.currentText(), [0],
            self._upscaled_input.size if self._upscaled_input else (0, 0),
            result.size,
        )
        self._cost_label.setText(f"Cost: {self._cost_tracker.summary()}")

        # No background keying — Gemini repaints the entire frame including the
        # backdrop, and the engine composites the full RGBA on top of the
        # indexed PORT slot. Transparent input would have shown through; opaque
        # input means alpha=255 is the right answer everywhere.
        keyed = result.convert("RGBA")
        self._output_image = keyed
        self._output_is_unsaved = True

        self._out_thumb.setPixmap(self._fit_to_label(self._output_image, self._out_thumb))
        self._output_caption.setText(
            f"Fresh Gemini output ({keyed.width}x{keyed.height}) — Accept to save."
        )
        self._accept_btn.setEnabled(True)
        self._reject_btn.setEnabled(True)
        self._status_label.setText(
            f"Output {keyed.width}x{keyed.height}. Click Accept to save."
        )

    def _on_error(self, error_msg: str):
        self._send_btn.setEnabled(True)
        self._cancel_btn.setEnabled(False)
        self._status_label.setText(f"Error: {error_msg}")
        if error_msg != "Cancelled by user":
            QMessageBox.critical(self, "Gemini Error", error_msg)

    # ── accept / reject / delete ──────────────────────────────────────────────

    def _accept(self):
        cfg = self._config
        if self._output_image is None or cfg is None:
            return
        # Big-target accept requires unsaved fresh Gemini output.
        # Small-target accept saves the current slice every time (cheap, no API call).
        if self._target == TARGET_BIG and not self._output_is_unsaved:
            return

        out_path = self._output_path()
        if out_path is None:
            return
        out_path.parent.mkdir(parents=True, exist_ok=True)
        self._output_image.save(out_path)

        flag_name = "has_custom_sprites_small" if self._target == TARGET_SMALL else "has_custom_sprites"
        if self._target == TARGET_SMALL:
            cfg.has_custom_sprites_small = True
        else:
            cfg.has_custom_sprites = True
        try:
            update_hero_manifest_entry(cfg.name, {flag_name: True})
        except OSError as e:
            self._status_label.setText(f"Saved PNG but could not update manifest: {e}")
            return

        self._output_is_unsaved = False
        if self._target == TARGET_SMALL:
            self._slice_caption.setText(f"Currently saved: {out_path.name}")
        else:
            self._output_caption.setText(f"Currently saved: {out_path.name}")
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(True)
        self._status_label.setText(
            f"Saved {out_path.name}. {flag_name}=true. Re-export specialties + rebuild."
        )
        self.portrait_accepted.emit(cfg.name)

    def _reject(self):
        if self._target == TARGET_BIG:
            self._output_image = None
            self._output_is_unsaved = False
            self._out_thumb.clear()
            self._out_thumb.setText("No output yet")
            self._output_caption.setText("")
            self._accept_btn.setEnabled(False)
            self._reject_btn.setEnabled(False)
            self._load_saved_big()

    def _delete_saved(self):
        path = self._output_path()
        cfg = self._config
        if path is None or not path.exists() or cfg is None:
            return
        flag_name = "has_custom_sprites_small" if self._target == TARGET_SMALL else "has_custom_sprites"
        fallback_msg = (
            "The engine will fall back to the cropped big portrait, then the palette MINIPORT."
            if self._target == TARGET_SMALL
            else f"The engine will fall back to the original PORT{cfg.port_index:04d} portrait."
        )
        confirm = QMessageBox.question(
            self, "Delete saved PNG",
            f"Delete {path.name} and clear {flag_name}?\n\n{fallback_msg}",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if confirm != QMessageBox.StandardButton.Yes:
            return
        try:
            path.unlink()
        except OSError as e:
            QMessageBox.critical(self, "Delete failed", str(e))
            return

        if self._target == TARGET_SMALL:
            cfg.has_custom_sprites_small = False
        else:
            cfg.has_custom_sprites = False
        try:
            update_hero_manifest_entry(cfg.name, {flag_name: False})
        except OSError as e:
            self._status_label.setText(f"Deleted PNG but could not update manifest: {e}")
            return

        self._status_label.setText(f"Deleted {path.name}, cleared {flag_name}.")
        self._reset_output_state()
        if self._target == TARGET_SMALL:
            self._refresh_slice_preview()
        self.portrait_accepted.emit(cfg.name)
