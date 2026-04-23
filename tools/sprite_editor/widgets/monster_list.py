"""Monster selector list with sprite thumbnails from MONS32."""

from pathlib import Path

from PySide6.QtWidgets import (
    QListWidget, QListWidgetItem, QAbstractItemView, QPushButton,
    QVBoxLayout, QWidget, QDialog, QFormLayout, QLineEdit, QDialogButtonBox,
    QLabel,
)
from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QIcon, QPixmap

from ..models.monster_config import MonsterConfig


THUMB_SIZE = 36


class CreateCustomDialog(QDialog):
    """Dialog for creating a new custom monster from a base."""

    def __init__(self, base_config: MonsterConfig, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Create Custom from {base_config.base_monster}")
        self.setMinimumWidth(350)

        layout = QFormLayout(self)

        self._name_edit = QLineEdit()
        self._name_edit.setPlaceholderText("e.g. fire_titan")
        layout.addRow("Name (key):", self._name_edit)

        self._display_edit = QLineEdit()
        self._display_edit.setPlaceholderText("e.g. Fire Titan")
        layout.addRow("Display name:", self._display_edit)

        self._prompt_edit = QLineEdit()
        self._prompt_edit.setPlaceholderText("e.g. Transform into a fire titan with flames...")
        layout.addRow("Prompt:", self._prompt_edit)

        # Show base info
        layout.addRow("Base ICN:", QLabel(base_config.base_icn))
        layout.addRow("BIN file:", QLabel(base_config.bin_file))

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)

        self._base = base_config

    @property
    def monster_name(self) -> str:
        return self._name_edit.text().strip().lower().replace(" ", "_")

    @property
    def display_name(self) -> str:
        return self._display_edit.text().strip() or self.monster_name

    @property
    def prompt(self) -> str:
        return self._prompt_edit.text().strip()

    def new_config(self) -> MonsterConfig:
        return MonsterConfig(
            name=self.monster_name,
            prefix=self.monster_name,
            base_icn=self._base.base_icn,
            bin_file=self._base.bin_file,
            base_monster=self._base.display_name or self._base.base_monster,
            display_name=self.display_name,
            faction="Custom",
            has_custom_sprites=True,
        )


class MonsterListWidget(QWidget):
    """Visual monster selector with MONS32 thumbnails grouped by faction."""

    monster_selected = Signal(str)  # emits monster key
    custom_created = Signal(MonsterConfig, str)  # (new config, prompt)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self._list = QListWidget()
        self._list.setIconSize(QSize(THUMB_SIZE, THUMB_SIZE))
        self._list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self._list.currentItemChanged.connect(self._on_item_changed)
        layout.addWidget(self._list)

        self._create_btn = QPushButton("Create Custom")
        self._create_btn.setToolTip("Create a new custom monster from the selected base")
        self._create_btn.clicked.connect(self._on_create_custom)
        layout.addWidget(self._create_btn)

        self._monsters: dict[str, MonsterConfig] = {}

    def set_monsters(self, monsters: dict[str, MonsterConfig]):
        self._monsters = monsters
        self._list.clear()
        faction_order = ["Custom", "Castle", "Barbarian", "Sorceress",
                         "Warlock", "Wizard", "Necromancer", "Neutral"]

        by_faction: dict[str, list[tuple[str, MonsterConfig]]] = {}
        for name, cfg in monsters.items():
            faction = cfg.faction or "Other"
            by_faction.setdefault(faction, []).append((name, cfg))

        for faction in faction_order:
            if faction not in by_faction:
                continue
            header = QListWidgetItem(faction)
            header.setFlags(Qt.ItemFlag.NoItemFlags)
            font = header.font()
            font.setBold(True)
            header.setFont(font)
            self._list.addItem(header)

            for name, cfg in by_faction[faction]:
                label = cfg.display_name or cfg.base_monster or name
                item = QListWidgetItem(f"  {label}")
                item.setData(Qt.ItemDataRole.UserRole, name)
                self._list.addItem(item)

    def set_thumbnails(self, thumbnails: dict[str, QPixmap]):
        for row in range(self._list.count()):
            item = self._list.item(row)
            name = item.data(Qt.ItemDataRole.UserRole)
            if name and name in thumbnails:
                pixmap = thumbnails[name]
                scaled = pixmap.scaled(
                    THUMB_SIZE, THUMB_SIZE,
                    Qt.AspectRatioMode.KeepAspectRatio,
                    Qt.TransformationMode.SmoothTransformation,
                )
                item.setIcon(QIcon(scaled))

    def select_monster(self, name: str):
        for row in range(self._list.count()):
            item = self._list.item(row)
            if item.data(Qt.ItemDataRole.UserRole) == name:
                self._list.setCurrentRow(row)
                break

    def _selected_key(self) -> str | None:
        item = self._list.currentItem()
        if item:
            return item.data(Qt.ItemDataRole.UserRole)
        return None

    def _on_item_changed(self, current: QListWidgetItem, previous: QListWidgetItem):
        if current is None:
            return
        name = current.data(Qt.ItemDataRole.UserRole)
        if name:
            self.monster_selected.emit(name)

    def _on_create_custom(self):
        key = self._selected_key()
        if not key or key not in self._monsters:
            return

        base_cfg = self._monsters[key]
        dialog = CreateCustomDialog(base_cfg, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        name = dialog.monster_name
        if not name:
            return

        new_cfg = dialog.new_config()
        self.custom_created.emit(new_cfg, dialog.prompt)
