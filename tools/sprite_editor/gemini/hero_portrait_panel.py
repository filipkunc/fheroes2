"""Hero portrait generation panel — single-sprite Gemini workflow for hi-res hero art.

Bound to the same HeroConfig the Heroes tab edits. Pulls the original PORT00xx
portrait out of HEROES2[X].AGG, sends an upscaled version to Gemini with a
hero-flavoured prompt, and saves the keyed RGBA result at
files/data/sprites/{prefix}_000.png so the engine's RGBA pipeline picks it up.

On accept the manifest's has_custom_sprites flips to true and the next codegen
export adds this hero to heroes_specialty_rgba.inl — at which point the engine
overlays the new portrait everywhere a hero portrait shows up.
"""

from pathlib import Path

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QFormLayout,
    QTextEdit, QComboBox, QSpinBox, QPushButton, QLabel,
    QFileDialog, QMessageBox, QColorDialog, QSplitter, QSizePolicy,
)
from PySide6.QtCore import Qt, QThread, Signal, QEvent
from PySide6.QtGui import QImage, QPixmap, QColor

from PIL import Image

from ..models.hero_config import HeroConfig, update_hero_manifest_entry
from .gemini_client import GeminiClient, _replace_bg_with_color
from .cost_tracker import CostTracker


# Target identifiers — each maps to a different (input ICN, output filename, prompts) triple.
TARGET_BIG = "big"      # PORT0xxx → hero_<name>_000.png (full-frame portrait)
TARGET_SMALL = "small"  # MINIPORT[port_index] → hero_<name>_small_000.png (mini variant)


_DEFAULT_TRANSFORM_PROMPT = (
    "Repaint this hero portrait at high resolution while keeping the EXACT pose, framing, "
    "facial features, hair, clothing, and silhouette of the original. Sharpen details and "
    "add believable texture (skin, fabric, metal, hair strands). Do NOT change the head "
    "position, body angle, expression, or background composition. Improve the quality and "
    "realism."
)

_DEFAULT_SYSTEM_PROMPT = (
    "You are a high-resolution painter for fantasy game character portraits. You receive "
    "a small low-resolution portrait on a solid coloured background and must output a "
    "high-resolution version that preserves the subject's identity exactly — same face, "
    "same pose, same outfit, same composition. CRITICAL: do not crop, reframe, mirror, "
    "or change the camera angle. Keep the exact background colour in pixels not covered "
    "by the figure. Aim for painterly fantasy-portrait realism — sharp eyes, natural skin "
    "tones, fabric and metal detail — without leaving the original silhouette."
)

_DEFAULT_SMALL_TRANSFORM_PROMPT = (
    "Repaint this tiny hero icon at high resolution. Keep the EXACT face, hair, headwear, "
    "and shoulder framing of the original — this is a mini portrait icon used in game UI, "
    "so the framing must match. Sharpen details. Do NOT zoom out, reframe, change the head "
    "position, or alter the background composition. Improve the quality and realism."
)

