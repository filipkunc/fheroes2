"""Hero specialty editor panel.

Edits one HeroSpecialty: a tagged choice of None / Spell / Unit / Resource with
the corresponding payload fields. Emits `specialty_changed` whenever any field
mutates so the main window can persist back to hero_manifest.json.
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel,
    QComboBox, QSpinBox, QStackedWidget, QGroupBox,
    QListWidget, QListWidgetItem, QPushButton,
)
from PySide6.QtCore import Signal, Qt

from ..models.hero_config import HeroSpecialty


# Spells the engine actually supports (excludes RANDOM* placeholders, NONE, PETRIFY).
# Order roughly follows spell.h.
SPELL_OPTIONS = [
    "FIREBALL", "FIREBLAST", "LIGHTNINGBOLT", "CHAINLIGHTNING",
    "TELEPORT", "CURE", "MASSCURE", "RESURRECT", "RESURRECTTRUE",
    "HASTE", "MASSHASTE", "SLOW", "MASSSLOW",
    "BLIND", "BLESS", "MASSBLESS", "STONESKIN", "STEELSKIN",
    "CURSE", "MASSCURSE", "HOLYWORD", "HOLYSHOUT",
    "ANTIMAGIC", "DISPEL", "MASSDISPEL", "ARROW", "BERSERKER",
    "ARMAGEDDON", "ELEMENTALSTORM", "METEORSHOWER", "PARALYZE", "HYPNOTIZE",
    "COLDRAY", "COLDRING", "DISRUPTINGRAY",
    "DEATHRIPPLE", "DEATHWAVE", "DRAGONSLAYER", "BLOODLUST",
    "ANIMATEDEAD", "MIRRORIMAGE", "SHIELD", "MASSSHIELD",
    "SUMMONEELEMENT", "SUMMONAELEMENT", "SUMMONFELEMENT", "SUMMONWELEMENT",
    "EARTHQUAKE",
    "VIEWMINES", "VIEWRESOURCES", "VIEWARTIFACTS", "VIEWTOWNS", "VIEWHEROES", "VIEWALL",
    "IDENTIFYHERO", "SUMMONBOAT", "DIMENSIONDOOR", "TOWNGATE", "TOWNPORTAL",
    "VISIONS", "HAUNT",
    "SETEGUARDIAN", "SETAGUARDIAN", "SETFGUARDIAN", "SETWGUARDIAN",
]

# Monsters from monster.h (excludes UNKNOWN, RANDOM_MONSTER variants), grouped
# by faction so the multi-select list reads naturally and tier chains stay
# adjacent. Headers (str values starting with "@") render as bold separators.
MONSTER_GROUPS: list[tuple[str, list[str]]] = [
    ("Knight", [
        "PEASANT", "ARCHER", "RANGER", "PIKEMAN", "VETERAN_PIKEMAN", "MAID",
        "SWORDSMAN", "MASTER_SWORDSMAN", "CAVALRY", "CHAMPION",
        "PALADIN", "CRUSADER", "AVENGER",
    ]),
    ("Barbarian", [
        "GOBLIN", "ORC", "ORC_CHIEF", "WOLF",
        "OGRE", "OGRE_LORD", "TROLL", "WAR_TROLL", "CYCLOPS", "THOR",
    ]),
    ("Sorceress", [
        "SPRITE", "DWARF", "BATTLE_DWARF", "ELF", "GRAND_ELF",
        "DRUID", "GREATER_DRUID", "UNICORN", "PHOENIX",
    ]),
    ("Warlock", [
        "CENTAUR", "GARGOYLE", "GRIFFIN", "MINOTAUR", "MINOTAUR_KING",
        "HYDRA", "GREEN_DRAGON", "RED_DRAGON", "BLACK_DRAGON", "AZURE_DRAGON",
    ]),
    ("Wizard", [
        "HALFLING", "BOAR", "IRON_GOLEM", "STEEL_GOLEM",
        "ROC", "MAGE", "ARCHMAGE", "GIANT", "TITAN",
    ]),
    ("Necromancer", [
        "SKELETON", "ZOMBIE", "MUTANT_ZOMBIE",
        "MUMMY", "ROYAL_MUMMY", "VAMPIRE", "VAMPIRE_LORD",
        "LICH", "POWER_LICH", "BONE_DRAGON", "BLOOD_DRAGON", "SUCCUBUS",
    ]),
    ("Neutral", [
        "ROGUE", "NOMAD", "GHOST", "GENIE", "MEDUSA", "DACHSHUND",
        "EARTH_ELEMENT", "AIR_ELEMENT", "FIRE_ELEMENT", "WATER_ELEMENT",
    ]),
]

# Flat list (preserves group order) — used for codegen / validation.
MONSTER_OPTIONS = [m for _, ms in MONSTER_GROUPS for m in ms]

# Quick-pick groups for unit specialty. Each entry is (label, [monster_ids]).
# Picking one of these checks all the listed monsters and unchecks everything
# else, so a Dragons specialist can be set up in one click.
UNIT_PRESETS: list[tuple[str, list[str]]] = [
    ("All Dragons", [
        "GREEN_DRAGON", "RED_DRAGON", "BLACK_DRAGON",
        "BONE_DRAGON", "AZURE_DRAGON", "BLOOD_DRAGON",
    ]),
    ("Pikeman line", ["PIKEMAN", "VETERAN_PIKEMAN", "MAID"]),
    ("Swordsman line", ["SWORDSMAN", "MASTER_SWORDSMAN"]),
    ("Cavalry/Paladin", ["CAVALRY", "CHAMPION", "PALADIN", "CRUSADER", "AVENGER"]),
    ("Orcs", ["ORC", "ORC_CHIEF"]),
    ("Ogres/Trolls", ["OGRE", "OGRE_LORD", "TROLL", "WAR_TROLL"]),
    ("Cyclops/Thor", ["CYCLOPS", "THOR"]),
    ("Dwarves", ["DWARF", "BATTLE_DWARF"]),
    ("Elves/Druids", ["ELF", "GRAND_ELF", "DRUID", "GREATER_DRUID"]),
    ("Centaur/Minotaur", ["CENTAUR", "MINOTAUR", "MINOTAUR_KING"]),
    ("Mages/Giants", ["MAGE", "ARCHMAGE", "GIANT", "TITAN"]),
    ("Vampires/Liches", ["VAMPIRE", "VAMPIRE_LORD", "LICH", "POWER_LICH"]),
    ("Mummies", ["MUMMY", "ROYAL_MUMMY"]),
    ("Elementals", ["EARTH_ELEMENT", "AIR_ELEMENT", "FIRE_ELEMENT", "WATER_ELEMENT"]),
]

RESOURCE_OPTIONS = ["WOOD", "MERCURY", "ORE", "SULFUR", "CRYSTAL", "GEMS", "GOLD"]

KIND_LABELS = [
    ("none", "None"),
    ("spell", "Spell boost"),
    ("unit", "Unit boost"),
    ("resource", "Resource bonus"),
]


def _humanize(enum_name: str) -> str:
    """Convert e.g. 'CHAIN_LIGHTNING' / 'CHAINLIGHTNING' to a presentable label."""
    s = enum_name.replace("_", " ").lower()
    return s.title()


class SpecialtyPanel(QWidget):
    """Editor for a single HeroSpecialty."""

    specialty_changed = Signal(HeroSpecialty)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._suspend_signals = False  # guard during programmatic field updates

        outer = QVBoxLayout(self)
        outer.setContentsMargins(8, 8, 8, 8)

        title = QLabel("Hero Specialty")
        title_font = title.font()
        title_font.setBold(True)
        title_font.setPointSize(title_font.pointSize() + 1)
        title.setFont(title_font)
        outer.addWidget(title)

        self._hero_label = QLabel("(no hero selected)")
        outer.addWidget(self._hero_label)

        # Kind selector
        kind_row = QHBoxLayout()
        kind_row.addWidget(QLabel("Kind:"))
        self._kind_combo = QComboBox()
        for value, label in KIND_LABELS:
            self._kind_combo.addItem(label, value)
        self._kind_combo.currentIndexChanged.connect(self._on_kind_changed)
        kind_row.addWidget(self._kind_combo, 1)
        outer.addLayout(kind_row)

        # Stacked payload forms
        self._stack = QStackedWidget()
        self._stack.addWidget(self._build_none_form())     # 0 = none
        self._stack.addWidget(self._build_spell_form())    # 1 = spell
        self._stack.addWidget(self._build_unit_form())     # 2 = unit
        self._stack.addWidget(self._build_resource_form()) # 3 = resource
        outer.addWidget(self._stack)

        outer.addStretch(1)

        self._current_specialty = HeroSpecialty()

    # ---------- form builders ----------

    def _build_none_form(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.addWidget(QLabel("This hero has no specialty."))
        layout.addStretch(1)
        return w

    def _build_spell_form(self) -> QWidget:
        box = QGroupBox("Spell boost")
        outer = QVBoxLayout(box)

        form = QFormLayout()

        self._spell_combo = QComboBox()
        for sp in SPELL_OPTIONS:
            self._spell_combo.addItem(_humanize(sp), sp)
        self._spell_combo.currentIndexChanged.connect(self._emit_change)
        form.addRow("Spell:", self._spell_combo)

        self._spell_damage = QSpinBox()
        self._spell_damage.setRange(0, 500)
        self._spell_damage.setSuffix(" %")
        self._spell_damage.setToolTip("Bonus damage applied on top of base spell damage")
        self._spell_damage.valueChanged.connect(self._emit_change)
        form.addRow("Damage bonus:", self._spell_damage)

        self._spell_sp_cost = QSpinBox()
        self._spell_sp_cost.setRange(0, 20)
        self._spell_sp_cost.setToolTip("Spell points the hero saves when casting this spell")
        self._spell_sp_cost.valueChanged.connect(self._emit_change)
        form.addRow("SP cost reduction:", self._spell_sp_cost)

        outer.addLayout(form)

        # Note about implicit spell-book inclusion (engine-side wiring lands in
        # Phase 2; the hint goes in now so designers know the contract).
        hint = QLabel(
            "<i>The selected spell is automatically added to the hero's spell book.</i>"
        )
        hint.setWordWrap(True)
        outer.addWidget(hint)

        return box

    def _build_unit_form(self) -> QWidget:
        box = QGroupBox("Unit boost")
        outer = QVBoxLayout(box)

        # Stat spinners (apply to every checked unit equally).
        stats_form = QFormLayout()

        self._unit_atk = QSpinBox()
        self._unit_atk.setRange(-10, 20)
        self._unit_atk.valueChanged.connect(self._emit_change)
        stats_form.addRow("Attack bonus:", self._unit_atk)

        self._unit_def = QSpinBox()
        self._unit_def.setRange(-10, 20)
        self._unit_def.valueChanged.connect(self._emit_change)
        stats_form.addRow("Defense bonus:", self._unit_def)

        self._unit_speed = QSpinBox()
        self._unit_speed.setRange(-5, 10)
        self._unit_speed.valueChanged.connect(self._emit_change)
        stats_form.addRow("Speed bonus:", self._unit_speed)

        outer.addLayout(stats_form)

        # Quick-pick presets: pick a thematic group (e.g. all Dragons) in one click.
        preset_row = QHBoxLayout()
        preset_row.addWidget(QLabel("Quick pick:"))
        self._preset_combo = QComboBox()
        self._preset_combo.addItem("(choose preset…)", None)
        for label, units in UNIT_PRESETS:
            self._preset_combo.addItem(label, units)
        self._preset_combo.activated.connect(self._on_preset_picked)
        preset_row.addWidget(self._preset_combo, 1)

        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear_unit_selection)
        preset_row.addWidget(clear_btn)
        outer.addLayout(preset_row)

        outer.addWidget(QLabel("Units (check all that get the bonus):"))

        # Checkable list grouped by faction. Headers (faction names) are
        # non-selectable label rows; monster rows are user-checkable items.
        self._unit_list = QListWidget()
        self._unit_list.itemChanged.connect(self._on_unit_item_changed)
        for faction, members in MONSTER_GROUPS:
            header = QListWidgetItem(faction)
            header.setFlags(Qt.ItemFlag.NoItemFlags)
            font = header.font()
            font.setBold(True)
            header.setFont(font)
            self._unit_list.addItem(header)
            for m in members:
                item = QListWidgetItem(f"  {_humanize(m)}")
                item.setData(Qt.ItemDataRole.UserRole, m)
                item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                item.setCheckState(Qt.CheckState.Unchecked)
                self._unit_list.addItem(item)
        outer.addWidget(self._unit_list, 1)

        return box

    def _on_unit_item_changed(self, _item):
        self._emit_change()

    def _set_checked_units(self, units: list[str]):
        """Programmatically check the named units, uncheck the rest, suppressing
        per-item itemChanged emits — we emit a single change at the end."""
        target = set(units)
        self._unit_list.blockSignals(True)
        try:
            for i in range(self._unit_list.count()):
                item = self._unit_list.item(i)
                key = item.data(Qt.ItemDataRole.UserRole)
                if key is None:
                    continue
                want = key in target
                state = Qt.CheckState.Checked if want else Qt.CheckState.Unchecked
                if item.checkState() != state:
                    item.setCheckState(state)
        finally:
            self._unit_list.blockSignals(False)

    def _on_preset_picked(self, idx: int):
        if idx <= 0:
            return
        units = self._preset_combo.itemData(idx) or []
        self._set_checked_units(list(units))
        # Reset combo back to the placeholder so the user can re-pick the same preset.
        self._preset_combo.blockSignals(True)
        self._preset_combo.setCurrentIndex(0)
        self._preset_combo.blockSignals(False)
        self._emit_change()

    def _clear_unit_selection(self):
        self._set_checked_units([])
        self._emit_change()

    def _checked_units(self) -> list[str]:
        out: list[str] = []
        for i in range(self._unit_list.count()):
            item = self._unit_list.item(i)
            if item.checkState() == Qt.CheckState.Checked:
                key = item.data(Qt.ItemDataRole.UserRole)
                if key:
                    out.append(key)
        return out

    def _build_resource_form(self) -> QWidget:
        box = QGroupBox("Resource bonus (per day)")
        form = QFormLayout(box)

        self._resource_combo = QComboBox()
        for r in RESOURCE_OPTIONS:
            self._resource_combo.addItem(_humanize(r), r)
        self._resource_combo.currentIndexChanged.connect(self._on_resource_kind_changed)
        form.addRow("Resource:", self._resource_combo)

        self._resource_amount = QSpinBox()
        self._resource_amount.setRange(0, 10000)
        self._resource_amount.setSingleStep(1)
        self._resource_amount.valueChanged.connect(self._emit_change)
        form.addRow("Amount per day:", self._resource_amount)

        return box

    # ---------- public API ----------

    def set_hero(self, hero_name: str, display_name: str, specialty: HeroSpecialty):
        """Load a hero's specialty into the editor without firing change signals."""
        self._suspend_signals = True
        try:
            self._current_specialty = specialty
            self._hero_label.setText(display_name)

            # Kind combo
            kind = specialty.kind if specialty.kind in {"none", "spell", "unit", "resource"} else "none"
            kind_index = next(i for i, (v, _) in enumerate(KIND_LABELS) if v == kind)
            self._kind_combo.setCurrentIndex(kind_index)

            # Spell payload
            self._select_combo_data(self._spell_combo, specialty.spell or SPELL_OPTIONS[0])
            self._spell_damage.setValue(specialty.damage_bonus_percent)
            self._spell_sp_cost.setValue(specialty.sp_cost_reduction)

            # Unit payload — checked unit list + stat bonuses
            self._set_checked_units(list(specialty.units))
            self._unit_atk.setValue(specialty.atk_bonus)
            self._unit_def.setValue(specialty.def_bonus)
            self._unit_speed.setValue(specialty.speed_bonus)

            # Resource payload
            self._select_combo_data(self._resource_combo, specialty.resource or RESOURCE_OPTIONS[0])
            self._resource_amount.setValue(specialty.amount_per_day)

            self._stack.setCurrentIndex(kind_index)
        finally:
            self._suspend_signals = False

    # ---------- internals ----------

    @staticmethod
    def _select_combo_data(combo: QComboBox, value: str):
        for i in range(combo.count()):
            if combo.itemData(i) == value:
                combo.setCurrentIndex(i)
                return
        combo.setCurrentIndex(0)

    def _on_kind_changed(self, index: int):
        self._stack.setCurrentIndex(index)
        self._emit_change()

    def _on_resource_kind_changed(self):
        # Adjust the default amount when switching resource type:
        # gold defaults to 250/day, all others to 1/day, but only if user hasn't
        # set anything yet.
        if self._suspend_signals:
            self._emit_change()
            return
        current = self._resource_amount.value()
        if current == 0 or current == 1 or current == 250:
            new_default = 250 if self._resource_combo.currentData() == "GOLD" else 1
            self._resource_amount.blockSignals(True)
            self._resource_amount.setValue(new_default)
            self._resource_amount.blockSignals(False)
        self._emit_change()

    def _emit_change(self):
        if self._suspend_signals:
            return

        kind = self._kind_combo.currentData()
        spec = HeroSpecialty(kind=kind)
        if kind == "spell":
            spec.spell = self._spell_combo.currentData() or ""
            spec.damage_bonus_percent = self._spell_damage.value()
            spec.sp_cost_reduction = self._spell_sp_cost.value()
        elif kind == "unit":
            spec.units = self._checked_units()
            spec.atk_bonus = self._unit_atk.value()
            spec.def_bonus = self._unit_def.value()
            spec.speed_bonus = self._unit_speed.value()
        elif kind == "resource":
            spec.resource = self._resource_combo.currentData() or ""
            spec.amount_per_day = self._resource_amount.value()

        self._current_specialty = spec
        self.specialty_changed.emit(spec)
