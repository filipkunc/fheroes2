"""Building art generation panel — single-sprite Gemini workflow for custom dwellings.

Supports two targets per monster:
- Full building (TWN*UP*A) saved as `files/data/sprites/{prefix}_building.png`,
  used by the live castle scene.
- Small construction-tile icon (CSTL*[index]) saved as
  `files/data/sprites/{prefix}_building_icon.png`, used by the build dialog and Well.

The two are separate AI generations because the original game ships them as
two hand-authored sprites at different sizes / framings — scaling one to the
other never matches the visual style of the other tiles in those screens.

When the panel is bound to a monster, any already-saved PNG for the current
target is loaded into the output preview so the user can see what's currently
shipped before deciding to regenerate.
"""

from pathlib import Path

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QFormLayout,
    QTextEdit, QComboBox, QSpinBox, QPushButton, QLabel,
    QFileDialog, QMessageBox, QColorDialog,
)
from PySide6.QtCore import Qt, QThread, Signal, QEvent
from PySide6.QtGui import QImage, QPixmap, QColor

from PIL import Image

from ..models.monster_config import MonsterConfig, update_manifest_entry
from ..widgets.busy_spinner import BusySpinner
from .gemini_client import GeminiClient, _replace_bg_with_color
from .cost_tracker import CostTracker


# Target identifiers used by the Target combo box and persistence helpers.
TARGET_BUILDING = "building"  # full TWN*UP*A sprite
TARGET_ICON = "icon"          # small CSTL*[index] tile


class BuildingWorker(QThread):
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
        # The Gemini SDK call itself is uninterruptible (it's a synchronous HTTP request),
        # but our retry-backoff sleeps poll this flag every 0.5s and will return early.
        self._cancelled = True

    def run(self):
        try:
            # Single attempt only — no auto-retry. If the model 503s the user can hit
            # Cancel and try again with a different model, instead of being locked into
            # 2 minutes of backoff sleeps.
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
            self.error.emit(str(e))


def _pil_to_qpixmap(img: Image.Image, max_w: int = 400, max_h: int = 400) -> QPixmap:
    rgba = img.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    qimg = QImage(data, rgba.width, rgba.height, rgba.width * 4, QImage.Format.Format_RGBA8888)
    pixmap = QPixmap.fromImage(qimg.copy())
    return pixmap.scaled(max_w, max_h, Qt.AspectRatioMode.KeepAspectRatio,
                         Qt.TransformationMode.SmoothTransformation)


def _sprites_dir() -> Path:
    return Path(__file__).parent.parent.parent.parent / "files" / "data" / "sprites"


