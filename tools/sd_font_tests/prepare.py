from pathlib import Path
import argparse
import shutil
import struct
import sys
import zlib

project = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(project / "tools/font_builder"))
from font_bin import BitmapFont, HEADER, RECORD

parser = argparse.ArgumentParser()
parser.add_argument("--fonts", type=Path, default=project / "tools/font_builder/output")
args = parser.parse_args()
root = project / "tmp/sd-font-test"
valid = root / "valid/Font"
valid.mkdir(parents=True, exist_ok=True)
samples = ("你好，面包！Normal", "0123456789 Aa", "Unicode 𠮷 é")

for size in (14, 20, 24):
    source = args.fonts / f"font-{size}.bin"
    if not source.is_file():
        raise SystemExit(f"Missing {source}; export 14,20,24 using font_builder or pass --fonts")
    font = BitmapFont.load(source)
    assert font.pixel_size == size
    shutil.copyfile(source, valid / source.name)
    # Independent byte-for-byte glyph reference, including supplementary-plane glyphs.
    cps = sorted({ord(c) for line in samples for c in line} | {0x2F9F4})
    with (root / f"expected-{size}.dat").open("wb") as out:
        for cp in cps:
            glyph = font.glyph(cp)
            if glyph is None:
                continue
            out.write(struct.pack("<IHHhhhI", cp, glyph.width, glyph.height,
                                  glyph.x_offset, glyph.y_offset, glyph.advance, len(glyph.bitmap)))
            out.write(glyph.bitmap)
    measure = "你好 A\n012"
    widths = [sum(font.glyph(c).advance if font.glyph(c) else size for c in line)
              for line in measure.splitlines()]
    (root / f"width-{size}.txt").write_text(str(max(widths)), encoding="ascii")
    print(f"Real font: {size}px, {len(font.codepoints)} glyphs, {len(font.data)} bytes")


def fixture(name, data):
    folder = root / name / "Font"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "font-14.bin").write_bytes(data)


source = bytearray((valid / "font-14.bin").read_bytes())
fixture("truncated", source[:32])
bad = source.copy()
bad[0] = 0
fixture("header", bad)
bad = source.copy()
bad[-1] ^= 0x80
fixture("crc", bad)
bad = source.copy()
struct.pack_into("H", bad, 10, 20)
fixture("wrong-size", bad)
(root / "missing/Font").mkdir(parents=True, exist_ok=True)


def synthetic(records, bitmaps, count=None):
    count = len(records) if count is None else count
    payload = b"".join(records) + bitmaps
    bitmap = 64 + count * 24
    return HEADER.pack(b"NBFT", 1, 64, 24, 14, 16, 4, 1, 1, count, 64, bitmap,
                       64 + len(payload), zlib.crc32(payload), bytes(24)) + payload


# Large glyph exercises row streaming, negative offsets and non-byte-aligned padding.
rows = bytes([0xFF] * 16 + [0x80]) * 20
fixture("large", synthetic([RECORD.pack(65, 0, 129, 20, -3, -17, 130, 0, len(rows))], rows))
fixture("bad-glyph", synthetic([RECORD.pack(65, 0xFFFFFFF0, 1, 1, 0, -1, 1, 0, 1)], b"\x80"))
fixture("padding", synthetic([RECORD.pack(65, 0, 1, 1, 0, -1, 1, 0, 1)], b"\x81"))
count = 65537
fixture("big-index", synthetic([RECORD.pack(0x10000 + i, 0, 0, 0, 0, 0, 8, 0, 0)
                                 for i in range(count)], b""))

# Reuse the firmware's actual built-in header/control font for page previews only.
source = (project / "lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c").read_text(encoding="utf-8")
start = source.index("const uint8_t chinese_font_all[")
end = source.index(";\n", start) + 1
(root / "fonts.c").write_text('#include "u8g2_fonts.h"\n' + source[start:end] + "\n", encoding="utf-8")
