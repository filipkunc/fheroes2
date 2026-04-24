"""Main application window assembling all panels and coordinating data flow."""

from pathlib import Path

from PySide6.QtWidgets import (
    QMainWindow, QDockWidget, QToolBar, QComboBox, QPushButton,
    QSpinBox, QLabel, QSlider, QFileDialog, QMessageBox, QStatusBar,
    QCheckBox, QWidget, QHBoxLayout, QTabWidget, QSplitter
)
from PySide6.QtCore import Qt, QSettings
from PySide6.QtGui import QAction, QKeySequence

from .models.monster_config import MonsterConfig, load_manifest, update_manifest_entry, remove_manifest_entry
from .models.bin_parser import (
    MonsterAnimInfo, AnimType, ANIM_DISPLAY_NAMES, COMPOSITE_ANIMATIONS,
    load_bin_file, parse_bin,
)
from .models.sprite_data import (
    SpriteCollection, load_png_sprites, load_bmp_sprites,
    apply_offsets_from_base, set_base_dimensions, load_custom_offsets, save_custom_offsets,
    FrameOverride,
)
from .models.animation import AnimationPlayer
from .widgets.animation_canvas import AnimationCanvas
from .widgets.frame_list import FrameListWidget
from .widgets.properties_panel import PropertiesPanel
from .widgets.spritesheet_view import SpritesheetView
from .widgets.monster_list import MonsterListWidget
from .gemini.gemini_panel import GeminiPanel


# Default paths
import os

PROJECT_ROOT = Path(__file__).parent.parent.parent
SPRITES_DIR = PROJECT_ROOT / "files" / "data" / "sprites"


def _find_build_dir(root: Path) -> Path:
    # MSBuild/CMake on Windows: build/Release; make on Linux/macOS: build
    for candidate in (root / "build" / "Release", root / "build"):
        if candidate.is_dir():
            return candidate
    return root / "build"


def _find_agg_path() -> Path:
    env = os.environ.get("FHEROES2_AGG")
    if env:
        return Path(env)
    candidates = [
        PROJECT_ROOT.parent / "fheroes2_gog" / "DATA" / "HEROES2.AGG",
        Path.home() / "Games" / "Heroic" / "HoMM 2 Gold" / "DATA" / "HEROES2.AGG",
    ]
    for p in candidates:
        if p.exists():
            return p
    return candidates[-1]


