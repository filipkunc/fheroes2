"""Gemini generation panel — prompt editor, sheet preview, accept/reject workflow."""

import time
from pathlib import Path

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QFormLayout,
    QTextEdit, QComboBox, QSpinBox, QPushButton, QLabel,
    QFileDialog, QMessageBox, QCheckBox, QColorDialog,
)
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QImage, QPixmap, QColor

from PIL import Image

from ..models.sprite_data import SpriteFrame
from ..widgets.busy_spinner import BusySpinner
from .gemini_client import GeminiClient, build_sheet, slice_sheet, SlicedFrame, load_api_key
from ..models.sprite_data import save_custom_offsets, FrameOverride
from .cost_tracker import CostTracker


class GeminiWorker(QThread):
    """Background thread for Gemini API calls."""
    finished = Signal(object)  # Image or None
    error = Signal(str)

    def __init__(self, client: GeminiClient, sheet: Image.Image, prompt: str,
                 system: str | None, reference: Image.Image | None):
        super().__init__()
        self._client = client
        self._sheet = sheet
        self._prompt = prompt
        self._system = system
        self._reference = reference

    def run(self):
        try:
            result = self._client.send_sheet(
                self._sheet, self._prompt,
                system_instruction=self._system,
                reference=self._reference,
            )
            self.finished.emit(result)
        except Exception as e:
            self.error.emit(str(e))


def _pil_to_qpixmap(img: Image.Image, max_w: int = 400, max_h: int = 500) -> QPixmap:
    """Convert PIL image to QPixmap, scaled to fit."""
    rgba = img.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    qimg = QImage(data, rgba.width, rgba.height, rgba.width * 4, QImage.Format.Format_RGBA8888)
    pixmap = QPixmap.fromImage(qimg.copy())
    return pixmap.scaled(max_w, max_h, Qt.AspectRatioMode.KeepAspectRatio,
                         Qt.TransformationMode.SmoothTransformation)


