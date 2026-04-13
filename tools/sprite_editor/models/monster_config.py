"""Monster configuration loaded from the manifest JSON."""

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass
class MonsterConfig:
    name: str
    prefix: str
    frame_count: int
    base_icn: str
    bin_file: str
    base_monster: str
    has_custom_sprites: bool = True
    palette_remap: str = ""  # key into REMAP_TABLES if using palette remap


def load_manifest(manifest_path: Path | None = None) -> dict[str, MonsterConfig]:
    """Load monster definitions from manifest JSON.

    Returns dict keyed by monster name (e.g. 'azure_dragon').
    """
    if manifest_path is None:
        manifest_path = Path(__file__).parent.parent / "resources" / "monster_manifest.json"

    with open(manifest_path) as f:
        data = json.load(f)

    configs = {}
    for name, entry in data.items():
        configs[name] = MonsterConfig(
            name=name,
            prefix=entry["prefix"],
            frame_count=entry["frame_count"],
            base_icn=entry["base_icn"],
            bin_file=entry["bin_file"],
            base_monster=entry.get("base_monster", ""),
            has_custom_sprites=entry.get("has_custom_sprites", True),
            palette_remap=entry.get("palette_remap", ""),
        )
    return configs
