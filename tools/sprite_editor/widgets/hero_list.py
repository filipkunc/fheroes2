"""Hero selector list with mini portraits from MINIPORT.icn.

Mirrors MonsterListWidget but for heroes — heroes are grouped by race + campaign
flag instead of by faction.
"""

from PySide6.QtWidgets import (
    QListWidget, QListWidgetItem, QAbstractItemView,
    QVBoxLayout, QWidget,
)
from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QIcon, QPixmap

from ..models.hero_config import HeroConfig


THUMB_SIZE = 36

_RACE_ORDER = ["Knight", "Barbarian", "Sorceress", "Warlock", "Wizard", "Necromancer"]
_CAMPAIGN_ORDER = ["", "SW", "PoL", "Debug"]


class HeroListWidget(QWidget):
    """Visual hero selector grouped by race, with campaign heroes appended."""

    hero_selected = Signal(str)  # emits hero key

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self._list = QListWidget()
        self._list.setIconSize(QSize(THUMB_SIZE, THUMB_SIZE))
        self._list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self._list.currentItemChanged.connect(self._on_item_changed)
        layout.addWidget(self._list)

        self._heroes: dict[str, HeroConfig] = {}

    def set_heroes(self, heroes: dict[str, HeroConfig]):
        self._heroes = heroes
        self._list.clear()

        # Group: (campaign, race) -> list of (key, cfg), preserving hero_id order within group
        groups: dict[tuple[str, str], list[tuple[str, HeroConfig]]] = {}
        for name, cfg in sorted(heroes.items(), key=lambda kv: kv[1].hero_id):
            groups.setdefault((cfg.campaign, cfg.race), []).append((name, cfg))

        # Headers: SW Campaign / PoL Campaign / Debug come last; non-campaign heroes by race first
        for campaign in _CAMPAIGN_ORDER:
            if campaign == "":
                # Roster heroes — group by race
                for race in _RACE_ORDER:
                    bucket = groups.get((campaign, race))
                    if not bucket:
                        continue
                    self._add_header(race)
                    for name, cfg in bucket:
                        self._add_hero(name, cfg)
            else:
                # Campaign heroes — flat list under one header
                campaign_label = {"SW": "Succession Wars", "PoL": "Price of Loyalty", "Debug": "Debug"}[campaign]
                bucket: list[tuple[str, HeroConfig]] = []
                for race in _RACE_ORDER:
                    bucket.extend(groups.get((campaign, race), []))
                # Catch any race that's not in _RACE_ORDER (shouldn't happen but safe)
                for (c, r), entries in groups.items():
                    if c == campaign and r not in _RACE_ORDER:
                        bucket.extend(entries)
                if not bucket:
                    continue
                self._add_header(campaign_label)
                for name, cfg in bucket:
                    self._add_hero(name, cfg)

    def _add_header(self, label: str):
        header = QListWidgetItem(label)
        header.setFlags(Qt.ItemFlag.NoItemFlags)
        font = header.font()
        font.setBold(True)
        header.setFont(font)
        self._list.addItem(header)

    def _add_hero(self, name: str, cfg: HeroConfig):
        item = QListWidgetItem(f"  {cfg.display_name}")
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

    def select_hero(self, name: str):
        for row in range(self._list.count()):
            item = self._list.item(row)
            if item.data(Qt.ItemDataRole.UserRole) == name:
                self._list.setCurrentRow(row)
                break

    def _on_item_changed(self, current: QListWidgetItem, previous: QListWidgetItem):
        if current is None:
            return
        name = current.data(Qt.ItemDataRole.UserRole)
        if name:
            self.hero_selected.emit(name)
