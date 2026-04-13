"""Main application window assembling all panels and coordinating data flow."""

from pathlib import Path

from PySide6.QtWidgets import (
    QMainWindow, QDockWidget, QToolBar, QComboBox, QPushButton,
    QSpinBox, QLabel, QSlider, QFileDialog, QMessageBox, QStatusBar,
    QCheckBox, QWidget, QHBoxLayout
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence

from .models.monster_config import MonsterConfig, load_manifest
from .models.bin_parser import (
    MonsterAnimInfo, AnimType, ANIM_DISPLAY_NAMES, COMPOSITE_ANIMATIONS,
    load_bin_file, parse_bin,
)
from .models.sprite_data import SpriteCollection, load_png_sprites, load_bmp_sprites, apply_offsets_from_base
from .models.animation import AnimationPlayer
from .widgets.animation_canvas import AnimationCanvas
from .widgets.frame_list import FrameListWidget
from .widgets.properties_panel import PropertiesPanel
from .widgets.spritesheet_view import SpritesheetView


# Default paths
PROJECT_ROOT = Path(__file__).parent.parent.parent
SPRITES_DIR = PROJECT_ROOT / "files" / "data" / "sprites"
BUILD_DIR = PROJECT_ROOT / "build"
AGG_PATH = Path.home() / "Games" / "Heroic" / "HoMM 2 Gold" / "DATA" / "HEROES2.AGG"


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("fheroes2 Sprite Editor")
        self.resize(1200, 800)

        # State
        self._monsters = load_manifest()
        self._current_config: MonsterConfig | None = None
        self._custom_sprites: SpriteCollection | None = None
        self._base_sprites: SpriteCollection | None = None
        self._anim_info: MonsterAnimInfo | None = None
        self._current_anim_name: str = "All Frames"

        # Animation player
        self._player = AnimationPlayer(self)
        self._player.frame_changed.connect(self._on_frame_changed)
        self._syncing_frame = False  # guard against frame_list → stop feedback loop

        self._setup_ui()
        self._setup_toolbar()
        self._setup_menu()
        self._setup_statusbar()

        # Load first monster if sprites exist
        if self._monsters:
            first = list(self._monsters.keys())[0]
            self._monster_combo.setCurrentText(first)
            self._load_monster(first)

    def _setup_ui(self):
        # Central canvas
        self._canvas = AnimationCanvas()
        self.setCentralWidget(self._canvas)

        # Left dock: frame list
        self._frame_list = FrameListWidget()
        frame_dock = QDockWidget("Frames")
        frame_dock.setWidget(self._frame_list)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, frame_dock)

        self._frame_list.frame_selected.connect(self._on_frame_selected)
        self._frame_list.frames_selected.connect(self._on_frames_selected)

        # Right dock: properties
        self._properties = PropertiesPanel()
        props_dock = QDockWidget("Properties")
        props_dock.setWidget(self._properties)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, props_dock)

        self._properties.offset_changed.connect(self._on_offset_changed)

        # Bottom dock: sprite sheet view
        self._sheet_view = SpritesheetView()
        sheet_dock = QDockWidget("Sprite Sheet")
        sheet_dock.setWidget(self._sheet_view)
        sheet_dock.setVisible(False)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, sheet_dock)
        self._sheet_dock = sheet_dock

    def _setup_toolbar(self):
        toolbar = QToolBar("Main")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        # Monster selector
        toolbar.addWidget(QLabel(" Monster: "))
        self._monster_combo = QComboBox()
        self._monster_combo.addItems(list(self._monsters.keys()))
        self._monster_combo.currentTextChanged.connect(self._load_monster)
        toolbar.addWidget(self._monster_combo)

        toolbar.addSeparator()

        # Animation selector
        toolbar.addWidget(QLabel(" Animation: "))
        self._anim_combo = QComboBox()
        self._anim_combo.setMinimumWidth(140)
        self._anim_combo.currentTextChanged.connect(self._on_anim_changed)
        toolbar.addWidget(self._anim_combo)

        toolbar.addSeparator()

        # Playback controls
        self._play_btn = QPushButton("Play")
        self._play_btn.setCheckable(True)
        self._play_btn.clicked.connect(self._toggle_play)
        toolbar.addWidget(self._play_btn)

        prev_btn = QPushButton("<")
        prev_btn.setFixedWidth(30)
        prev_btn.clicked.connect(self._player.step_backward)
        toolbar.addWidget(prev_btn)

        next_btn = QPushButton(">")
        next_btn.setFixedWidth(30)
        next_btn.clicked.connect(self._player.step_forward)
        toolbar.addWidget(next_btn)

        # Frame position indicator
        self._pos_label = QLabel(" 0/0 ")
        toolbar.addWidget(self._pos_label)

        toolbar.addSeparator()

        # Speed control
        toolbar.addWidget(QLabel(" Speed: "))
        self._speed_spin = QSpinBox()
        self._speed_spin.setRange(20, 500)
        self._speed_spin.setValue(120)
        self._speed_spin.setSuffix(" ms")
        self._speed_spin.valueChanged.connect(self._player.set_speed)
        toolbar.addWidget(self._speed_spin)

        toolbar.addSeparator()

        # Zoom
        toolbar.addWidget(QLabel(" Zoom: "))
        self._zoom_spin = QSpinBox()
        self._zoom_spin.setRange(1, 10)
        self._zoom_spin.setValue(3)
        self._zoom_spin.valueChanged.connect(self._canvas.set_zoom)
        toolbar.addWidget(self._zoom_spin)

        toolbar.addSeparator()

        # Side-by-side toggle
        self._side_by_side_cb = QCheckBox("Side-by-side")
        self._side_by_side_cb.toggled.connect(self._toggle_side_by_side)
        toolbar.addWidget(self._side_by_side_cb)

        # Show offsets toggle
        self._offsets_cb = QCheckBox("Show offsets")
        self._offsets_cb.toggled.connect(self._canvas.set_show_offsets)
        toolbar.addWidget(self._offsets_cb)

    def _setup_menu(self):
        menu = self.menuBar()

        # File menu
        file_menu = menu.addMenu("&File")
        load_dir = QAction("&Load Sprites Directory...", self)
        load_dir.triggered.connect(self._load_sprites_dir)
        file_menu.addAction(load_dir)

        load_bin = QAction("Load &BIN File...", self)
        load_bin.triggered.connect(self._load_bin_file)
        file_menu.addAction(load_bin)

        file_menu.addSeparator()

        extract_action = QAction("&Extract Base Sprites...", self)
        extract_action.triggered.connect(self._extract_base)
        file_menu.addAction(extract_action)

        file_menu.addSeparator()

        quit_action = QAction("&Quit", self)
        quit_action.setShortcut(QKeySequence.StandardKey.Quit)
        quit_action.triggered.connect(self.close)
        file_menu.addAction(quit_action)

        # View menu
        view_menu = menu.addMenu("&View")
        sheet_action = QAction("Sprite &Sheet Panel", self)
        sheet_action.setCheckable(True)
        sheet_action.toggled.connect(self._sheet_dock.setVisible)
        view_menu.addAction(sheet_action)

    def _setup_statusbar(self):
        self._status = QStatusBar()
        self.setStatusBar(self._status)

    def _load_monster(self, name: str):
        """Load a monster by name from the manifest."""
        config = self._monsters.get(name)
        if not config:
            return

        self._current_config = config
        self._player.stop()
        self._play_btn.setChecked(False)

        # Load sprites depending on monster type
        self._custom_sprites = None
        self._base_sprites = None

        if config.has_custom_sprites:
            # Try loading custom PNG sprites
            if SPRITES_DIR.exists():
                first_frame = SPRITES_DIR / f"{config.prefix}_000.png"
                if first_frame.exists():
                    self._custom_sprites = load_png_sprites(SPRITES_DIR, config.prefix, config.frame_count)
                    self._status.showMessage(f"Loaded {self._custom_sprites.frame_count} custom frames for {config.prefix}")
                else:
                    self._status.showMessage(f"No custom sprites found for {config.prefix} in {SPRITES_DIR}")
        elif config.palette_remap:
            # Palette-remapped monster: extract base ICN and apply remap
            self._status.showMessage(f"Extracting {config.base_icn} for palette remap...")
            self.repaint()
            self._load_palette_remapped(config)
        else:
            self._status.showMessage(f"No sprites available for {config.prefix}")

        # Load BIN animation data
        self._anim_info = self._try_load_bin(config.bin_file)

        # Update UI
        self._update_canvas()
        self._update_frame_list()
        self._update_anim_combo()

    def _try_load_bin(self, bin_filename: str) -> MonsterAnimInfo | None:
        """Try to load a BIN file from various locations."""
        # Check for pre-extracted BIN files in a few locations
        search_paths = [
            PROJECT_ROOT / "tools" / "sprite_editor" / "resources" / bin_filename,
            BUILD_DIR / bin_filename,
            Path("/tmp") / bin_filename,
        ]
        for p in search_paths:
            if p.exists() and p.stat().st_size == 821:
                try:
                    return load_bin_file(p)
                except Exception as e:
                    print(f"Failed to parse {p}: {e}")

        # Try extracting from AGG
        from .tools.icn_extractor import extract_bin
        data = extract_bin(bin_filename, BUILD_DIR, AGG_PATH)
        if data:
            try:
                info = parse_bin(data)
                # Cache it for next time
                cache_path = PROJECT_ROOT / "tools" / "sprite_editor" / "resources" / bin_filename
                cache_path.write_bytes(data)
                return info
            except Exception as e:
                print(f"Failed to parse extracted BIN: {e}")

        self._status.showMessage(f"BIN file not found: {bin_filename} (animations unavailable)")
        return None

    def _load_palette_remapped(self, config: MonsterConfig):
        """Extract base ICN sprites and apply palette remap to show the recolored monster."""
        from .tools.icn_extractor import extract_icn, _find_file
        from .tools.palette_remap import REMAP_TABLES, load_palette, apply_palette_remap_fast
        import tempfile, subprocess

        remap_table = REMAP_TABLES.get(config.palette_remap)
        if not remap_table:
            self._status.showMessage(f"No remap table for '{config.palette_remap}'")
            return

        # Extract base sprites
        base = extract_icn(config.base_icn, BUILD_DIR, AGG_PATH)
        if not base:
            self._status.showMessage(f"Could not extract base {config.base_icn}")
            return

        self._base_sprites = base

        # Load KB.PAL for RGB conversion
        pal_path = self._find_pal_file()
        if not pal_path:
            self._status.showMessage("KB.PAL not found — showing base sprites without remap")
            self._custom_sprites = base
            return

        palette = load_palette(pal_path)

        # Apply palette remap to each frame
        remapped = SpriteCollection(prefix=config.prefix)
        from .models.sprite_data import SpriteFrame
        for frame in base.frames:
            if frame.is_placeholder:
                remapped.frames.append(SpriteFrame(
                    index=frame.index, image=frame.image,
                    offset_x=frame.offset_x, offset_y=frame.offset_y,
                    is_placeholder=True,
                ))
            else:
                new_img = apply_palette_remap_fast(frame.image, palette, remap_table)
                remapped.frames.append(SpriteFrame(
                    index=frame.index, image=new_img,
                    offset_x=frame.offset_x, offset_y=frame.offset_y,
                ))
        remapped.source_dir = base.source_dir

        self._custom_sprites = remapped
        self._side_by_side_cb.setChecked(True)
        self._status.showMessage(
            f"Loaded {config.base_monster} with {config.palette_remap} palette remap "
            f"({remapped.frame_count} frames) — side-by-side: base vs remapped"
        )

    def _find_pal_file(self) -> Path | None:
        """Find KB.PAL in cached resources or extract from AGG."""
        cached = PROJECT_ROOT / "tools" / "sprite_editor" / "resources" / "KB.PAL"
        if cached.exists():
            return cached

        # Extract from AGG
        import subprocess, tempfile
        extractor = BUILD_DIR / "extractor"
        if not extractor.exists() or not AGG_PATH.exists():
            return None

        with tempfile.TemporaryDirectory(prefix="sprite_editor_pal_") as tmpdir:
            tmp = Path(tmpdir)
            subprocess.run(
                [str(extractor), str(tmp), str(AGG_PATH)],
                capture_output=True, timeout=60,
            )
            from .tools.icn_extractor import _find_file
            pal = _find_file(tmp, "KB.PAL")
            if pal:
                # Cache for next time
                import shutil
                shutil.copy2(pal, cached)
                return cached
        return None

    def _update_canvas(self):
        """Update canvas with current sprites."""
        if self._side_by_side_cb.isChecked() and self._base_sprites:
            self._canvas.set_sprites(self._base_sprites, self._custom_sprites)
        else:
            self._canvas.set_sprites(self._custom_sprites or self._base_sprites)

    def _update_frame_list(self):
        """Update frame list with current sprites."""
        sprites = self._custom_sprites or self._base_sprites
        if sprites:
            self._frame_list.set_sprites(sprites)

    def _update_anim_combo(self):
        """Populate animation combo box based on BIN data."""
        self._anim_combo.blockSignals(True)
        self._anim_combo.clear()

        if self._anim_info:
            for name in self._anim_info.available_animations():
                self._anim_combo.addItem(name)
        else:
            # No BIN data — just offer "All Frames"
            self._anim_combo.addItem("All Frames")

        self._anim_combo.blockSignals(False)
        self._on_anim_changed(self._anim_combo.currentText())

    def _on_anim_changed(self, name: str):
        """Animation selection changed."""
        if not name:
            return
        self._current_anim_name = name

        if self._anim_info:
            self._player.set_animation(self._anim_info, name)
        elif self._custom_sprites:
            # No BIN data: play all frames sequentially
            self._player.set_raw_sequence(list(range(self._custom_sprites.frame_count)))
        elif self._base_sprites:
            self._player.set_raw_sequence(list(range(self._base_sprites.frame_count)))

        self._update_pos_label()

    def _on_frame_changed(self, frame_index: int):
        """Animation player advanced to a new frame."""
        self._canvas.set_frame_index(frame_index)
        # Guard: select_frame triggers currentRowChanged → _on_frame_selected,
        # which must not stop the player during animated playback.
        self._syncing_frame = True
        self._frame_list.select_frame(frame_index)
        self._syncing_frame = False
        self._update_pos_label()

        # Update properties
        sprites = self._custom_sprites or self._base_sprites
        if sprites:
            frame = sprites.get_frame(frame_index)
            self._properties.set_frame(frame)
            if self._anim_info:
                self._properties.set_animation_info(
                    self._current_anim_name,
                    self._anim_info.get_composite_frames(self._current_anim_name),
                    self._player.interval_ms,
                    self._player.current_position,
                )

    def _on_frame_selected(self, frame_index: int):
        """User clicked a frame in the list."""
        if self._syncing_frame:
            return  # triggered by animation playback, not user click

        self._canvas.set_frame_index(frame_index)

        sprites = self._custom_sprites or self._base_sprites
        if sprites:
            frame = sprites.get_frame(frame_index)
            self._properties.set_frame(frame)

    def _on_frames_selected(self, indices: list[int]):
        """Multi-selection changed."""
        self._properties.set_selection_count(len(indices))

    def _on_offset_changed(self, frame_index: int, offset_x: int, offset_y: int):
        """User changed offset in properties panel."""
        sprites = self._custom_sprites or self._base_sprites
        if sprites:
            frame = sprites.get_frame(frame_index)
            if frame:
                frame.offset_x = offset_x
                frame.offset_y = offset_y
                self._canvas.update()

    def _toggle_play(self, checked: bool):
        if checked:
            self._player.play()
            self._play_btn.setText("Stop")
        else:
            self._player.stop()
            self._play_btn.setText("Play")

    def _toggle_side_by_side(self, enabled: bool):
        if enabled and not self._base_sprites:
            self._status.showMessage("No base sprites loaded. Use File > Extract Base Sprites first.")
        self._update_canvas()

    def _update_pos_label(self):
        pos = self._player.current_position + 1
        total = self._player.sequence_length
        frame_idx = self._player.current_frame_index
        self._pos_label.setText(f" {pos}/{total} (#{frame_idx}) ")

    def _load_sprites_dir(self):
        """Load sprites from a user-chosen directory."""
        dir_path = QFileDialog.getExistingDirectory(self, "Select Sprites Directory")
        if not dir_path:
            return
        dir_path = Path(dir_path)

        # Try to detect format
        png_files = sorted(dir_path.glob("*.png"))
        bmp_files = sorted(dir_path.glob("*.bmp"))

        if png_files:
            # Detect prefix from first file: "azure_dragon_000.png" -> "azure_dragon"
            name = png_files[0].stem
            parts = name.rsplit("_", 1)
            if len(parts) == 2 and parts[1].isdigit():
                prefix = parts[0]
                count = len(png_files)
                self._custom_sprites = load_png_sprites(dir_path, prefix, count)
                self._status.showMessage(f"Loaded {count} PNG sprites from {dir_path}")
            else:
                self._custom_sprites = load_bmp_sprites(dir_path)
                self._status.showMessage(f"Loaded {self._custom_sprites.frame_count} sprites from {dir_path}")
        elif bmp_files:
            self._custom_sprites = load_bmp_sprites(dir_path)
            self._status.showMessage(f"Loaded {self._custom_sprites.frame_count} BMP sprites from {dir_path}")
        else:
            QMessageBox.warning(self, "No Sprites", f"No PNG or BMP files found in {dir_path}")
            return

        self._update_canvas()
        self._update_frame_list()

    def _load_bin_file(self):
        """Load a BIN file from disk."""
        path, _ = QFileDialog.getOpenFileName(self, "Select BIN File", "", "BIN Files (*.BIN *.bin);;All Files (*)")
        if not path:
            return
        try:
            self._anim_info = load_bin_file(Path(path))
            self._status.showMessage(f"Loaded BIN: {path}")
            self._update_anim_combo()
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to load BIN file:\n{e}")

    def _extract_base(self):
        """Extract base ICN sprites using icn2img."""
        if not self._current_config:
            QMessageBox.warning(self, "No Monster", "Select a monster first.")
            return

        from .tools.icn_extractor import extract_icn
        self._status.showMessage(f"Extracting {self._current_config.base_icn} from AGG...")
        self.repaint()

        sprites = extract_icn(self._current_config.base_icn, BUILD_DIR, AGG_PATH)
        if sprites:
            self._base_sprites = sprites
            self._status.showMessage(f"Extracted {sprites.frame_count} base sprites for {self._current_config.base_icn}")
            if self._side_by_side_cb.isChecked():
                self._update_canvas()
        else:
            QMessageBox.warning(self, "Extraction Failed",
                                f"Could not extract {self._current_config.base_icn}.\n"
                                f"Check that build tools exist in {BUILD_DIR}\n"
                                f"and AGG file exists at {AGG_PATH}")

    def keyPressEvent(self, event):
        key = event.key()
        if key == Qt.Key.Key_Space:
            self._play_btn.click()
        elif key == Qt.Key.Key_Left:
            self._player.step_backward()
        elif key == Qt.Key.Key_Right:
            self._player.step_forward()
        elif key == Qt.Key.Key_Plus or key == Qt.Key.Key_Equal:
            self._zoom_spin.setValue(self._zoom_spin.value() + 1)
        elif key == Qt.Key.Key_Minus:
            self._zoom_spin.setValue(self._zoom_spin.value() - 1)
        else:
            super().keyPressEvent(event)
