#!/usr/bin/env python3
"""Convert images to ESP32/TJpg_Decoder-friendly JPEGs.

- Output format: baseline JPEG (non-progressive), RGB
- Default max size: 240x280 (fits ST7789 portrait display)
- Saves converted files into an output folder (default: <input>/formatted)
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Iterable

from PIL import Image, ImageOps, UnidentifiedImageError

SUPPORTED_EXTENSIONS = {
    ".jpg",
    ".jpeg",
    ".png",
    ".bmp",
    ".webp",
    ".tif",
    ".tiff",
    ".gif",
}

EXIF_ORIENTATION_TAG = 274


def iter_image_files(input_dir: pathlib.Path) -> Iterable[pathlib.Path]:
    for path in sorted(input_dir.rglob("*")):
        if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS:
            yield path


def unique_output_path(output_dir: pathlib.Path, stem: str) -> pathlib.Path:
    candidate = output_dir / f"{stem}.jpg"
    if not candidate.exists():
        return candidate

    index = 2
    while True:
        candidate = output_dir / f"{stem}_{index}.jpg"
        if not candidate.exists():
            return candidate
        index += 1


def parse_name_pattern(pattern: str) -> tuple[str, int, str]:
    hash_runs = re.findall(r"#+", pattern)
    if len(hash_runs) != 1:
        raise ValueError("must contain exactly one run of '#' (e.g. photo_####.jpg)")

    match = re.match(r"^(.*?)(#+)([^#]*)$", pattern)
    if not match:
        raise ValueError("invalid pattern")

    prefix, hashes, suffix = match.groups()
    return prefix, len(hashes), suffix


def convert_image(
    src_path: pathlib.Path,
    dst_path: pathlib.Path,
    max_width: int,
    max_height: int,
    quality: int,
) -> None:
    with Image.open(src_path) as img:
        # Apply orientation first, then normalize EXIF Orientation to 1.
        img = ImageOps.exif_transpose(img)

        exif_bytes = None
        try:
            exif = img.getexif()
            if exif:
                if EXIF_ORIENTATION_TAG in exif:
                    exif[EXIF_ORIENTATION_TAG] = 1
                exif_bytes = exif.tobytes()
        except Exception:
            exif_bytes = None

        img = img.convert("RGB")
        img.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)

        # Baseline JPEG (progressive=False) works with TJpg_Decoder.
        save_kwargs = {
            "format": "JPEG",
            "quality": quality,
            "optimize": True,
            "progressive": False,
            "subsampling": "4:2:0",
        }
        if exif_bytes:
            save_kwargs["exif"] = exif_bytes
        img.save(dst_path, **save_kwargs)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert images to baseline JPEGs for ESP32 display use."
    )
    parser.add_argument(
        "input_dir",
        type=pathlib.Path,
        help="Folder containing source images.",
    )
    parser.add_argument(
        "--output-dir",
        type=pathlib.Path,
        default=None,
        help="Output folder (default: <input_dir>/formatted).",
    )
    parser.add_argument(
        "--max-width",
        type=int,
        default=240,
        help="Maximum output width (default: 240).",
    )
    parser.add_argument(
        "--max-height",
        type=int,
        default=280,
        help="Maximum output height (default: 280).",
    )
    parser.add_argument(
        "--quality",
        type=int,
        default=88,
        help="JPEG quality 1-95 (default: 88).",
    )
    parser.add_argument(
        "--name-pattern",
        type=str,
        default=None,
        help="Output naming pattern, e.g. 'photo_####.jpg'.",
    )
    parser.add_argument(
        "--start-id",
        type=int,
        default=1,
        help="Starting ID used with --name-pattern (default: 1).",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing output files when using --name-pattern.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    input_dir = args.input_dir.expanduser().resolve()
    if not input_dir.exists() or not input_dir.is_dir():
        print(f"ERROR: Input folder does not exist or is not a directory: {input_dir}")
        return 1

    output_dir = (
        args.output_dir.expanduser().resolve()
        if args.output_dir is not None
        else input_dir / "formatted"
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.max_width <= 0 or args.max_height <= 0:
        print("ERROR: --max-width and --max-height must be > 0")
        return 1

    if not (1 <= args.quality <= 95):
        print("ERROR: --quality must be between 1 and 95")
        return 1

    if args.start_id < 0:
        print("ERROR: --start-id must be >= 0")
        return 1

    pattern_parts: tuple[str, int, str] | None = None
    if args.name_pattern is not None:
        try:
            pattern_parts = parse_name_pattern(args.name_pattern)
        except ValueError as exc:
            print(f"ERROR: --name-pattern {exc}")
            return 1

    sources = list(iter_image_files(input_dir))
    # Don't re-convert files inside output folder if output is inside input.
    sources = [p for p in sources if output_dir not in p.parents]

    if not sources:
        print(f"No supported image files found in: {input_dir}")
        return 0

    converted = 0
    skipped = 0

    for idx, src_path in enumerate(sources):
        if pattern_parts is None:
            dst_path = unique_output_path(output_dir, src_path.stem)
        else:
            prefix, digits, suffix = pattern_parts
            seq = args.start_id + idx
            dst_name = f"{prefix}{seq:0{digits}d}{suffix}"
            dst_path = output_dir / dst_name
            if dst_path.exists() and not args.overwrite:
                print(f"SKIP (already exists, use --overwrite): {dst_path.name}")
                skipped += 1
                continue
        try:
            convert_image(
                src_path=src_path,
                dst_path=dst_path,
                max_width=args.max_width,
                max_height=args.max_height,
                quality=args.quality,
            )
            print(f"OK: {src_path.name} -> {dst_path.name}")
            converted += 1
        except UnidentifiedImageError:
            print(f"SKIP (unrecognized image): {src_path}")
            skipped += 1
        except OSError as exc:
            print(f"SKIP (cannot convert): {src_path} | {exc}")
            skipped += 1

    print("---")
    print(f"Converted: {converted}")
    print(f"Skipped:   {skipped}")
    print(f"Output:    {output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
