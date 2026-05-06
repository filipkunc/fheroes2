"""Extract base ICN sprites and BIN files from AGG archives."""

import atexit
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from ..models.sprite_data import SpriteCollection, SpriteFrame
from .icn_parser import parse_icn
from .palette_remap import load_palette


_PROJECT_ROOT = Path(__file__).parent.parent.parent.parent

# Session-wide cache: extracting the full AGG (~30MB, hundreds of files) takes
# a few seconds. Without caching, every extract_icn() call re-runs the extractor,
# which makes browsing many ICNs (e.g. switching heroes) painfully slow.
# We extract once per (build_dir, agg_path, agg_mtime) into a temp dir kept alive
# for the lifetime of the process, then look up individual ICNs from that dir.
_extracted_cache: dict[tuple[str, str, int], Path] = {}
_palette_cache: dict[Path, object] = {}


def _cleanup_extracted_cache():
    for path in _extracted_cache.values():
        shutil.rmtree(path, ignore_errors=True)
    _extracted_cache.clear()


atexit.register(_cleanup_extracted_cache)


def _default_build_dir() -> Path:
    for candidate in (_PROJECT_ROOT / "build" / "Release", _PROJECT_ROOT / "build"):
        if candidate.is_dir():
            return candidate
    return _PROJECT_ROOT / "build"


def _default_agg_path() -> Path:
    env = os.environ.get("FHEROES2_AGG")
    if env:
        return Path(env)
    for p in (
        _PROJECT_ROOT.parent / "fheroes2_gog" / "DATA" / "HEROES2.AGG",
        Path.home() / "Games" / "Heroic" / "HoMM 2 Gold" / "DATA" / "HEROES2.AGG",
    ):
        if p.exists():
            return p
    return Path.home() / "Games" / "Heroic" / "HoMM 2 Gold" / "DATA" / "HEROES2.AGG"


def resolve_extractor(build_dir: Path) -> Path:
    """Return the extractor binary path, preferring .exe on Windows."""
    for name in ("extractor.exe", "extractor"):
        candidate = build_dir / name
        if candidate.exists():
            return candidate
    return build_dir / "extractor"