class BuildingPanel(QWidget):
    """Two-target Gemini panel for custom dwelling art (building + icon)."""

    bg_color_changed = Signal(tuple)
    reference_changed = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)

        self._config: MonsterConfig | None = None
        self._target: str = TARGET_BUILDING

        self._base_image: Image.Image | None = None
        self._base_size: tuple[int, int] = (0, 0)
        self._upscaled_input: Image.Image | None = None

        # _output_image holds whatever the output preview is showing. It's either:
        #   (a) a fresh Gemini result waiting for Accept/Reject, OR
        #   (b) the on-disk saved PNG from a prior run (loaded for reference).
        # _output_is_unsaved distinguishes them so we only enable Accept when there's
        # something new to save.
        self._output_image: Image.Image | None = None
        self._output_is_unsaved: bool = False

        self._reference_image: Image.Image | None = None
        self._reference_path: str = ""
        self._cost_tracker = CostTracker.load()
        self._worker: BuildingWorker | None = None

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
        from PySide6.QtWidgets import QSplitter, QSizePolicy

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Header: monster label + Target combo
        header = QHBoxLayout()
        self._monster_label = QLabel("No monster selected")
        self._monster_label.setStyleSheet("font-weight: bold;")
        header.addWidget(self._monster_label, stretch=1)

        header.addWidget(QLabel("Target:"))
        self._target_combo = QComboBox()
        self._target_combo.addItem("Full Building (TWN*UP*A)", TARGET_BUILDING)
        self._target_combo.addItem("Small Icon (CSTL*[index])", TARGET_ICON)
        self._target_combo.currentIndexChanged.connect(self._on_target_changed)
        header.addWidget(self._target_combo)

        layout.addLayout(header)

        # Side-by-side previews
        preview_splitter = QSplitter(Qt.Orientation.Horizontal)

        base_group = QGroupBox("Base sprite (input)")
        base_outer = QVBoxLayout(base_group)
        base_outer.setContentsMargins(6, 6, 6, 6)
        self._base_thumb = QLabel("Select a custom monster with a building.")
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

        self._spinner = BusySpinner()
        action_layout.addWidget(self._spinner)

        self._cancel_btn = QPushButton("Cancel")
        self._cancel_btn.setToolTip("Abort the current generation. The HTTP request itself "
                                     "completes in the background, but retry-backoff sleeps "
                                     "abort within 0.5s and the result is discarded.")
        self._cancel_btn.clicked.connect(self._cancel_generation)
        self._cancel_btn.setEnabled(False)
        action_layout.addWidget(self._cancel_btn)

        self._accept_btn = QPushButton("Accept (save PNG)")
        self._accept_btn.clicked.connect(self._accept)
        self._accept_btn.setEnabled(False)
        action_layout.addWidget(self._accept_btn)

        self._reject_btn = QPushButton("Reject")
        self._reject_btn.clicked.connect(self._reject)
        self._reject_btn.setEnabled(False)
        action_layout.addWidget(self._reject_btn)

        self._delete_btn = QPushButton("Delete saved")
        self._delete_btn.setToolTip("Remove the saved PNG for the current target. Engine falls back to the base building.")
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
        # Identifiers per https://ai.google.dev/gemini-api/docs/models (2026):
        #   gemini-3-pro-image-preview      = "Nano Banana Pro" (highest quality, frequently 503s)
        #   gemini-3.1-flash-image-preview  = "Nano Banana 2" (Feb 2026 release, good middle ground)
        #   gemini-2.5-flash-image          = "Nano Banana" (most stable / lowest 5xx rate)
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
        self._upscale_spin.setToolTip("Upscale factor for the input sent to Gemini. The output is downscaled back to native size.")
        settings_layout.addWidget(self._upscale_spin)

        settings_layout.addWidget(QLabel("BG:"))
        self._bg_color = (255, 0, 255)
        self._bg_btn = QPushButton()
        self._bg_btn.setFixedWidth(30)
        self._bg_btn.setStyleSheet("background-color: #FF00FF;")
        self._bg_btn.setToolTip("Background color for transparent pixels — Gemini sees this and the slicer keys it out.")
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
        self._prompt_edit.setMaximumHeight(56)
        prompt_body_layout.addRow("Transform:", self._prompt_edit)

        self._system_edit = QTextEdit()
        self._system_edit.setMaximumHeight(80)
        prompt_body_layout.addRow("System:", self._system_edit)

        prompt_outer.addWidget(self._prompt_body)
        prompt_group.toggled.connect(self._prompt_body.setVisible)
        layout.addWidget(prompt_group)

        # Reference image — collapsible, default closed
        ref_group = QGroupBox("Reference image (sent to Gemini)")
        ref_group.setCheckable(True)
        ref_group.setChecked(False)
        ref_outer = QVBoxLayout(ref_group)
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
        ref_group.toggled.connect(self._ref_body.setVisible)
        layout.addWidget(ref_group)

        # Status bar
        self._status_label = QLabel("")
        layout.addWidget(self._status_label)

        # Apply default prompts so they're populated even when no monster is bound.
        self._apply_default_prompts_for_target()

    # ── public API ────────────────────────────────────────────────────────────

    def set_monster(self, config: MonsterConfig | None):
        """Bind the panel to a monster and reload base + saved PNG for the current target."""
        # Persist the prompt of the OUTGOING monster before swapping.
        self._persist_current_prompt()

        self._config = config
        self._reset_output_state()

        if config is None or not config.base_building_icn:
            self._monster_label.setText("No custom building configured for this monster.")
            self._base_thumb.clear()
            self._base_thumb.setText("Select a custom monster with a building.")
            self._send_btn.setEnabled(False)
            return

        # Disable the icon target if the manifest doesn't define an icon source.
        icon_supported = bool(config.base_building_icon_icn)
        # currentIndexChanged is connected, so block signals while we tweak items.
        self._target_combo.blockSignals(True)
        item_idx = self._target_combo.findData(TARGET_ICON)
        if item_idx >= 0:
            self._target_combo.model().item(item_idx).setEnabled(icon_supported)
        if not icon_supported and self._target == TARGET_ICON:
            self._target = TARGET_BUILDING
            self._target_combo.setCurrentIndex(self._target_combo.findData(TARGET_BUILDING))
        self._target_combo.blockSignals(False)

        self._refresh_for_target()

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

    # ── target switching ─────────────────────────────────────────────────────

    def _on_target_changed(self, _idx: int):
        # Save the prompt for the OUTGOING target before swapping.
        self._persist_current_prompt()
        self._target = self._target_combo.currentData() or TARGET_BUILDING
        self._reset_output_state()
        self._refresh_for_target()

    def _refresh_for_target(self):
        """Reload everything that depends on the (config, target) pair."""
        self._apply_prompt_for_target()
        self._update_monster_label()
        self._load_base_for_target()
        self._load_saved_for_target()

    def _apply_default_prompts_for_target(self):
        """Populate prompt/system with the appropriate default for the current target."""
        if self._target == TARGET_ICON:
            self._prompt_edit.setPlainText(
                "Repaint the building tile to match the new monster. Match the framing, "
                "perspective, and ground/sky context of the original tile. The building "
                "must sit on the same ground line at the bottom."
            )
            self._system_edit.setPlainText(
                "You are a pixel-art editor for game construction-tile icons. You receive a "
                "small building tile (sky, terrain, and building, all on a solid background "
                "outside the tile). Repaint the building per the prompt while preserving the "
                "tile's overall framing, the surrounding scenery (sky on top, ground on the "
                "bottom), the perspective, and the pixel-art style. CRITICAL: keep the same "
                "pixel dimensions and the exact background color in any pixels not covered "
                "by the tile content."
            )
        else:  # TARGET_BUILDING
            self._prompt_edit.setPlainText(
                "Transform the building to match the new monster. The building MUST fill the "
                "entire frame edge-to-edge — match the framing and footprint of the original. "
                "Keep the same ground level at the bottom and the same silhouette. No empty borders."
            )
            self._system_edit.setPlainText(
                "You are a pixel-art editor for game dwellings. You receive a single building "
                "tile on a solid colored background. Transform its appearance per the prompt "
                "while preserving the silhouette, ground footprint, perspective, and pixel-art "
                "style. CRITICAL: the new building MUST occupy the same pixel area as the "
                "original — same width, same height, and the building's base must sit on the "
                "SAME bottom row of pixels. Do NOT shrink, center, or float the building. Output "
                "the same pixel dimensions and keep the exact solid background color in any "
                "pixels not covered by the building."
            )

    def _apply_prompt_for_target(self):
        """Use the saved per-monster per-target prompt if set, else the default."""
        cfg = self._config
        saved_prompt = ""
        if cfg is not None:
            saved_prompt = cfg.building_icon_prompt if self._target == TARGET_ICON else cfg.building_prompt
        if saved_prompt:
            self._prompt_edit.setPlainText(saved_prompt)
        else:
            self._apply_default_prompts_for_target()

    def _persist_current_prompt(self):
        """Save the current prompt back to the manifest under the right field."""
        cfg = self._config
        if cfg is None:
            return
        prompt = self._prompt_edit.toPlainText()
        field = "building_icon_prompt" if self._target == TARGET_ICON else "building_prompt"
        existing = cfg.building_icon_prompt if self._target == TARGET_ICON else cfg.building_prompt
        if prompt == existing:
            return
        if self._target == TARGET_ICON:
            cfg.building_icon_prompt = prompt
        else:
            cfg.building_prompt = prompt
        try:
            update_manifest_entry(cfg.name, {field: prompt})
        except OSError as e:
            self._status_label.setText(f"Could not save prompt: {e}")

    def _update_monster_label(self):
        cfg = self._config
        if cfg is None:
            self._monster_label.setText("No monster selected")
            return
        out_filename = self._current_output_filename()
        base_icn = self._current_base_icn() or "(none)"
        idx_suffix = (f"[{cfg.base_building_icon_index}]"
                      if self._target == TARGET_ICON and cfg.base_building_icon_icn else "")
        self._monster_label.setText(
            f"{cfg.display_name or cfg.name}  |  base: {base_icn}{idx_suffix}  |  "
            f"output: files/data/sprites/{out_filename}"
        )

    # ── target-aware accessors ───────────────────────────────────────────────

    def _current_base_icn(self) -> str:
        cfg = self._config
        if cfg is None:
            return ""
        if self._target == TARGET_ICON:
            return cfg.base_building_icon_icn
        return cfg.base_building_icn

    def _current_base_index(self) -> int:
        cfg = self._config
        if cfg is None or self._target == TARGET_BUILDING:
            return 0
        return cfg.base_building_icon_index

    def _current_output_filename(self) -> str:
        cfg = self._config
        if cfg is None:
            return ""
        if self._target == TARGET_ICON:
            return f"{cfg.prefix}_building_icon.png"
        return f"{cfg.prefix}_building.png"

    def _current_output_path(self) -> Path | None:
        cfg = self._config
        if cfg is None:
            return None
        return _sprites_dir() / self._current_output_filename()

    # ── base & saved loading ──────────────────────────────────────────────────

    def _load_base_for_target(self):
        """Extract the base sprite from the AGG for the current (config, target)."""
        from ..tools.icn_extractor import extract_icn

        cfg = self._config
        base_icn = self._current_base_icn()
        if cfg is None or not base_icn:
            self._base_image = None
            self._base_thumb.clear()
            self._base_thumb.setText("Select a custom monster with a building.")
            self._send_btn.setEnabled(False)
            return

        self._status_label.setText(f"Extracting {base_icn} from AGG...")
        self.repaint()

        sprites = extract_icn(base_icn)
        if sprites is None or not sprites.frames:
            self._status_label.setText(f"Failed to extract {base_icn}.")
            self._send_btn.setEnabled(False)
            return

        idx = self._current_base_index()
        if idx < 0 or idx >= len(sprites.frames):
            self._status_label.setText(
                f"Frame {idx} out of range for {base_icn} ({len(sprites.frames)} frames)."
            )
            self._send_btn.setEnabled(False)
            return

        frame = sprites.frames[idx].image.convert("RGBA")
        self._base_image = frame
        self._base_size = (frame.width, frame.height)

        composited = _replace_bg_with_color(frame, self._bg_color)
        self._base_thumb.setPixmap(self._fit_to_label(composited, self._base_thumb))
        self._send_btn.setEnabled(True)
        self._status_label.setText(
            f"Loaded {base_icn}[{idx}] base sprite ({self._base_size[0]}x{self._base_size[1]})."
        )

    def _load_saved_for_target(self):
        """Show the on-disk PNG for the current target if it exists, else clear preview."""
        path = self._current_output_path()
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
        self._spinner.start()
        self._status_label.setText("Sending to Gemini...")

        self._worker = BuildingWorker(client, upscaled, prompt, system, self._reference_image)
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
        self._spinner.stop()

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

        keyed = self._key_bg_to_alpha(result.convert("RGBA"))
        self._output_image = self._fit_into_base_canvas(keyed)
        self._output_is_unsaved = True

        self._out_thumb.setPixmap(self._fit_to_label(self._output_image, self._out_thumb))
        self._output_caption.setText(
            f"Fresh Gemini output ({self._base_size[0]}x{self._base_size[1]}) — Accept to save."
        )
        self._accept_btn.setEnabled(True)
        self._reject_btn.setEnabled(True)
        self._status_label.setText(
            f"Output {self._base_size[0]}x{self._base_size[1]}. Click Accept to save."
        )

    def _on_error(self, error_msg: str):
        self._send_btn.setEnabled(True)
        self._cancel_btn.setEnabled(False)
        self._spinner.stop()
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

    def _fit_into_base_canvas(self, img: Image.Image) -> Image.Image:
        """Crop transparent borders and bottom-center into a canvas of the exact base size."""
        rgba = img.convert("RGBA")
        bbox = rgba.getbbox()
        if bbox is None:
            return Image.new("RGBA", self._base_size, (0, 0, 0, 0))

        cropped = rgba.crop(bbox)
        target_w, target_h = self._base_size
        scale = min(target_w / cropped.width, target_h / cropped.height)
        new_w = max(1, int(round(cropped.width * scale)))
        new_h = max(1, int(round(cropped.height * scale)))
        scaled = cropped.resize((new_w, new_h), Image.Resampling.LANCZOS)

        canvas = Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))
        paste_x = (target_w - new_w) // 2
        paste_y = target_h - new_h  # bottom-align
        canvas.paste(scaled, (paste_x, paste_y), scaled)
        return canvas

    # ── accept / reject / delete ──────────────────────────────────────────────

    def _accept(self):
        if self._output_image is None or self._config is None or not self._output_is_unsaved:
            return
        out_path = self._current_output_path()
        if out_path is None:
            return
        out_path.parent.mkdir(parents=True, exist_ok=True)
        self._output_image.save(out_path)
        self._output_is_unsaved = False
        self._output_caption.setText(f"Currently saved: {out_path.name}")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._delete_btn.setEnabled(True)
        self._status_label.setText(f"Saved {out_path}")

    def _reject(self):
        # Drop the unsaved Gemini output and restore whatever was saved on disk
        # (or empty if nothing was).
        self._output_image = None
        self._output_is_unsaved = False
        self._out_thumb.clear()
        self._out_thumb.setText("No output yet")
        self._output_caption.setText("")
        self._accept_btn.setEnabled(False)
        self._reject_btn.setEnabled(False)
        self._load_saved_for_target()

    def _delete_saved(self):
        path = self._current_output_path()
        if path is None or not path.exists():
            return
        confirm = QMessageBox.question(
            self, "Delete saved PNG",
            f"Delete {path.name}?\n\nThe engine will fall back to the base building art.",
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
        self._status_label.setText(f"Deleted {path.name}")
        self._reset_output_state()
