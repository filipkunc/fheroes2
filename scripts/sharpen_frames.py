#!/usr/bin/env python3
"""Sharpen sprite frames via Real-ESRGAN super-resolution.

For each input PNG: 4x upscale with Real-ESRGAN, then downsample back to the
original size with Lanczos. Net effect: sharper detail at the same resolution,
without changing pose or composition.

By default, sharpened outputs are written to
    <sprites-dir>/sharpened_<model>/<prefix>_NNN.png
so you can inspect them before deciding to overwrite the originals. Pass
--in-place to overwrite the source files directly (destructive — the
originals are only recoverable via `git restore` if they're committed).

Requires the realesrgan-ncnn-vulkan binary. Default location:
    <repo>/tools/realesrgan/realesrgan-ncnn-vulkan.exe

Usage:
    python scripts/sharpen_frames.py                            # succubus 1-6, anime model, side-by-side
    python scripts/sharpen_frames.py --frames 1-6
    python scripts/sharpen_frames.py --model realesrgan-x4plus  # general model
    python scripts/sharpen_frames.py --frames 1 --in-place      # overwrite frame 1
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BINARY = REPO_ROOT / "tools" / "realesrgan" / "realesrgan-ncnn-vulkan.exe"
DEFAULT_SPRITES = REPO_ROOT / "files" / "data" / "sprites"


def parse_frames(spec: str) -> list[int]:
    if "," in spec:
        return [int(x) for x in spec.split(",")]
    if "-" in spec:
        a, b = spec.split("-", 1)
        return list(range(int(a), int(b) + 1))
    return [int(spec)]


def find_binary(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit)
        if not p.exists():
            sys.exit(f"binary not found: {p}")
        return p
    if DEFAULT_BINARY.exists():
        return DEFAULT_BINARY
    onpath = shutil.which("realesrgan-ncnn-vulkan") or shutil.which("realesrgan-ncnn-vulkan.exe")
    if onpath:
        return Path(onpath)
    sys.exit(
        f"realesrgan-ncnn-vulkan not found at {DEFAULT_BINARY} or on PATH.\n"
        "Download the Windows zip from\n"
        "  https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.2.5.0\n"
        f"and extract into {DEFAULT_BINARY.parent}"
    )


def sharpen(src: Path, dst: Path, binary: Path, model: str) -> None:
    with Image.open(src) as im:
        orig_size = im.size

    with tempfile.TemporaryDirectory() as tmp:
        up_path = Path(tmp) / "up.png"
        cmd = [
            str(binary),
            "-i", str(src),
            "-o", str(up_path),
            "-n", model,
            "-s", "4",
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            sys.exit(f"realesrgan failed on {src.name}:\n{result.stderr}")

        with Image.open(up_path) as up:
            up.load()
            sharpened = up.resize(orig_size, Image.Resampling.LANCZOS)

    dst.parent.mkdir(parents=True, exist_ok=True)
    sharpened.save(dst)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--prefix", default="succubus")
    parser.add_argument("--frames", default="1-6", help="e.g. '1-6', '1,3,5', '7'")
    parser.add_argument(
        "--model", default="realesrgan-x4plus-anime",
        choices=["realesrgan-x4plus", "realesrgan-x4plus-anime",
                 "realesrnet-x4plus", "realesr-animevideov3"],
        help="Default 'realesrgan-x4plus-anime' preserves stylized edges; "
             "'realesrgan-x4plus' is for photographic content (smooths stylized art).",
    )
    parser.add_argument("--binary", default=None,
                        help="Path to realesrgan-ncnn-vulkan binary")
    parser.add_argument("--sprites-dir", default=str(DEFAULT_SPRITES))
    parser.add_argument("--in-place", action="store_true",
                        help="Overwrite source files (destructive). "
                             "Default writes to sharpened_<model>/ for inspection.")
    args = parser.parse_args()

    binary = find_binary(args.binary)
    sprites_dir = Path(args.sprites_dir)
    frames = parse_frames(args.frames)
    out_dir = sprites_dir if args.in_place else sprites_dir / f"sharpened_{args.model}"

    print(f"binary: {binary}")
    print(f"model:  {args.model}")
    print(f"input:  {sprites_dir}/{args.prefix}_NNN.png")
    print(f"output: {out_dir}{'  (IN-PLACE — overwriting sources)' if args.in_place else ''}")
    print(f"frames: {len(frames)}")
    print()

    for n in frames:
        src = sprites_dir / f"{args.prefix}_{n:03d}.png"
        if not src.exists():
            print(f"skip {src.name}: not found")
            continue
        dst = out_dir / src.name
        with Image.open(src) as im:
            size = im.size
        print(f"sharpening {src.name} ({size[0]}x{size[1]})...", flush=True)
        sharpen(src, dst, binary, args.model)
        print(f"  -> {dst.relative_to(sprites_dir.parent.parent)}")


if __name__ == "__main__":
    main()