class GeminiPanel(QWidget):
    """Gemini generation workflow panel."""

    frames_accepted = Signal(list, list)  # (frame_indices, PIL images)
    sheet_built = Signal(object, list, str)  # (PIL Image, cell rects, label)
    output_received = Signal(object, list, str)  # (PIL Image, cell rects, label)
    bg_color_changed = Signal(tuple)  # (r, g, b)
    reference_changed = Signal(str)  # absolute path, or "" when cleared

    def __init__(self, parent=None):
        super().__init__(parent)

        self._custom_frames: list[SpriteFrame] = []
        self._base_frames: list[SpriteFrame] = []
        self._frame_indices: list[int] = []
        self._batch_frames: list[SpriteFrame] = []  # frames locked in for current batch
        self._input_sheet: Image.Image | None = None
        self._output_sheet: Image.Image | None = None
        self._layout: list[tuple[int, int, int, int] | None] = []
        self._sliced_results: list[Image.Image] = []
        self._accepted: list[bool] = []
        self._worker: GeminiWorker | None = None
        self._cost_tracker = CostTracker.load()
        self._reference_image: Image.Image | None = None
        self._reference_path: str = ""
        self._prefix: str = ""

        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Prompt section
        prompt_group = QGroupBox("Prompt")
        prompt_layout = QFormLayout()

        self._prompt_edit = QTextEdit()
        self._prompt_edit.setMaximumHeight(60)
        self._prompt_edit.setPlainText(
            "Transform into Thor with golden armor, winged helmet, red cape, and hammer"
        )
        prompt_layout.addRow("Transform:", self._prompt_edit)

        self._system_edit = QTextEdit()
        self._system_edit.setMaximumHeight(80)
        self._system_edit.setPlainText(
            "You are a specialized pixel art editor for game sprite animations. "
            "You receive a sprite sheet grid containing animation frames of a game character on a colored background. Your task:\n"
            "1. Transform the character's appearance as instructed while preserving EXACT pose, proportions, and position of every frame\n"
            "2. Each cell in the grid is one animation frame — you must output exactly the same number of cells in the same grid layout\n"
            "3. Do NOT merge, remove, or rearrange any cells\n"
            "4. Keep the background color exactly as provided\n"
            "5. Maintain pixel art style consistency across all frames\n"
            "Adhere to the reference image."
        )
        prompt_layout.addRow("System:", self._system_edit)

        prompt_group.setLayout(prompt_layout)
        layout.addWidget(prompt_group)

        # Reference image — collapsible group with thumbnail
        ref_group = QGroupBox("Reference image (sent to Gemini)")
        ref_group.setCheckable(True)
        ref_group.setChecked(True)
        ref_outer = QVBoxLayout(ref_group)
        ref_outer.setContentsMargins(6, 6, 6, 6)

        self._ref_body = QWidget()
        ref_body_layout = QVBoxLayout(self._ref_body)
        ref_body_layout.setContentsMargins(0, 0, 0, 0)

        self._ref_thumb = QLabel("No reference image loaded")
        self._ref_thumb.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._ref_thumb.setMinimumHeight(120)
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

        # Settings row
        settings_layout = QHBoxLayout()

        settings_layout.addWidget(QLabel("Source:"))
        self._source_combo = QComboBox()
        self._source_combo.addItems(["Custom", "Base"])
        self._source_combo.setToolTip("Which sprites to send as input: Custom (e.g. Thor) or Base (e.g. Titan)")
        settings_layout.addWidget(self._source_combo)

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
        self._upscale_spin.setRange(1, 16)
        self._upscale_spin.setValue(4)
        settings_layout.addWidget(self._upscale_spin)

        settings_layout.addWidget(QLabel("Margin:"))
        self._margin_spin = QSpinBox()
        self._margin_spin.setRange(0, 20)
        self._margin_spin.setValue(4)
        self._margin_spin.setToolTip("Padding around each sprite (native px) — gives Gemini room for capes, wings, etc.")
        settings_layout.addWidget(self._margin_spin)

        settings_layout.addWidget(QLabel("BG:"))
        self._bg_color = (255, 0, 255)
        self._bg_btn = QPushButton()
        self._bg_btn.setFixedWidth(30)
        self._bg_btn.setStyleSheet("background-color: #FF00FF;")
        self._bg_btn.setToolTip("Background color for sprite sheet cells")
        self._bg_btn.clicked.connect(self._pick_bg_color)
        settings_layout.addWidget(self._bg_btn)

        self._mask_cb = QCheckBox("Silhouette mask")
        self._mask_cb.setToolTip("When checked, clips output to original silhouette. Uncheck to allow new content (cape, wings).")
        self._mask_cb.setChecked(False)
        settings_layout.addWidget(self._mask_cb)

        layout.addLayout(settings_layout)

        # Build / Send buttons
        action_layout = QHBoxLayout()
        self._selection_label = QLabel("No frames selected")
        action_layout.addWidget(self._selection_label)
        action_layout.addStretch()

        self._build_btn = QPushButton("Build Sheet")
        self._build_btn.clicked.connect(self._build_sheet)
        action_layout.addWidget(self._build_btn)

        self._send_btn = QPushButton("Send to Gemini")
        self._send_btn.clicked.connect(self._send_to_gemini)
        self._send_btn.setEnabled(False)
        action_layout.addWidget(self._send_btn)

        self._spinner = BusySpinner()
        action_layout.addWidget(self._spinner)

        layout.addLayout(action_layout)

        # Accept/reject section
        result_group = QGroupBox("Results")
        result_layout = QVBoxLayout()
        cb_row = QHBoxLayout()
        cb_row.addWidget(QLabel("Per-frame accept:"))
        self._checkboxes_container = QWidget()
        self._checkboxes_inner = QHBoxLayout(self._checkboxes_container)
        self._checkboxes_inner.setContentsMargins(0, 0, 0, 0)
        cb_row.addWidget(self._checkboxes_container)
        cb_row.addStretch()
        result_layout.addLayout(cb_row)

        result_btns = QHBoxLayout()
        self._accept_btn = QPushButton("Accept Selected")
        self._accept_btn.clicked.connect(self._accept_selected)
        self._accept_btn.setEnabled(False)
        result_btns.addWidget(self._accept_btn)

        self._reject_btn = QPushButton("Reject All")
        self._reject_btn.clicked.connect(self._reject_all)
        self._reject_btn.setEnabled(False)
        result_btns.addWidget(self._reject_btn)

        result_btns.addStretch()

        self._cost_label = QLabel(f"Cost: {self._cost_tracker.summary()}")
        result_btns.addWidget(self._cost_label)

        result_layout.addLayout(result_btns)
        result_group.setLayout(result_layout)
        layout.addWidget(result_group)

        # Status
        self._status_label = QLabel("")
        layout.addWidget(self._status_label)

    def set_selected_frames(self, custom_frames: list[SpriteFrame], base_frames: list[SpriteFrame],
                            indices: list[int], prefix: str):
        """Called when frame selection changes in the main window."""
        self._custom_frames = custom_frames
        self._base_frames = base_frames
        self._frame_indices = indices
        self._prefix = prefix

        # Update source combo labels
        self._source_combo.setItemText(0, f"Custom ({prefix})")
        self._source_combo.setItemText(1, f"Base")

        if indices:
            idx_str = ", ".join(str(i) for i in indices[:10])
            if len(indices) > 10:
                idx_str += f"... ({len(indices)} total)"
            self._selection_label.setText(f"Selected: {idx_str}")
        else:
            self._selection_label.setText("No frames selected")

    @property
    def _selected_frames(self) -> list[SpriteFrame]:
        """Return frames based on current source selection."""
        if self._source_combo.currentIndex() == 1 and self._base_frames:
            return self._base_frames
        return self._custom_frames

    def _browse_reference(self):
        start_dir = str(Path(self._reference_path).parent) if self._reference_path else ""
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Reference Image", start_dir,
            "Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)",
        )
        if path:
            self.load_reference_image(path)
            self.reference_changed.emit(path)

    def load_reference_image(self, path: str) -> bool:
        """Load a reference image from disk and populate the preview.

        Returns True on success. Used by callers restoring persisted state, so
        missing files or decode errors silently clear the reference instead of
        raising.
        """
        try:
            image = Image.open(path).convert("RGB")
        except (FileNotFoundError, OSError):
            self._clear_reference_state()
            return False
        self._reference_image = image
        self._reference_path = path
        name = Path(path).name
        self._ref_label.setText(f"{name} ({image.width}x{image.height})")
        self._ref_thumb.setPixmap(_pil_to_qpixmap(image, 260, 260))
        return True

    def _clear_reference(self):
        self._clear_reference_state()
        self.reference_changed.emit("")

    def _clear_reference_state(self):
        self._reference_image = None
        self._reference_path = ""
        self._ref_label.setText("None")
        self._ref_thumb.clear()
        self._ref_thumb.setText("No reference image loaded")

    def _get_sheets_dir(self) -> Path:
        """Get or create the sheets output directory."""
        sprites_dir = Path(__file__).parent.parent.parent.parent / "files" / "data" / "sprites"
        sheets_dir = sprites_dir / "sheets"
        sheets_dir.mkdir(parents=True, exist_ok=True)
        return sheets_dir

    def _pick_bg_color(self):
        """Open color picker for sheet background."""
        r, g, b = self._bg_color
        color = QColorDialog.getColor(QColor(r, g, b), self, "Sheet Background Color")
        if color.isValid():
            rgb = (color.red(), color.green(), color.blue())
            self.set_bg_color(rgb)
            self.bg_color_changed.emit(rgb)

    def set_bg_color(self, rgb: tuple[int, int, int]) -> None:
        """Apply a bg color without emitting bg_color_changed — for restoring persisted state."""
        self._bg_color = rgb
        self._bg_btn.setStyleSheet(f"background-color: rgb({rgb[0]},{rgb[1]},{rgb[2]});")

    def _build_sheet(self):
        """Build input sprite sheet from selected frames."""
        if not self._selected_frames:
            QMessageBox.warning(self, "No Frames", "Select frames in the frame list first.")
            return

        # Lock in the frames for this batch — don't let source combo changes affect results
        self._batch_frames = list(self._selected_frames)

        upscale = self._upscale_spin.value()
        margin = self._margin_spin.value()
        self._input_sheet, self._layout = build_sheet(
            self._batch_frames, upscale=upscale, margin=margin,
            bg_color=self._bg_color,
        )

        # Save input sheet to disk
        self._sheets_dir = self._get_sheets_dir()
        idx_str = "_".join(f"{i:03d}" for i in self._frame_indices[:6])
        if len(self._frame_indices) > 6:
            idx_str += f"_etc{len(self._frame_indices)}"
        self._current_batch_name = f"{self._prefix}_{idx_str}"
        input_path = self._sheets_dir / f"{self._current_batch_name}_input.png"
        self._input_sheet.save(input_path)

        self._send_btn.setEnabled(True)
        real_count = len([f for f in self._selected_frames if not f.is_placeholder])
        label = (f"Input: {self._input_sheet.width}x{self._input_sheet.height}, "
                 f"{real_count} frames, {upscale}x, margin {margin}px")
        self._status_label.setText(f"{label} — saved to {input_path.name}")

        # Show in Sprite Sheet panel
        cells = [c for c in self._layout if c is not None]
        self.sheet_built.emit(self._input_sheet, cells, label)

    def _send_to_gemini(self):
        """Send the built sheet to Gemini API."""
        if self._input_sheet is None:
            return

        prompt = self._prompt_edit.toPlainText().strip()
        if not prompt:
            QMessageBox.warning(self, "No Prompt", "Enter a transform prompt.")
            return

        system = self._system_edit.toPlainText().strip() or None
        model = self._model_combo.currentText()

        try:
            client = GeminiClient(model=model)
        except ValueError as e:
            QMessageBox.critical(self, "API Key Error", str(e))
            return

        self._send_btn.setEnabled(False)
        self._spinner.start()
        self._status_label.setText("Sending to Gemini...")

        self._worker = GeminiWorker(client, self._input_sheet, prompt, system, self._reference_image)
        self._worker.finished.connect(self._on_gemini_result)
        self._worker.error.connect(self._on_gemini_error)
        self._worker.start()

    def _on_gemini_result(self, result: Image.Image | None):
        """Handle Gemini API response."""
        self._send_btn.setEnabled(True)
        self._spinner.stop()

        if result is None:
            self._status_label.setText("Gemini returned no image")
            self._cost_tracker.log_call(
                self._model_combo.currentText(), self._frame_indices,
                self._input_sheet.size, success=False,
            )
            self._cost_label.setText(f"Cost: {self._cost_tracker.summary()}")
            return

        self._output_sheet = result
        self._cost_tracker.log_call(
            self._model_combo.currentText(), self._frame_indices,
            self._input_sheet.size, result.size,
        )
        self._cost_label.setText(f"Cost: {self._cost_tracker.summary()}")

        # Save output sheet to disk
        if hasattr(self, '_sheets_dir') and hasattr(self, '_current_batch_name'):
            output_path = self._sheets_dir / f"{self._current_batch_name}_output.png"
            result.save(output_path)

        # Slice into individual frames using the locked-in batch frames
        upscale = self._upscale_spin.value()
        margin = self._margin_spin.value()
        self._sliced_results = slice_sheet(
            result, self._layout, self._batch_frames,
            upscale=upscale, margin=margin,
            use_silhouette_mask=self._mask_cb.isChecked(),
            bg_color=self._bg_color,
        )

        # Save sliced frames to sheets dir for manual editing
        if hasattr(self, '_sheets_dir') and hasattr(self, '_current_batch_name'):
            for sliced in self._sliced_results:
                if sliced.image.width > 1:
                    slice_path = self._sheets_dir / f"{self._current_batch_name}_slice_{sliced.frame_index:03d}.png"
                    sliced.image.save(slice_path)

        # Create per-frame checkboxes
        self._build_checkboxes()
        self._accept_btn.setEnabled(True)
        self._reject_btn.setEnabled(True)
        out_label = f"Output: {result.width}x{result.height}, {len(self._sliced_results)} frames"
        self._status_label.setText(f"{out_label}. Saved to {self._sheets_dir.name}/. Select frames to accept.")

        # Show output in Sprite Sheet panel
        cells = [c for c in self._layout if c is not None]
        self.output_received.emit(result, cells, out_label)

    def _on_gemini_error(self, error_msg: str):
        """Handle Gemini API error."""
        self._send_btn.setEnabled(True)
        self._spinner.stop()
        self._status_label.setText(f"Error: {error_msg}")
        self._cost_tracker.log_call(
            self._model_combo.currentText(), self._frame_indices,
            self._input_sheet.size if self._input_sheet else (0, 0), success=False,
        )
        self._cost_label.setText(f"Cost: {self._cost_tracker.summary()}")
        QMessageBox.critical(self, "Gemini Error", error_msg)

    def _build_checkboxes(self):
        """Create accept/reject checkboxes for each sliced frame."""
        # Clear old checkboxes by replacing the container's layout
        old_layout = self._checkboxes_inner
        while old_layout.count():
            item = old_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        self._accepted = []
        for i, sliced in enumerate(self._sliced_results):
            if sliced.image.width <= 1:
                self._accepted.append(False)
                continue
            cb = QCheckBox(f"{sliced.frame_index:03d}")
            cb.setChecked(True)
            self._accepted.append(True)
            idx = i
            cb.toggled.connect(lambda checked, j=idx: self._on_checkbox_toggled(j, checked))
            self._checkboxes_inner.addWidget(cb)

    def _on_checkbox_toggled(self, index: int, checked: bool):
        if index < len(self._accepted):
            self._accepted[index] = checked

    def _reject_all(self):
        """Uncheck all frame checkboxes."""
        for i in range(self._checkboxes_inner.count()):
            widget = self._checkboxes_inner.itemAt(i).widget()
            if isinstance(widget, QCheckBox):
                widget.setChecked(False)

    def _accept_selected(self):
        """Save accepted frames to the sprites directory."""
        if not self._sliced_results or not self._prefix:
            return

        sprites_dir = Path(__file__).parent.parent.parent.parent / "files" / "data" / "sprites"
        sprites_dir.mkdir(parents=True, exist_ok=True)

        saved = 0
        accepted_indices = []
        offset_overrides: dict[int, FrameOverride] = {}
        for sliced, accepted in zip(self._sliced_results, self._accepted):
            if not accepted or sliced.image.width <= 1:
                continue
            out_path = sprites_dir / f"{self._prefix}_{sliced.frame_index:03d}.png"
            sliced.image.save(out_path)
            accepted_indices.append(sliced.frame_index)
            offset_overrides[sliced.frame_index] = FrameOverride(
                offset_x=sliced.offset_x,
                offset_y=sliced.offset_y,
                display_width=sliced.display_width,
                display_height=sliced.display_height,
            )
            saved += 1

        # Persist offset overrides to sidecar JSONL (read by both editor and engine)
        if offset_overrides:
            save_custom_offsets(sprites_dir, self._prefix, offset_overrides)

        self._status_label.setText(f"Saved {saved} frames + offsets to {sprites_dir}")
        if saved > 0:
            self.frames_accepted.emit(
                accepted_indices,
                [s.image for s, a in zip(self._sliced_results, self._accepted) if a],
            )
