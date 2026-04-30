"""Extract base ICN sprites and BIN files from AGG archives."""

import os
import subprocess
import tempfile
from pathlib import Path

from ..models.sprite_data import SpriteCollection, SpriteFrame
from .icn_parser import parse_icn
from .palette_remap import load_palette


_PROJECT_ROOT = Path(__file__).parent.parent.parent.parent


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


def extract_icn(
    icn_name: str,
    build_dir: Path = DEFAULT_BUILD_DIR,
    agg_path: Path = DEFAULT_AGG_PATH,
) -> SpriteCollection | None:
    """Extract ICN sprites from AGG with proper per-pixel transparency.

    Uses the Python ICN parser to decode the binary ICN format directly,
    producing RGBA sprites with correct alpha from the transform layer.
    No more gray background — transparent pixels have alpha=0.

    Returns a SpriteCollection or None on failure.
    """
    with tempfile.TemporaryDirectory(prefix="sprite_editor_") as tmpdir:
        tmp = Path(tmpdir)

        extractor = resolve_extractor(build_dir)
        if not extractor.exists() or not agg_path.exists():
            print(f"Build tools not found: extractor={extractor.exists()}, agg={agg_path.exists()}")
            return None

        try:
            subprocess.run(
                [str(extractor), str(tmp), str(agg_path)],
                capture_output=True, text=True, timeout=60,
            )
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            print(f"Extractor failed: {e}")
            return None

        icn_file = _find_file(tmp, f"{icn_name}.ICN")
        pal_file = _find_file(tmp, "KB.PAL")

        if not icn_file:
            print(f"ICN file not found after extraction: {icn_name}.ICN")
            return None
        if not pal_file:
            print("KB.PAL not found after extraction")
            return None

        # Parse ICN directly with proper transparency
        icn_data = icn_file.read_bytes()
        palette = load_palette(pal_file)
        frames = parse_icn(icn_data, palette)

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
    """Extract a BIN file from AGG using extractor.

    Returns raw bytes or None on failure.
    """
    extractor = resolve_extractor(build_dir)

    if not extractor.exists() or not agg_path.exists():
        return None

    with tempfile.TemporaryDirectory(prefix="sprite_editor_bin_") as tmpdir:
        tmp = Path(tmpdir)

        try:
            subprocess.run(
                [str(extractor), str(tmp), str(agg_path)],
                capture_output=True, text=True, timeout=60,
            )
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return None

        bin_file = _find_file(tmp, bin_name)
        if not bin_file:
            return None

        return bin_file.read_bytes()