_DEFAULT_SMALL_SYSTEM_PROMPT = (
    "You are a high-resolution painter for fantasy game mini portrait icons. You receive a "
    "very small low-resolution headshot on a solid coloured background. Output a high-"
    "resolution version that preserves the subject's identity exactly — same face, same "
    "framing, same shoulders, same headwear. CRITICAL: do not zoom out, do not add body "
    "below the shoulders, do not reframe. Keep the exact background colour in pixels not "
    "covered by the figure. Aim for painterly fantasy-portrait realism."
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
            # No auto-retry — surface failures immediately so the user can read
            # the underlying error and decide (different model, wait, etc.).
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
            # Surface the underlying cause if there is one — Gemini 503s often
            # chain a more informative response body that the wrapper hides.
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


class HeroPortraitPanel(QWidget):
    """Gemini portrait panel for the currently selected hero."""

    bg_color_changed = Signal(tuple)
    reference_changed = Signal(str)
    portrait_accepted = Signal(str)  # emits hero name when a new PNG is saved

    def __init__(self, parent=None):
        super().__init__(parent)

        self._config: HeroConfig | None = None
        self._target: str = TARGET_BIG

        self._base_image: Image.Image | None = None
        self._base_size: tuple[int, int] = (0, 0)
        self._upscaled_input: Image.Image | None = None

        self._output_image: Image.Image | None = None
        self._output_is_unsaved: bool = False

        self._reference_image: Image.Image | None = None
        self._reference_path: str = ""
        self._cost_tracker = CostTracker.load()
        self._worker: HeroPortraitWorker | None = None
        self._bg_color: tuple[int, int, int] = (255, 0, 255)

        self._setup_ui()

        # Re-render previews when their labels resize so dragging the splitter
        # actually grows the images instead of leaving them at a fixed cap.
        self._base_thumb.installEventFilter(self)
        self._out_thumb.installEventFilter(self)
        self._ref_thumb.installEventFilter(self)

    # ── Qt event handling ─────────────────────────────────────────────────────

    def eventFilter(self, obj, event):
        if event.type() == QEvent.Type.Resize:
            if obj is self._base_thumb and self._base_image is not None:
                composited = _replace_bg_with_color(self._base_image, self._bg_color)
                self._base_thumb.setPixmap(self._fit_to_label(composited, self._base_thumb))
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
        self._target_combo.addItem("Mini Portrait (MINIPORT)", TARGET_SMALL)
        self._target_combo.currentIndexChanged.connect(self._on_target_changed)
        header.addWidget(self._target_combo)

        layout.addLayout(header)

        # Side-by-side previews
        preview_splitter = QSplitter(Qt.Orientation.Horizontal)

        base_group = QGroupBox("Original PORT (input)")
        base_outer = QVBoxLayout(base_group)
        base_outer.setContentsMargins(6, 6, 6, 6)
        self._base_thumb = QLabel("Select a hero in the Heroes tab.")
        self._base_thumb.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._base_thumb.setMinimumHeight(180)
        self._base_thumb.setStyleSheet("background: #2a2a2a; color: #888;")
        self._base_thumb.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        base_outer.addWidget(self._base_thumb)
        preview_splitter.addWidget(base_group)

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
        preview_splitter.addWidget(out_group)

        preview_splitter.setSizes([400, 400])
        layout.addWidget(preview_splitter, stretch=1)

        # Action buttons
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

        # Settings row
        settings_layout = QHBoxLayout()

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

        settings_layout.addWidget(QLabel("BG:"))
        self._bg_btn = QPushButton()
        self._bg_btn.setFixedWidth(30)
        self._bg_btn.setStyleSheet("background-color: #FF00FF;")
        self._bg_btn.setToolTip(
            "Background color for transparent pixels — Gemini sees this and the slicer keys it out."
        )
        self._bg_btn.clicked.connect(self._pick_bg_color)
        settings_layout.addWidget(self._bg_btn)

        settings_layout.addStretch()
        layout.addLayout(settings_layout)

        # Prompt — collapsible
        prompt_group = QGroupBox("Prompt")
        prompt_group.setCheckable(True)
        prompt_group.setChecked(True)
        prompt_outer = QVBoxLayout(prompt_group)
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
        prompt_group.toggled.connect(self._prompt_body.setVisible)
        layout.addWidget(prompt_group)

        # Reference image — collapsible AND a hard on/off switch. The group's
        # checked state is read at send time: if unchecked, the reference image
        # (even if loaded) is NOT sent to Gemini. This avoids a stale monster
        # reference silently bleeding into hero generation.
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
            return

        self._refresh_for_target()

    def _on_target_changed(self, _idx: int):
        # Persist the OUTGOING target's prompt before swapping.
        self._persist_current_prompt()
        self._target = self._target_combo.currentData() or TARGET_BIG
        self._reset_output_state()
        self._refresh_for_target()

    def _refresh_for_target(self):
        """Reload everything that depends on the (config, target) pair."""
        self._apply_prompt_for_hero()
        self._update_hero_label()
        self._load_base()
        self._load_saved()

    def set_bg_color(self, rgb: tuple[int, int, int]) -> None:
        self._bg_color = rgb
        self._bg_btn.setStyleSheet(f"background-color: rgb({rgb[0]},{rgb[1]},{rgb[2]});")

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
        if self._target == TARGET_SMALL:
            self._prompt_edit.setPlainText(_DEFAULT_SMALL_TRANSFORM_PROMPT)
            self._system_edit.setPlainText(_DEFAULT_SMALL_SYSTEM_PROMPT)
        else:
            self._prompt_edit.setPlainText(_DEFAULT_TRANSFORM_PROMPT)
            self._system_edit.setPlainText(_DEFAULT_SYSTEM_PROMPT)

    def _apply_prompt_for_hero(self):
        cfg = self._config
        saved = ""
        if cfg is not None:
            saved = cfg.portrait_prompt_small if self._target == TARGET_SMALL else cfg.portrait_prompt
        if saved:
            self._prompt_edit.setPlainText(saved)
        else:
            self._apply_default_prompts()

    def _persist_current_prompt(self):
        cfg = self._config
        if cfg is None:
            return
        prompt = self._prompt_edit.toPlainText()
        field_name = "portrait_prompt_small" if self._target == TARGET_SMALL else "portrait_prompt"
        existing = cfg.portrait_prompt_small if self._target == TARGET_SMALL else cfg.portrait_prompt
        if prompt == existing:
            return
        if self._target == TARGET_SMALL:
            cfg.portrait_prompt_small = prompt
        else:
            cfg.portrait_prompt = prompt
        try:
            update_hero_manifest_entry(cfg.name, {field_name: prompt})
        except OSError as e:
            self._status_label.setText(f"Could not save prompt: {e}")

    def _update_hero_label(self):
        cfg = self._config
        if cfg is None:
            self._hero_label.setText("No hero selected")
            return
        out_filename = self._output_filename()
        base_label = "MINIPORT" if self._target == TARGET_SMALL else f"PORT{cfg.port_index:04d}"
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

    # ── base & saved loading ──────────────────────────────────────────────────

    def _load_base(self):
        """Extract the input ICN frame for the current (hero, target)."""
        from ..tools.icn_extractor import extract_icn

        cfg = self._config
        if cfg is None:
            return

        if self._target == TARGET_SMALL:
            icn_name = "MINIPORT"
            frame_idx = cfg.port_index  # MINIPORT frames are 0-indexed by hero_id - 1, == port_index
            label = f"MINIPORT[{frame_idx}]"
        else:
            icn_name = f"PORT{cfg.port_index:04d}"
            frame_idx = 0
            label = icn_name

        self._status_label.setText(f"Extracting {label} from AGG...")
        self.repaint()

        sprites = extract_icn(icn_name)
        if sprites is None or not sprites.frames:
            self._status_label.setText(f"Failed to extract {icn_name}.")
            self._send_btn.setEnabled(False)
            return
        if frame_idx < 0 or frame_idx >= len(sprites.frames):
            self._status_label.setText(f"Frame {frame_idx} out of range for {icn_name}.")
            self._send_btn.setEnabled(False)
            return

        frame = sprites.frames[frame_idx].image.convert("RGBA")
        self._base_image = frame
        self._base_size = (frame.width, frame.height)

        composited = _replace_bg_with_color(frame, self._bg_color)
        self._base_thumb.setPixmap(self._fit_to_label(composited, self._base_thumb))
        self._send_btn.setEnabled(True)
        self._status_label.setText(
            f"Loaded {label} ({self._base_size[0]}x{self._base_size[1]})."
        )

    def _load_saved(self):
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

    def _reset_output_state(self):
        self._output_image = None
        self._output_is_unsaved = False
        self._upscaled_input = None
        self._out_thumb.clear()
        self._out_thumb.setText("No output yet")
        self._output_caption.setText("")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(False)

    # ── bg color & reference image ────────────────────────────────────────────

    def _pick_bg_color(self):
        r, g, b = self._bg_color
        color = QColorDialog.getColor(QColor(r, g, b), self, "Background Color")
        if color.isValid():
            rgb = (color.red(), color.green(), color.blue())
            self.set_bg_color(rgb)
            self.bg_color_changed.emit(rgb)
            if self._base_image is not None:
                composited = _replace_bg_with_color(self._base_image, self._bg_color)
                self._base_thumb.setPixmap(self._fit_to_label(composited, self._base_thumb))

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

    # ── generation flow ──────────────────────────────────────────────────────

    def _send_to_gemini(self):
        if self._base_image is None or self._config is None:
            return

        prompt = self._prompt_edit.toPlainText().strip()
        if not prompt:
            QMessageBox.warning(self, "No Prompt", "Enter a transform prompt.")
            return

        self._persist_current_prompt()

        upscale = self._upscale_spin.value()
        composited = _replace_bg_with_color(self._base_image, self._bg_color).convert("RGB")
        upscaled = composited.resize(
            (composited.width * upscale, composited.height * upscale),
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

        # Reference image is opt-in: only sent when the group is checked. A
        # collapsed/unchecked group means "do not include the reference" even if
        # one is loaded into _reference_image, which keeps stale references from
        # the monster panel out of hero generation.
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

        # Hero portraits stay at the upscaled resolution — the engine's RGBA
        # pipeline downscales at draw time. We just key the BG to alpha so the
        # PNG has clean transparency where Gemini left the magenta backdrop.
        keyed = self._key_bg_to_alpha(result.convert("RGBA"))
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

    def _key_bg_to_alpha(self, img: Image.Image) -> Image.Image:
        import numpy as np

        from .gemini_client import _compute_bg_mask

        pixels = np.array(img.convert("RGBA"))
        mask = _compute_bg_mask(pixels, self._bg_color)
        pixels[mask, 3] = 0
        return Image.fromarray(pixels)

    # ── accept / reject / delete ──────────────────────────────────────────────

    def _accept(self):
        if self._output_image is None or self._config is None or not self._output_is_unsaved:
            return
        out_path = self._output_path()
        if out_path is None:
            return
        out_path.parent.mkdir(parents=True, exist_ok=True)
        self._output_image.save(out_path)

        # Flip the right has_custom_sprites_* flag so the next codegen export
        # adds this hero to the matching RGBA registry. The main window listens
        # for the signal and runs the codegen automatically.
        cfg = self._config
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
        self._output_caption.setText(f"Currently saved: {out_path.name}")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(True)
        self._status_label.setText(
            f"Saved {out_path.name}. {flag_name}=true. Re-export specialties + rebuild."
        )
        self.portrait_accepted.emit(cfg.name)

    def _reject(self):
        self._output_image = None
        self._output_is_unsaved = False
        self._out_thumb.clear()
        self._out_thumb.setText("No output yet")
        self._output_caption.setText("")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._load_saved()

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
        # Notify the host to refresh the codegen so the hero is removed from the registry.
        self.portrait_accepted.emit(cfg.name)
