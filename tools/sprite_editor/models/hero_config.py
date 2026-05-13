"""Hero configuration loaded from the hero manifest JSON."""

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class HeroSpecialty:
    """One hero's specialty. `kind` selects which payload is meaningful."""

    kind: str = "none"  # "none" | "spell" | "unit" | "resource"

    # spell payload
    spell: str = ""  # spell enum name e.g. "FIREBALL"
    damage_bonus_percent: int = 0
    sp_cost_reduction: int = 0

    # unit payload — list of monster enum names. The bonus applies to any troop
    # whose monster ID is in this list, so picking a tier chain (Pikeman +
    # Veteran Pikeman) or a thematic group (all dragons) keeps the bonus after
    # upgrades without needing extra logic on the engine side.
    units: list[str] = field(default_factory=list)
    atk_bonus: int = 0
    def_bonus: int = 0
    speed_bonus: int = 0

    # resource payload
    resource: str = ""  # one of WOOD/MERCURY/ORE/SULFUR/CRYSTAL/GEMS/GOLD
    amount_per_day: int = 0

    def to_json(self) -> dict[str, Any]:
        if self.kind == "spell":
            return {
                "kind": "spell",
                "spell": self.spell,
                "damage_bonus_percent": self.damage_bonus_percent,
                "sp_cost_reduction": self.sp_cost_reduction,
            }
        if self.kind == "unit":
            return {
                "kind": "unit",
                "units": list(self.units),
                "atk_bonus": self.atk_bonus,
                "def_bonus": self.def_bonus,
                "speed_bonus": self.speed_bonus,
            }
        if self.kind == "resource":
            return {
                "kind": "resource",
                "resource": self.resource,
                "amount_per_day": self.amount_per_day,
            }
        return {"kind": "none"}

    @classmethod
    def from_json(cls, data: dict[str, Any]) -> "HeroSpecialty":
        kind = data.get("kind", "none")
        if kind == "spell":
            return cls(
                kind="spell",
                spell=data.get("spell", ""),
                damage_bonus_percent=int(data.get("damage_bonus_percent", 0)),
                sp_cost_reduction=int(data.get("sp_cost_reduction", 0)),
            )
        if kind == "unit":
            # Tolerate the legacy single-unit shape ("unit": "PIKEMAN") so an
            # older manifest still loads after the multi-select migration.
            units = data.get("units")
            if not units:
                legacy = data.get("unit", "")
                units = [legacy] if legacy else []
            return cls(
                kind="unit",
                units=[str(u) for u in units if u],
                atk_bonus=int(data.get("atk_bonus", 0)),
                def_bonus=int(data.get("def_bonus", 0)),
                speed_bonus=int(data.get("speed_bonus", 0)),
            )
        if kind == "resource":
            return cls(
                kind="resource",
                resource=data.get("resource", ""),
                amount_per_day=int(data.get("amount_per_day", 0)),
            )
        return cls(kind="none")


@dataclass
class HeroConfig:
    name: str  # manifest key, e.g. "lord_kilburn"
    enum: str  # C++ enum name, e.g. "LORDKILBURN"
    hero_id: int
    race: str
    display_name: str
    port_index: int  # PORT00xx frame (== ICN id offset from PORT0000)
    campaign: str = ""  # "" | "SW" | "PoL" | "Debug"
    prefix: str = ""
    has_custom_sprites: bool = False
    has_custom_sprites_small: bool = False
    portrait_prompt: str = ""  # per-hero Gemini transform prompt for hi-res portrait
    portrait_prompt_small: str = ""  # per-hero Gemini transform prompt for the mini variant
    flux_portrait_prompt: str = ""  # per-hero FLUX (ComfyUI) prompt — different style than Gemini
    specialty: HeroSpecialty = field(default_factory=HeroSpecialty)


def _default_manifest_path() -> Path:
    return Path(__file__).parent.parent / "resources" / "hero_manifest.json"


def load_hero_manifest(manifest_path: Path | None = None) -> dict[str, HeroConfig]:
    """Load hero definitions from manifest JSON. Keyed by hero name."""
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        data = json.load(f)

    configs: dict[str, HeroConfig] = {}
    for name, entry in data.items():
        configs[name] = HeroConfig(
            name=name,
            enum=entry["enum"],
            hero_id=int(entry["hero_id"]),
            race=entry.get("race", ""),
            display_name=entry.get("display_name", name),
            port_index=int(entry.get("port_index", 0)),
            campaign=entry.get("campaign", ""),
            prefix=entry.get("prefix", f"hero_{name}"),
            has_custom_sprites=bool(entry.get("has_custom_sprites", False)),
            has_custom_sprites_small=bool(entry.get("has_custom_sprites_small", False)),
            portrait_prompt=entry.get("portrait_prompt", ""),
            portrait_prompt_small=entry.get("portrait_prompt_small", ""),
            flux_portrait_prompt=entry.get("flux_portrait_prompt", ""),
            specialty=HeroSpecialty.from_json(entry.get("specialty", {"kind": "none"})),
        )
    return configs


def save_hero_manifest(configs: dict[str, HeroConfig], manifest_path: Path | None = None) -> None:
    """Write the entire manifest back to disk. Stable key order = hero_id."""
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    sorted_items = sorted(configs.items(), key=lambda kv: kv[1].hero_id)
    out: dict[str, dict[str, Any]] = {}
    for name, cfg in sorted_items:
        out[name] = {
            "enum": cfg.enum,
            "hero_id": cfg.hero_id,
            "race": cfg.race,
            "display_name": cfg.display_name,
            "port_index": cfg.port_index,
            "campaign": cfg.campaign,
            "prefix": cfg.prefix,
            "has_custom_sprites": cfg.has_custom_sprites,
            "specialty": cfg.specialty.to_json(),
        }

    with open(manifest_path, "w") as f:
        json.dump(out, f, indent=2)


def update_hero_specialty(name: str, specialty: HeroSpecialty, manifest_path: Path | None = None) -> None:
    """Persist a specialty change for a single hero without rewriting unrelated fields."""
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        manifest = json.load(f)

    if name not in manifest:
        return

    manifest[name]["specialty"] = specialty.to_json()
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)


def update_hero_manifest_entry(name: str, fields: dict, manifest_path: Path | None = None) -> None:
    """Merge `fields` into the manifest entry for `name`. Mirrors monster_config's
    update_manifest_entry — used for one-off field updates (has_custom_sprites,
    portrait_prompt, etc.) without serialising the whole HeroConfig dataclass."""
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        manifest = json.load(f)

    if name not in manifest:
        return

    manifest[name].update(fields)
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