BUILD_DIR = _find_build_dir(PROJECT_ROOT)
AGG_PATH = _find_agg_path()


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("fheroes2 Sprite Editor")
        self.resize(1200, 800)

        # State
        self._settings = QSettings("fheroes2", "sprite_editor")
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
        self._restore_persistent_state()

        # Load thumbnails and select first monster
        self._load_monster_thumbnails()
        if self._monsters:
            first = list(self._monsters.keys())[0]
            self._monster_list.select_monster(first)

    def _restore_persistent_state(self):
        """Reload last-used bg color + reference image from QSettings, then
        connect change signals so future edits are persisted."""
        bg = self._settings.value("gemini/bg_color")
        if isinstance(bg, (list, tuple)) and len(bg) == 3:
            try:
                self._gemini_panel.set_bg_color(tuple(int(c) for c in bg))
            except (TypeError, ValueError):
                pass

        ref_path = self._settings.value("gemini/reference_path", "", type=str)
        if ref_path and Path(ref_path).exists():
            self._gemini_panel.load_reference_image(ref_path)

        self._gemini_panel.bg_color_changed.connect(self._save_bg_color)
        self._gemini_panel.reference_changed.connect(self._save_reference_path)

    def _save_bg_color(self, rgb: tuple):
        self._settings.setValue("gemini/bg_color", list(rgb))

    def _save_reference_path(self, path: str):
        self._settings.setValue("gemini/reference_path", path)

    def _persist_current_prompt(self):
        """Write the current prompt back to the loaded monster's manifest entry."""
        if not self._current_config:
            return
        prompt = self._gemini_panel._prompt_edit.toPlainText()
        if prompt == self._current_config.prompt:
            return
        self._current_config.prompt = prompt
        try:
            update_manifest_entry(self._current_config.name, {"prompt": prompt})
        except OSError as e:
            self._status.showMessage(f"Could not save prompt: {e}")

    def closeEvent(self, event):
        self._persist_current_prompt()
        super().closeEvent(event)

    def _setup_ui(self):
        # Left dock (shared across tabs): monster list
        self._monster_list = MonsterListWidget()
        self._monster_list.set_monsters(self._monsters)
        self._monster_list.monster_selected.connect(self._load_monster)
        self._monster_list.custom_created.connect(self._on_custom_created)
        self._monster_list.custom_deleted.connect(self._on_custom_deleted)
        monster_dock = QDockWidget("Monsters")
        monster_dock.setWidget(self._monster_list)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, monster_dock)

        # Create tab-owned widgets
        self._canvas = AnimationCanvas()
        self._frame_list = FrameListWidget()
        self._frame_list.frame_selected.connect(self._on_frame_selected)
        self._frame_list.frames_selected.connect(self._on_frames_selected)
        self._properties = PropertiesPanel()
        self._properties.offset_changed.connect(self._on_offset_changed)
        self._canvas.offset_changed.connect(self._on_offset_changed)
        self._sheet_view = SpritesheetView()
        self._gemini_panel = GeminiPanel()
        self._gemini_panel.frames_accepted.connect(self._on_gemini_frames_accepted)
        self._gemini_panel.sheet_built.connect(self._on_sheet_built)
        self._gemini_panel.output_received.connect(self._on_sheet_output)

        # Edit Frames tab: frame list | canvas | properties
        edit_splitter = QSplitter(Qt.Orientation.Horizontal)
        edit_splitter.addWidget(self._frame_list)
        edit_splitter.addWidget(self._canvas)
        edit_splitter.addWidget(self._properties)
        edit_splitter.setStretchFactor(0, 0)
        edit_splitter.setStretchFactor(1, 1)
        edit_splitter.setStretchFactor(2, 0)
        edit_splitter.setSizes([200, 800, 220])

        # Generate tab: Gemini panel | SpritesheetView
        gen_splitter = QSplitter(Qt.Orientation.Horizontal)
        gen_splitter.addWidget(self._gemini_panel)
        gen_splitter.addWidget(self._sheet_view)
        gen_splitter.setStretchFactor(0, 0)
        gen_splitter.setStretchFactor(1, 1)
        gen_splitter.setSizes([540, 660])

        self._tabs = QTabWidget()
        self._tabs.addTab(edit_splitter, "Edit Frames")
        self._tabs.addTab(gen_splitter, "Generate with Gemini")
        self.setCentralWidget(self._tabs)

    def _setup_toolbar(self):
        toolbar = QToolBar("Main")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

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

        # Hex grid toggle
        self._hex_grid_cb = QCheckBox("Hex grid")
        self._hex_grid_cb.toggled.connect(self._canvas.set_show_hex_grid)
        toolbar.addWidget(self._hex_grid_cb)

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
        edit_tab_action = QAction("&Edit Frames Tab", self)
        edit_tab_action.setShortcut(QKeySequence("Ctrl+1"))
        edit_tab_action.triggered.connect(lambda: self._tabs.setCurrentIndex(0))
        view_menu.addAction(edit_tab_action)

        gen_tab_action = QAction("&Generate with Gemini Tab", self)
        gen_tab_action.setShortcut(QKeySequence("Ctrl+2"))
        gen_tab_action.triggered.connect(lambda: self._tabs.setCurrentIndex(1))
        view_menu.addAction(gen_tab_action)

    def _load_monster_thumbnails(self):
        """Extract MONS32 thumbnails from AGG and apply to the monster list."""
        from .tools.icn_extractor import extract_icn
        from PySide6.QtGui import QPixmap

        mons32 = extract_icn("MONS32", BUILD_DIR, AGG_PATH)
        if not mons32:
            return

        # MONS32 frame indices map to MonsterType enum (1-based: frame 0 = PEASANT, etc.)
        # Build mapping: monster key → MONS32 frame index
        monster_index = {
            "peasant": 0, "archer": 1, "ranger": 2, "pikeman": 3, "veteran_pikeman": 4,
            "swordsman": 5, "master_swordsman": 6, "cavalry": 7, "champion": 8,
            "paladin": 9, "crusader": 10,
            "goblin": 11, "orc": 12, "orc_chief": 13, "wolf": 14,
            "ogre": 15, "ogre_lord": 16, "troll": 17, "war_troll": 18, "cyclops": 19,
            "sprite": 20, "dwarf": 21, "battle_dwarf": 22, "elf": 23, "grand_elf": 24,
            "druid": 25, "greater_druid": 26, "unicorn": 27, "phoenix": 28,
            "centaur": 29, "gargoyle": 30, "griffin": 31, "minotaur": 32, "minotaur_king": 33,
            "hydra": 34, "green_dragon": 35, "red_dragon": 36, "black_dragon": 37,
            "halfling": 38, "boar": 39, "iron_golem": 40, "steel_golem": 41,
            "roc": 42, "mage": 43, "archmage": 44, "giant": 45, "titan": 46,
            "skeleton": 47, "zombie": 48, "mutant_zombie": 49,
            "mummy": 50, "royal_mummy": 51, "vampire": 52, "vampire_lord": 53,
            "lich": 54, "power_lich": 55, "bone_dragon": 56,
            "rogue": 57, "nomad": 58, "ghost": 59, "genie": 60, "medusa": 61,
            "earth_element": 62, "air_element": 63, "fire_element": 64, "water_element": 65,
        }

        # For custom monsters, reuse the base monster's MONS32 frame.
        # Match by base_icn so new custom entries inherit their thumbnail automatically.
        icn_to_idx = {}
        for key, idx in monster_index.items():
            cfg = self._monsters.get(key)
            if cfg:
                icn_to_idx[cfg.base_icn] = idx
        for name, cfg in self._monsters.items():
            if name not in monster_index and cfg.base_icn in icn_to_idx:
                monster_index[name] = icn_to_idx[cfg.base_icn]

        thumbnails = {}
        for name, idx in monster_index.items():
            frame = mons32.get_frame(idx)
            if frame and not frame.is_placeholder:
                thumbnails[name] = frame.to_qpixmap()

        # Override the inherited base thumbnail with custom art where available:
        #   - has_custom_sprites=True → use a bbox-cropped frame from the PNG
        #     sequence (matches GetRGBACustomPortrait in agg_image.cpp)
        #   - palette_remap → apply the same remap table used at runtime so the
        #     mini-sprite matches what the engine actually draws
        from .tools.palette_remap import REMAP_TABLES, apply_palette_remap_fast, load_palette
        from .models.sprite_data import SpriteFrame

        palette = None
        needs_palette = any(
            cfg.palette_remap and cfg.palette_remap in REMAP_TABLES
            for cfg in self._monsters.values()
        )
        if needs_palette:
            pal_path = self._find_pal_file()
            if pal_path:
                palette = load_palette(pal_path)

        for name, cfg in self._monsters.items():
            if cfg.has_custom_sprites:
                png_thumb = self._build_custom_png_thumbnail(cfg.prefix)
                if png_thumb is not None:
                    thumbnails[name] = png_thumb
            elif cfg.palette_remap and palette is not None:
                remap_table = REMAP_TABLES.get(cfg.palette_remap)
                base_idx = icn_to_idx.get(cfg.base_icn)
                if remap_table and base_idx is not None:
                    base_frame = mons32.get_frame(base_idx)
                    if base_frame and not base_frame.is_placeholder:
                        new_img = apply_palette_remap_fast(base_frame.image, palette, remap_table)
                        thumbnails[name] = SpriteFrame(index=base_idx, image=new_img).to_qpixmap()

        self._monster_list.set_thumbnails(thumbnails)

    def _build_custom_png_thumbnail(self, prefix: str):
        """Build a thumbnail pixmap from a custom monster's PNG sprite sequence.

        Prefers frame 1 (the idle pose GetRGBACustomPortrait uses), falling back
        to the first non-placeholder PNG. The image is cropped to its opaque
        bbox so the figure fills the thumbnail rect after scaling.
        """
        from PIL import Image
        from .models.sprite_data import SpriteFrame

        if not SPRITES_DIR.exists():
            return None

        preferred = SPRITES_DIR / f"{prefix}_001.png"
        candidates = [preferred] if preferred.exists() else []
        candidates.extend(
            p for p in sorted(SPRITES_DIR.glob(f"{prefix}_???.png")) if p != preferred
        )

        for png in candidates:
            try:
                img = Image.open(png).convert("RGBA")
            except Exception:
                continue
            if img.width <= 1 or img.height <= 1:
                continue
            bbox = img.getbbox()
            if bbox:
                img = img.crop(bbox)
            if img.width <= 0 or img.height <= 0:
                continue
            return SpriteFrame(index=0, image=img).to_qpixmap()
        return None

    def _setup_statusbar(self):
        self._status = QStatusBar()
        self.setStatusBar(self._status)

    def _load_monster(self, name: str):
        """Load a monster by name from the manifest."""
        config = self._monsters.get(name)
        if not config:
            return

        # Persist the prompt of the outgoing monster before switching away.
        self._persist_current_prompt()

        self._current_config = config
        self._gemini_panel._prompt_edit.setPlainText(config.prompt)
        self._player.stop()
        self._play_btn.setChecked(False)

        # Load sprites depending on monster type
        self._custom_sprites = None
        self._base_sprites = None

        if config.has_custom_sprites:
            # Always extract base ICN first — needed for new custom monsters with no PNGs yet
            self._status.showMessage(f"Extracting {config.base_icn}...")
            self.repaint()
            from .tools.icn_extractor import extract_icn
            base = extract_icn(config.base_icn, BUILD_DIR, AGG_PATH)
            if base:
                self._base_sprites = base

            # Try loading custom PNG sprites (if any exist for any frame)
            has_any_custom = SPRITES_DIR.exists() and any(
                SPRITES_DIR.glob(f"{config.prefix}_???.png")
            )
            if has_any_custom:
                frame_count = config.frame_count or (base.frame_count if base else 0)
                self._custom_sprites = load_png_sprites(SPRITES_DIR, config.prefix, frame_count)
                if base:
                    apply_offsets_from_base(self._custom_sprites, base)
                    set_base_dimensions(self._custom_sprites, base)
                load_custom_offsets(SPRITES_DIR, config.prefix, self._custom_sprites)
                self._status.showMessage(f"Loaded {self._custom_sprites.frame_count} custom frames for {config.prefix}")
            elif base:
                # No custom PNGs yet — use base sprites as the starting view
                self._custom_sprites = base
                self._status.showMessage(
                    f"No custom sprites yet for {config.prefix} — showing base {config.base_icn}. "
                    f"Select frames and use Gemini to generate."
                )
            else:
                self._status.showMessage(f"Could not load {config.prefix}")
        elif config.palette_remap:
            # Palette-remapped monster: extract base ICN and apply remap
            self._status.showMessage(f"Extracting {config.base_icn} for palette remap...")
            self.repaint()
            self._load_palette_remapped(config)
        else:
            # Base-only monster: extract ICN directly from AGG
            self._status.showMessage(f"Extracting {config.base_icn}...")
            self.repaint()
            from .tools.icn_extractor import extract_icn
            base = extract_icn(config.base_icn, BUILD_DIR, AGG_PATH)
            if base:
                self._base_sprites = base
                self._custom_sprites = base  # show as the main view
                self._status.showMessage(f"Loaded {base.frame_count} frames for {config.base_monster}")
            else:
                self._status.showMessage(f"Could not extract {config.base_icn} from AGG")

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
        from .tools.icn_extractor import resolve_extractor
        extractor = resolve_extractor(BUILD_DIR)
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

        # Update Gemini panel with selected frames from both sources
        prefix = self._current_config.prefix if self._current_config else ""
        custom_frames = []
        base_frames = []
        if self._custom_sprites:
            custom_frames = [self._custom_sprites.get_frame(i) for i in indices
                             if self._custom_sprites.get_frame(i) is not None]
        if self._base_sprites:
            base_frames = [self._base_sprites.get_frame(i) for i in indices
                           if self._base_sprites.get_frame(i) is not None]
        self._gemini_panel.set_selected_frames(custom_frames, base_frames, indices, prefix)

    def _on_offset_changed(self, frame_index: int, offset_x: int, offset_y: int):
        """Offset changed via spinbox or canvas drag — update memory, sync UI, persist."""
        sprites = self._custom_sprites or self._base_sprites
        if not sprites:
            return
        frame = sprites.get_frame(frame_index)
        if not frame:
            return
        frame.offset_x = offset_x
        frame.offset_y = offset_y
        self._canvas.update()
        # Sync spinboxes (no-op when the change came from them; _updating guards re-emit)
        self._properties.set_frame(frame)

        # Persist for custom monsters so the engine picks up the new offsets
        if self._current_config and self._current_config.has_custom_sprites:
            override = FrameOverride(
                offset_x=offset_x,
                offset_y=offset_y,
                display_width=frame.base_width,
                display_height=frame.base_height,
            )
            save_custom_offsets(SPRITES_DIR, self._current_config.prefix, {frame_index: override})
            self._status.showMessage(
                f"Saved offset for {self._current_config.prefix} frame {frame_index}: ({offset_x}, {offset_y})"
            )

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

    def _on_gemini_frames_accepted(self, frame_indices: list[int], images: list):
        """Gemini frames were accepted and saved — reload the monster to see changes."""
        if self._current_config:
            self._status.showMessage(f"Accepted {len(frame_indices)} frames — reloading...")
            self._load_monster(self._current_config.name)

    def _on_custom_created(self, new_config, prompt: str):
        """Handle new custom monster creation."""
        new_config.prompt = prompt

        # Add to in-memory config and persist to manifest
        self._monsters[new_config.name] = new_config
        update_manifest_entry(new_config.name, {
            "prefix": new_config.prefix,
            "base_icn": new_config.base_icn,
            "bin_file": new_config.bin_file,
            "display_name": new_config.display_name,
            "base_monster": new_config.base_monster,
            "faction": "Custom",
            "has_custom_sprites": True,
            "prompt": prompt,
        })

        # Write the frame-0 placeholder PNG the engine uses as a presence probe
        # for the custom sprite set (see loadCustomSpritesFromPNG / GetRGBACustomFrames).
        self._ensure_placeholder_png(new_config.prefix)

        # Refresh monster list
        self._monster_list.set_monsters(self._monsters)
        self._load_monster_thumbnails()

        # Select the new monster and load it (this populates the prompt via _load_monster)
        self._monster_list.select_monster(new_config.name)

        # Switch to Generate tab
        self._tabs.setCurrentIndex(1)

        self._status.showMessage(f"Created custom monster '{new_config.base_monster}' from {new_config.base_icn}")

    @staticmethod
    def _ensure_placeholder_png(prefix: str) -> None:
        """Create a 1x1 transparent {prefix}_000.png if it doesn't already exist.

        The game's loadCustomSpritesFromPNG probes frame 0 to decide whether a
        custom sprite set is present at all; without the placeholder the engine
        falls back to the base ICN even when frames 1..N are on disk.
        """
        from PIL import Image

        SPRITES_DIR.mkdir(parents=True, exist_ok=True)
        placeholder = SPRITES_DIR / f"{prefix}_000.png"
        if placeholder.exists():
            return
        Image.new("RGBA", (1, 1), (0, 0, 0, 0)).save(placeholder)

    def _on_custom_deleted(self, name: str):
        """Remove a custom monster from the manifest and refresh the UI."""
        if name not in self._monsters:
            return

        # Drop the outgoing prompt-persist attempt — the entry is about to vanish.
        if self._current_config and self._current_config.name == name:
            self._current_config = None

        del self._monsters[name]
        try:
            remove_manifest_entry(name)
        except OSError as e:
            self._status.showMessage(f"Could not remove from manifest: {e}")
            return

        self._monster_list.set_monsters(self._monsters)
        self._load_monster_thumbnails()

        # Pick a fallback monster so the editor isn't in a dangling state.
        if self._monsters:
            fallback = next(iter(self._monsters))
            self._monster_list.select_monster(fallback)

        self._status.showMessage(f"Removed custom monster '{name}'")

    def _on_sheet_built(self, image, cells, label):
        """Show built input sheet in the Sprite Sheet panel."""
        self._sheet_view.set_sheet(image, cells, f"Input: {label}")

    def _on_sheet_output(self, image, cells, label):
        """Show Gemini output sheet in the Sprite Sheet panel."""
        self._sheet_view.set_sheet(image, cells, f"Output: {label}")

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
