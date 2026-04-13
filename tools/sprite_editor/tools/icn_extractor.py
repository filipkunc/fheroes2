"""Extract base ICN sprites using the build/icn2img tool."""

import subprocess
import tempfile
from pathlib import Path

from ..models.sprite_data import SpriteCollection, load_bmp_sprites


# Default paths
DEFAULT_BUILD_DIR = Path(__file__).parent.parent.parent.parent / "build"
DEFAULT_AGG_PATH = Path.home() / "Games" / "Heroic" / "HoMM 2 Gold" / "DATA" / "HEROES2.AGG"


def _find_sprite_subdir(output_dir: Path, icn_name: str) -> Path | None:
    """Find the subdirectory created by icn2img containing numbered PNGs."""
    # icn2img creates e.g. output_dir/draggree/ with 000.png, 001.png, ...
    candidate = output_dir / icn_name.lower()
    if candidate.is_dir():
        return candidate
    # Fallback: any subdir containing numbered PNGs
    for p in output_dir.iterdir():
        if p.is_dir() and list(p.glob("[0-9]*.png")):
            return p
    # Maybe files are directly in output_dir
    if list(output_dir.glob("[0-9]*.png")) or list(output_dir.glob("[0-9]*.bmp")):
        return output_dir
    return None


def _find_file(root: Path, name: str) -> Path | None:
    """Find a file by name (case-insensitive) recursively under root."""
    # Try exact match first
    direct = root / name
    if direct.exists():
        return direct
    # Try lowercase
    direct = root / name.lower()
    if direct.exists():
        return direct
    # Search recursively
    for p in root.rglob("*"):
        if p.name.lower() == name.lower():
            return p
    return None


def extract_icn(
    icn_name: str,
    build_dir: Path = DEFAULT_BUILD_DIR,
    agg_path: Path = DEFAULT_AGG_PATH,
) -> SpriteCollection | None:
    """Extract ICN sprites from AGG using icn2img.

    First extracts the ICN and PAL files from AGG using extractor,
    then converts ICN to individual BMP/PNG files using icn2img.

    Returns a SpriteCollection or None on failure.
    """
    extractor = build_dir / "extractor"
    icn2img = build_dir / "icn2img"

    if not extractor.exists() or not icn2img.exists():
        print(f"Build tools not found in {build_dir}")
        return None

    if not agg_path.exists():
        print(f"AGG file not found: {agg_path}")
        return None

    with tempfile.TemporaryDirectory(prefix="sprite_editor_") as tmpdir:
        tmp = Path(tmpdir)

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

        # Convert ICN to individual PNG sprites
        output_dir = tmp / "sprites"
        output_dir.mkdir()

        try:
            result = subprocess.run(
                [str(icn2img), str(output_dir), str(pal_file), str(icn_file)],
                capture_output=True, text=True, timeout=30,
            )
            if result.returncode != 0:
                print(f"icn2img failed: {result.stderr}")
                return None
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            print(f"icn2img failed: {e}")
            return None

        # icn2img creates a subdirectory named after the ICN (e.g. "draggree/")
        # containing numbered PNG files (000.png, 001.png, ...)
        sprite_subdir = _find_sprite_subdir(output_dir, icn_name)
        if not sprite_subdir:
            print(f"No sprite output directory found after icn2img")
            return None

        return load_bmp_sprites(sprite_subdir)


def extract_bin(
    bin_name: str,
    build_dir: Path = DEFAULT_BUILD_DIR,
    agg_path: Path = DEFAULT_AGG_PATH,
) -> bytes | None:
    """Extract a BIN file from AGG using extractor.

    Returns raw bytes or None on failure.
    """
    extractor = build_dir / "extractor"

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
