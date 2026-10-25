"""Export the fixed-UI subset of an NBFT v1 font as a small NBFT v1 file.

The device loads `/Font/font-14.ui.bin` for fixed menus when it is present;
when the file is absent, corrupt or stale it falls back to the full
`/Font/font-14.bin` path. The subset keeps the NBFT v1 layout so the payload
CRC and record rules stay identical to the full font. The first four bytes of
the 24-byte reserved header area store the codepoint count of
UiFontCodepoints.h so the firmware can detect a stale subset.
"""

from __future__ import annotations

import argparse
import re
import struct
import zlib
from bisect import bisect_left
from pathlib import Path

from font_bin import BitmapFont, HEADER, RECORD


def parse_ui_codepoints(header_path: Path) -> list[int]:
    text = header_path.read_text(encoding="utf-8")
    values = [int(match.group(1), 16) for match in re.finditer(r"0x([0-9A-Fa-f]+)", text)]
    if not values:
        raise ValueError("UiFontCodepoints.h contains no codepoints")
    points = sorted(set(values))
    if points != values:
        raise ValueError("UiFontCodepoints.h kValues is not sorted or has duplicates")
    return points


def build_subset(font_path: Path, codepoints_path: Path) -> tuple[bytes, list[int]]:
    font = BitmapFont.load(font_path)
    points = parse_ui_codepoints(codepoints_path)
    records = bytearray()
    bitmaps = bytearray()
    missing: list[int] = []
    for cp in points:
        index = bisect_left(font.codepoints, cp)
        if index == len(font.codepoints) or font.codepoints[index] != cp:
            missing.append(cp)
            continue
        record = RECORD.unpack_from(font.data, HEADER.size + index * RECORD.size)
        cp0, offset, width, height, x, y, advance, flags, length = record
        bitmap = font.data[font.bitmap_offset + offset:font.bitmap_offset + offset + length]
        records.extend(RECORD.pack(cp0, len(bitmaps), width, height, x, y, advance, flags, length))
        bitmaps.extend(bitmap)
    payload = bytes(records + bitmaps)
    count = len(records) // RECORD.size
    header = HEADER.pack(
        b"NBFT", 1, HEADER.size, RECORD.size, font.pixel_size,
        font.ascent, font.descent, font.line_gap, 1, count,
        HEADER.size, HEADER.size + len(records), HEADER.size + len(payload),
        0, bytes(24))
    header = header[:36] + struct.pack("<I", zlib.crc32(payload)) + header[40:]
    data = header + payload
    subset = BitmapFont(data)  # Full NBFT v1 validation: header, CRC, records, bitmap ranges.
    expected = [cp for cp in points if cp not in missing]
    if subset.codepoints != expected:
        raise ValueError("subset codepoint list mismatch after packing")
    # Mark the expected fixed-UI codepoint count so firmware can reject a stale file.
    reserved = struct.pack("<I", len(points)) + bytes(20)
    data = data[:40] + reserved + payload
    return data, missing


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=14,
                        help="pixel size of the source and output fonts")
    parser.add_argument("--font", type=Path,
                        default=None)
    parser.add_argument("--codepoints", type=Path,
                        default=root / "src/services/UiFontCodepoints.h")
    parser.add_argument("--out", type=Path,
                        default=None)
    parser.add_argument("--check", action="store_true",
                        help="verify the exported subset and SD package match current UI codepoints")
    args = parser.parse_args()
    font_path = args.font or root / f"tools/font_builder/output/font-{args.size}.bin"
    out_path = args.out or root / f"tools/font_builder/output/font-{args.size}.ui.bin"
    data, missing = build_subset(font_path, args.codepoints)
    if args.check:
        packaged = out_path.parent / "Font" / out_path.name
        if not out_path.is_file() or out_path.read_bytes() != data:
            raise SystemExit(f"stale UI subset: {out_path}; regenerate before copying to SD")
        if packaged.is_file() and packaged.read_bytes() != data:
            raise SystemExit(f"stale SD package: {packaged}; copy the regenerated subset")
        print(f"checked {out_path}: {len(data)} bytes")
    else:
        out_path.write_bytes(data)
        print(f"wrote {out_path}: {len(data)} bytes")
    if missing:
        print("missing in source font (box placeholder): " +
              ", ".join(f"U+{cp:04X}" for cp in missing))


if __name__ == "__main__":
    main()