def ensure_extractor_built(build_dir: Path, *, log=print) -> Path | None:
    """Return the path to a working extractor binary, building it if needed.

    The fheroes2 CMake project gates `extractor` (and the other AGG tools) behind
    `ENABLE_TOOLS=ON`. When the user's existing build was configured without it,
    we reconfigure the cache and build only the `extractor` target. Returns the
    binary path on success, or None if the build is unavailable / failed.

    `log` receives single-line status messages — defaults to `print` so the
    output is visible in the launching shell.
    """
    existing = resolve_extractor(build_dir)
    if existing.exists():
        return existing

    # The cache lives in the CMake build dir; on Visual Studio that's the parent
    # of the per-config Release/ output dir, on make/ninja it IS the output dir.
    cmake_dir = build_dir if (build_dir / "CMakeCache.txt").exists() else build_dir.parent
    cache = cmake_dir / "CMakeCache.txt"
    if not cache.exists():
        log(f"No CMake cache near {build_dir}; cannot auto-build extractor.")
        return None

    cache_text = cache.read_text(errors="replace")
    if "ENABLE_TOOLS:BOOL=OFF" in cache_text:
        log("Reconfiguring CMake with ENABLE_TOOLS=ON...")
        result = subprocess.run(
            ["cmake", "-DENABLE_TOOLS=ON", str(cmake_dir)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            log(f"CMake reconfigure failed:\n{result.stderr.strip()}")
            return None

    log("Building extractor target (this may take a minute)...")
    result = subprocess.run(
        ["cmake", "--build", str(cmake_dir), "--target", "extractor", "--config", "Release"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        # Show the tail of the build log — the head is mostly project setup noise.
        tail = "\n".join(result.stdout.splitlines()[-20:])
        log(f"Extractor build failed:\n{tail}\n{result.stderr.strip()}")
        return None

    found = resolve_extractor(build_dir)
    if found.exists():
        log(f"Built extractor: {found}")
        return found
    log("Extractor build reported success but the binary was not found.")
    return None


# Default paths
DEFAULT_BUILD_DIR = _default_build_dir()
DEFAULT_AGG_PATH = _default_agg_path()


def _find_file(root: Path, name: str) -> Path | None:
    """Find a file by name (case-insensitive) recursively under root."""
    direct = root / name
    if direct.exists():
        return direct
    direct = root / name.lower()
    if direct.exists():
        return direct
    for p in root.rglob("*"):
        if p.name.lower() == name.lower():
            return p
    return None


def _extract_agg(build_dir: Path, agg_path: Path) -> Path | None:
    """Extract AGG archive to a temp directory. Returns the temp path or None."""
    extractor = resolve_extractor(build_dir)
    if not extractor.exists():
        print(f"Extractor not found: {extractor}")
        return None
    if not agg_path.exists():
        print(f"AGG file not found: {agg_path}")
        return None

    tmp = Path(tempfile.mkdtemp(prefix="sprite_editor_"))
    try:
        subprocess.run(
            [str(extractor), str(tmp), str(agg_path)],
            capture_output=True, text=True, timeout=60,
        )
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print(f"Extractor failed: {e}")
        return None

    return tmp


def _find_expansion_agg(base_agg: Path) -> Path | None:
    """Return HEROES2X.AGG if it sits next to the base AGG. Price of Loyalty
    portraits and animations (Solmyr through Jarkonas, PORT0060+) live there,
    not in HEROES2.AGG. The engine opens both at runtime — we mirror that.
    Lookup is case-insensitive to tolerate Windows / GOG installs."""
    parent = base_agg.parent
    if not parent.is_dir():
        return None
    target = "heroes2x.agg"
    for entry in parent.iterdir():
        if entry.name.lower() == target and entry.is_file():
            return entry
    return None


def _get_or_extract_agg(build_dir: Path, agg_path: Path) -> Path | None:
    """Return a cached extraction of the AGG (plus HEROES2X.AGG if present),
    extracting once on first call.

    The cache key includes both AGGs' mtimes so editing either invalidates the
    cache automatically. The temp dir lives until process exit (atexit cleanup).
    """
    if not agg_path.exists():
        return None
    try:
        base_mtime = agg_path.stat().st_mtime_ns
    except OSError:
        return None

    expansion_agg = _find_expansion_agg(agg_path)
    expansion_mtime = 0
    if expansion_agg is not None:
        try:
            expansion_mtime = expansion_agg.stat().st_mtime_ns
        except OSError:
            expansion_agg = None

    key = (str(build_dir), str(agg_path), base_mtime, str(expansion_agg or ""), expansion_mtime)
    cached = _extracted_cache.get(key)
    if cached is not None and cached.exists():
        return cached

    extractor = resolve_extractor(build_dir)
    if not extractor.exists():
        return None

    tmp = Path(tempfile.mkdtemp(prefix="sprite_editor_agg_"))
    try:
        # The extractor writes into <dst>/<agg_stem>/, so HEROES2.AGG → tmp/HEROES2/
        # and HEROES2X.AGG → tmp/HEROES2X/. Pass both in one invocation; we merge
        # them after to mirror the engine's expansion-first lookup.
        cmd = [str(extractor), str(tmp), str(agg_path)]
        if expansion_agg is not None:
            cmd.append(str(expansion_agg))
        subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print(f"Extractor failed: {e}")
        shutil.rmtree(tmp, ignore_errors=True)
        return None

    # Merge HEROES2X/ over HEROES2/ so MINIPORT.ICN (and any other replaced asset)
    # resolves to the expansion's 71-frame version, matching what the engine sees.
    if expansion_agg is not None:
        base_subdir = tmp / agg_path.stem
        ext_subdir = tmp / expansion_agg.stem
        if base_subdir.is_dir() and ext_subdir.is_dir():
            for src in ext_subdir.iterdir():
                dst = base_subdir / src.name
                try:
                    if dst.exists():
                        dst.unlink()
                    shutil.move(str(src), str(dst))
                except OSError as e:
                    print(f"Could not merge {src.name} from expansion: {e}")
            shutil.rmtree(ext_subdir, ignore_errors=True)

    # Drop stale cache entries for the same (build_dir, base_agg) that we just
    # superseded — keeps the cache from growing if the user edits AGGs.
    for stale_key in [k for k in _extracted_cache if k[0] == key[0] and k[1] == key[1] and k != key]:
        shutil.rmtree(_extracted_cache.pop(stale_key), ignore_errors=True)

    _extracted_cache[key] = tmp
    return tmp


def _get_palette(pal_file: Path):
    cached = _palette_cache.get(pal_file)
    if cached is not None:
        return cached
    palette = load_palette(pal_file)
    _palette_cache[pal_file] = palette
    return palette


def extract_icn(
    icn_name: str,
    build_dir: Path = DEFAULT_BUILD_DIR,
    agg_path: Path = DEFAULT_AGG_PATH,
) -> SpriteCollection | None:
    """Extract ICN sprites from AGG with proper per-pixel transparency.

    Uses the Python ICN parser to decode the binary ICN format directly,
    producing RGBA sprites with correct alpha from the transform layer.
    The full AGG extraction is cached for the session — first call is slow
    (a few seconds), subsequent calls are essentially free.

    Returns a SpriteCollection or None on failure.
    """
    root = _get_or_extract_agg(build_dir, agg_path)
    if root is None:
        return None

    icn_file = _find_file(root, f"{icn_name}.ICN")
    pal_file = _find_file(root, "KB.PAL")

    if not icn_file:
        print(f"ICN file not found after extraction: {icn_name}.ICN")
        return None
    if not pal_file:
        print("KB.PAL not found after extraction")
        return None

    palette = _get_palette(pal_file)
    frames = parse_icn(icn_file.read_bytes(), palette)
    if not frames:
        print(f"Failed to parse ICN: {icn_name}")
        return None

    collection = SpriteCollection(prefix="base")
    collection.frames = frames
    return collection


def extract_bin(
    bin_name: str,
    build_dir: Path = DEFAULT_BUILD_DIR,
    agg_path: Path = DEFAULT_AGG_PATH,
) -> bytes | None:
    """Extract a BIN file from AGG. Uses the same session-cached extraction as
    extract_icn(), so calling both for the same AGG only pays the extract cost
    once.
    """
    root = _get_or_extract_agg(build_dir, agg_path)
    if root is None:
        return None

    bin_file = _find_file(root, bin_name)
    if not bin_file:
        return None

    return bin_file.read_bytes()
