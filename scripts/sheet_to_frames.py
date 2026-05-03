#!/usr/bin/env python3
"""Slice a Gemini-style input sheet, remove background, sharpen each cell.

Pipeline:
  1. Auto-detect grid (cols=ceil(sqrt(N)), rows=ceil(N/cols)) and cell size.
  2. Slice each cell.
  3. Remove background by alpha-masking pixels matching the corner pixel of
     the sheet (hue-window detection mirrored from gemini_client._compute_bg_mask).
  4. Downsample each cell from sheet upscale (e.g. 8x) back to native size
     (slice_w / upscale). The sheet was NEAREST-upscaled by the sprite editor,
     so this is nearly lossless and gives Real-ESRGAN a low-res input to work
     with — without downsampling first, ESRGAN sees crisp NEAREST blocks and
     barely modifies them.
  5. Sharpen with Real-ESRGAN (4x by default) — ends at native * out-scale.

Outputs go to <sprites-dir>/sharpened_from_sheet/<prefix>_NNN.png by default.
Use --in-place to overwrite <sprites-dir>/<prefix>_NNN.png directly.

Usage:
  python scripts/sheet_to_frames.py \
    --sheet files/data/sprites/sheets/succubus_001_002_003_004_005_006_input.png \
    --frames 1-6 --prefix succubus

  python scripts/sheet_to_frames.py --sheet <sheet> --frames 1-6 --in-place
"""
import argparse
import colorsys
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BINARY = REPO_ROOT / "tools" / "realesrgan" / "realesrgan-ncnn-vulkan.exe"
DEFAULT_SPRITES = REPO_ROOT / "files" / "data" / "sprites"
GRID_GAP_NATIVE = 2  # matches gemini_client.GRID_GAP


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
        "Download from https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.2.5.0"
    )


def detect_bg_mask(cell: np.ndarray, bg: tuple[int, int, int]) -> np.ndarray:
    """Mirror of gemini_client._compute_bg_mask."""
    r, g, b = bg
    bg_h, bg_s, _ = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    if bg_s < 0.15:
        rgb = cell[:, :, :3].astype(np.int16)
        target = np.array(bg, dtype=np.int16).reshape(1, 1, 3)
        return np.max(np.abs(rgb - target), axis=2) < 40
    img = Image.fromarray(cell[:, :, :3], "RGB")
    hsv = np.array(img.convert("HSV")).transpose(2, 0, 1).astype(np.float32)
    hue = hsv[0] / 255.0 * 360.0
    sat = hsv[1] / 255.0
    target = bg_h * 360.0
    diff = np.abs(hue - target)
    diff = np.minimum(diff, 360.0 - diff)
    return (diff < 30.0) & (sat > 0.2)


