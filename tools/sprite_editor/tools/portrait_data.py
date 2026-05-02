"""Lazy-extract STRIP frame + MONH dimensions for the portrait preview.

The preview needs to render the same composite the engine builds in
`MonsterDialogElement::draw`:
    1. STRIP[12] (94×105 gold frame border, transparent interior)
    2. STRIP[race] (82×93 race-specific landscape) blitted at (6,6) inside STRIP[12]
    3. The hi-res custom portrait fitted into a box that expands from MONH
       toward the race-frame interior as portrait_zoom grows.

Source data lives in HEROES2.AGG. We extract once per session into a tmpdir and
keep PIL images cached in module state. If the AGG / extractor isn't available
the preview falls back to a synthesised brown rectangle so the editor still works
without game data.
"""

from __future__ import annotations

import struct
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

from .icn_extractor import (
    DEFAULT_AGG_PATH,
    DEFAULT_BUILD_DIR,
    _find_file,
    resolve_extractor,
)
from .icn_parser import parse_icn
from .palette_remap import load_palette


# Monster prefix → race STRIP frame index. Race ordering matches
# `renderMonsterFrame` in src/fheroes2/gui/ui_monster.cpp:
#     KNGT→4, BARB→5, SORC→6, WRLK→7, WZRD→8, NECR→9, default→10
MONSTER_RACE_STRIP: dict[str, int] = {
    "azure_dragon": 7,   # WRLK
    "blood_dragon": 9,   # NECR
    "thor": 8,           # WZRD
    "avenger": 4,        # KNGT
    "succubus": 5,       # BARB
    "dachshund": 5,      # BARB
}

# Monster prefix → base monster's in-game ID. ICNMonh() returns
#     MONH(id - 1) for vanilla monsters; custom monsters reuse the base.
# Numbers come from the MonsterType enum in src/fheroes2/monster/monster.h
# (PEASANT=1 .. WATER_ELEMENT=65).
MONSTER_BASE_ID: dict[str, int] = {
    "azure_dragon": 36,  # GREEN_DRAGON → MONH0035
    "blood_dragon": 56,  # BONE_DRAGON  → MONH0055
    "thor": 47,          # TITAN        → MONH0046
    "avenger": 11,       # CRUSADER     → MONH0010
    "succubus": 31,      # GARGOYLE     → MONH0030
    "dachshund": 15,     # WOLF         → MONH0014
}


_agg_tmpdir: Path | None = None
_strip_frames_cache: list | None = None  # list[SpriteFrame] | None
_monh_cache: dict[str, object] = {}      # base_id (int) → SpriteFrame


def _ensure_agg_extracted() -> Path | None:
    """Run extractor.exe on HEROES2.AGG once per session and cache the temp dir."""
    global _agg_tmpdir
    if _agg_tmpdir is not None and _agg_tmpdir.exists():
        return _agg_tmpdir

    extractor = resolve_extractor(DEFAULT_BUILD_DIR)
    if not extractor.exists() or not DEFAULT_AGG_PATH.exists():
        return None

    tmp = Path(tempfile.mkdtemp(prefix="sprite_editor_portrait_"))
    try:
        subprocess.run(
            [str(extractor), str(tmp), str(DEFAULT_AGG_PATH)],
            capture_output=True, text=True, timeout=60,
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None

    _agg_tmpdir = tmp
    return tmp


def _get_palette():
    tmp = _ensure_agg_extracted()
    if tmp is None:
        return None
    pal_file = _find_file(tmp, "KB.PAL")
    return load_palette(pal_file) if pal_file else None


def get_strip_frames() -> list | None:
    """Parse and cache all 13 STRIP frames. Returns None if AGG isn't available."""
    global _strip_frames_cache
    if _strip_frames_cache is not None:
        return _strip_frames_cache

    tmp = _ensure_agg_extracted()
    if tmp is None:
        return None
    strip_file = _find_file(tmp, "STRIP.ICN")
    palette = _get_palette()
    if not strip_file or palette is None:
        return None
    _strip_frames_cache = parse_icn(strip_file.read_bytes(), palette)
    return _strip_frames_cache


def get_monh_for_monster(prefix: str):
    """Return the MONH SpriteFrame the engine uses for this monster.

    For custom monsters (Succubus, Thor, …) this is the base monster's MONH —
    the engine clones it and overwrites pixels with the hi-res portrait.
    Returns None if the AGG isn't available or the prefix isn't known.
    """
    base_id = MONSTER_BASE_ID.get(prefix)
    if base_id is None:
        return None

    if base_id in _monh_cache:
        return _monh_cache[base_id]

    tmp = _ensure_agg_extracted()
    palette = _get_palette()
    if tmp is None or palette is None:
        return None

    monh_index = base_id - 1  # PEASANT=1 → MONH0000
    monh_file = _find_file(tmp, f"MONH{monh_index:04d}.ICN")
    if not monh_file:
        return None

    frames = parse_icn(monh_file.read_bytes(), palette)
    if not frames:
        return None

    _monh_cache[base_id] = frames[0]
    return frames[0]


def peek_monh_dims(prefix: str) -> tuple[int, int, int, int] | None:
    """Read MONH header for `prefix` and return (width, height, offset_x, offset_y).

    Cheap shortcut — parses just the 13-byte header instead of the full sprite.
    Returns None if data isn't available.
    """
    base_id = MONSTER_BASE_ID.get(prefix)
    if base_id is None:
        return None
    tmp = _ensure_agg_extracted()
    if tmp is None:
        return None
    monh_index = base_id - 1
    monh_file = _find_file(tmp, f"MONH{monh_index:04d}.ICN")
    if not monh_file:
        return None
    data = monh_file.read_bytes()
    if len(data) < 19:
        return None
    ox, oy, w, h, _af, _data_off = struct.unpack_from("<hhHHBI", data, 6)
    return (int(w), int(h), int(ox), int(oy))
