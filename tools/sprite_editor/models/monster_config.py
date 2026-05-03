"""Monster configuration loaded from the manifest JSON."""

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass
class MonsterConfig:
    name: str
    prefix: str
    base_icn: str
    bin_file: str
    base_monster: str = ""
    display_name: str = ""  # shown in UI; falls back to base_monster for base monsters
    faction: str = ""
    frame_count: int = 0  # 0 = auto-detect from ICN
    has_custom_sprites: bool = False
    palette_remap: str = ""
    prompt: str = ""  # Gemini transform prompt, persisted per custom monster
    base_building_icn: str = ""  # full-building TWN sprite (e.g. TWNKDW_0) — used by the live castle scene
    base_building_icon_icn: str = ""  # construction-tile CSTL sprite (e.g. CSTLKNGT) — used by the build dialog and Well
    base_building_icon_index: int = 0  # frame index into base_building_icon_icn (e.g. 19 for Thatched Hut)
    building_prompt: str = ""  # Gemini prompt for the full building art (TWN*UP*A)
    building_icon_prompt: str = ""  # Gemini prompt for the small icon art (CSTL*[index])


def _default_manifest_path() -> Path:
    return Path(__file__).parent.parent / "resources" / "monster_manifest.json"


def load_manifest(manifest_path: Path | None = None) -> dict[str, MonsterConfig]:
    """Load monster definitions from manifest JSON.

    Returns dict keyed by monster name (e.g. 'azure_dragon').
    """
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        data = json.load(f)

    configs = {}
    for name, entry in data.items():
        configs[name] = MonsterConfig(
            name=name,
            prefix=entry["prefix"],
            frame_count=entry.get("frame_count", 0),
            base_icn=entry["base_icn"],
            bin_file=entry["bin_file"],
            base_monster=entry.get("base_monster", ""),
            display_name=entry.get("display_name", ""),
            faction=entry.get("faction", ""),
            has_custom_sprites=entry.get("has_custom_sprites", False),
            palette_remap=entry.get("palette_remap", ""),
            prompt=entry.get("prompt", ""),
            base_building_icn=entry.get("base_building_icn", ""),
            base_building_icon_icn=entry.get("base_building_icon_icn", ""),
            base_building_icon_index=entry.get("base_building_icon_index", 0),
            building_prompt=entry.get("building_prompt", ""),
            building_icon_prompt=entry.get("building_icon_prompt", ""),
        )
    return configs


def update_manifest_entry(name: str, fields: dict, manifest_path: Path | None = None) -> None:
    """Merge `fields` into the manifest entry for `name` and write the file back.

    Creates the entry if it doesn't exist. Preserves fields not in `fields`.
    """
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        manifest = json.load(f)

    entry = manifest.get(name, {})
    entry.update(fields)
    manifest[name] = entry

    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)


def remove_manifest_entry(name: str, manifest_path: Path | None = None) -> bool:
    """Drop `name` from the manifest. Returns True if an entry was removed."""
    if manifest_path is None:
        manifest_path = _default_manifest_path()

    with open(manifest_path) as f:
        manifest = json.load(f)

    if name not in manifest:
        return False

    del manifest[name]
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    return True