def slice_sheet(sheet_path: Path, n_frames: int, upscale: int) -> tuple[list[Image.Image], tuple[int, int, int]]:
    """Slice a sheet into per-cell RGBA images with bg removed and downsampled
    back to native size (slice_w / upscale).

    Returns (cells, bg_color). Uses the same grid layout as build_sheet:
    cols=ceil(sqrt(N)), rows=ceil(N/cols), gap=GRID_GAP_NATIVE*upscale.

    Cells are returned at NATIVE resolution (downsampled from sheet upscale)
    so super-resolution models have low-res input to work with.
    """
    sheet = Image.open(sheet_path).convert("RGB")
    sw, sh = sheet.size
    arr = np.array(sheet)

    cols = max(1, math.ceil(math.sqrt(n_frames)))
    rows = max(1, (n_frames + cols - 1) // cols)
    gap = GRID_GAP_NATIVE * upscale

    cell_w_total = (sw + gap) // cols
    cell_h_total = (sh + gap) // rows
    slice_w = cell_w_total - gap
    slice_h = cell_h_total - gap
    native_w = slice_w // upscale
    native_h = slice_h // upscale

    bg = tuple(int(v) for v in arr[0, 0])
    print(f"  grid: {cols}x{rows}, slice {slice_w}x{slice_h}, native {native_w}x{native_h}, bg={bg}")

    cells: list[Image.Image] = []
    for i in range(n_frames):
        col = i % cols
        row = i // cols
        x0 = col * cell_w_total
        y0 = row * cell_h_total
        cell_rgb = arr[y0:y0 + slice_h, x0:x0 + slice_w]
        cell_rgba = np.zeros((slice_h, slice_w, 4), dtype=np.uint8)
        cell_rgba[:, :, :3] = cell_rgb
        cell_rgba[:, :, 3] = 255
        mask = detect_bg_mask(cell_rgba, bg)
        cell_rgba[mask, 3] = 0
        cell_img = Image.fromarray(cell_rgba, "RGBA")
        # Downsample to native (sheet was NEAREST-upscaled, so this is ~lossless)
        if (native_w, native_h) != cell_img.size:
            cell_img = cell_img.resize((native_w, native_h), Image.Resampling.LANCZOS)
        cells.append(cell_img)
    return cells, bg


def upscale_image(src_path: Path, dst_path: Path, binary: Path, model: str) -> None:
    """Run Real-ESRGAN 4x on src and save the upscaled output as-is."""
    cmd = [str(binary), "-i", str(src_path), "-o", str(dst_path),
           "-n", model, "-s", "4"]
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"realesrgan failed on {src_path.name}:\n{result.stderr}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--sheet", required=True, help="Path to input sheet PNG")
    parser.add_argument("--frames", default="1-6",
                        help="Frame numbers in cell order (top-left to bottom-right).")
    parser.add_argument("--prefix", default="succubus")
    parser.add_argument("--upscale", type=int, default=8,
                        help="Upscale used when the sheet was built (default 8)")
    parser.add_argument(
        "--model", default="realesrgan-x4plus-anime",
        choices=["realesrgan-x4plus", "realesrgan-x4plus-anime",
                 "realesrnet-x4plus", "realesr-animevideov3"],
    )
    parser.add_argument("--no-sharpen", action="store_true",
                        help="Just slice and remove bg, skip Real-ESRGAN.")
    parser.add_argument("--binary", default=None)
    parser.add_argument("--sprites-dir", default=str(DEFAULT_SPRITES))
    parser.add_argument("--in-place", action="store_true",
                        help="Overwrite sprites-dir/<prefix>_NNN.png. "
                             "Default writes to sharpened_from_sheet/.")
    args = parser.parse_args()

    sheet_path = Path(args.sheet)
    if not sheet_path.exists():
        sys.exit(f"sheet not found: {sheet_path}")
    sprites_dir = Path(args.sprites_dir)
    frames = parse_frames(args.frames)
    out_dir = sprites_dir if args.in_place else sprites_dir / "sharpened_from_sheet"
    binary = None if args.no_sharpen else find_binary(args.binary)

    print(f"sheet:  {sheet_path}")
    print(f"frames: {frames}")
    print(f"output: {out_dir}{'  (IN-PLACE)' if args.in_place else ''}")
    print(f"sharpen: {'OFF' if args.no_sharpen else args.model}")
    print()

    print("slicing + bg-removing...")
    cells, _ = slice_sheet(sheet_path, len(frames), args.upscale)

    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        for idx, frame_n in enumerate(frames):
            sliced = cells[idx]
            name = f"{args.prefix}_{frame_n:03d}.png"
            if args.no_sharpen:
                sliced.save(out_dir / name)
                print(f"  {name} ({sliced.size[0]}x{sliced.size[1]})")
                continue
            tmp_path = tmp_dir / name
            sliced.save(tmp_path)
            print(f"sharpening {name} ({sliced.size[0]}x{sliced.size[1]} -> 4x)...", flush=True)
            upscale_image(tmp_path, out_dir / name, binary, args.model)
            with Image.open(out_dir / name) as out_im:
                print(f"  -> {out_dir / name} ({out_im.size[0]}x{out_im.size[1]})")


if __name__ == "__main__":
    main()
