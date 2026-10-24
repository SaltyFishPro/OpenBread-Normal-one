from pathlib import Path
import argparse
import shutil
import struct
import sys
import zlib

project = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(project / "tools/font_builder"))
from font_bin import BitmapFont, HEADER, RECORD
from font_common_index import CommonGlyphIndex, encode_common_index

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
    (valid / f"font-{size}.common.bin").unlink(missing_ok=True)  # Baseline must use no sidecar.
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
    # A bounded 7,500-entry fixture includes sample/extended glyphs and leaves Z
    # outside the common set to verify complete-font fallback.
    preferred = {cp for cp in cps if cp in font.codepoints} - {ord("Z")}
    common_points = set(preferred)
    for cp in font.codepoints:
        if len(common_points) >= min(7500, len(font.codepoints) - 1):
            break
        if cp != ord("Z"):
            common_points.add(cp)
    common_dir = root / "common-valid/Font"
    common_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, common_dir / source.name)
    (common_dir / f"font-{size}.common.bin").write_bytes(encode_common_index(font, sorted(common_points)))
    # Also validate the user's actual current export when sidecars were generated.
    sidecar = args.fonts / f"font-{size}.common.bin"
    (root / f"expected-common-{size}.dat").unlink(missing_ok=True)
    if sidecar.is_file():
        decoded_common = CommonGlyphIndex.load(sidecar, font)
        exported_dir = root / "common-exported/Font"
        exported_dir.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, exported_dir / source.name)
        shutil.copyfile(sidecar, exported_dir / sidecar.name)
        with (root / f"expected-common-{size}.dat").open("wb") as out:
            for cp in decoded_common.codepoints:
                glyph = font.glyph(cp)
                out.write(struct.pack("<IHHhhhI", cp, glyph.width, glyph.height,
                                      glyph.x_offset, glyph.y_offset, glyph.advance, len(glyph.bitmap)))
                out.write(glyph.bitmap)
        print(f"Exported common index: {size}px, {len(decoded_common.codepoints)} entries")


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
bad = source.copy()
struct.pack_into("<h", bad, 16, struct.unpack_from("<h", source, 16)[0] + 1)
fixture("changed-header", bad)
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


def common_fixture(name, data):
    folder = root / name / "Font"
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(valid / "font-14.bin", folder / "font-14.bin")
    (folder / "font-14.common.bin").write_bytes(data)


common = (root / "common-valid/Font/font-14.common.bin").read_bytes()
for name, change in (
        ("magic", lambda b: b.__setitem__(0, 0)),
        ("version", lambda b: struct.pack_into("<H", b, 4, 2)),
        ("header-size", lambda b: struct.pack_into("<H", b, 6, 64)),
        ("record-size", lambda b: struct.pack_into("<H", b, 8, 4)),
        ("size", lambda b: struct.pack_into("<H", b, 10, 20)),
        ("zero", lambda b: struct.pack_into("<I", b, 12, 0)),
        ("oversize", lambda b: struct.pack_into("<I", b, 12, 0xFFFFFFFF)),
        ("length", lambda b: struct.pack_into("<I", b, 16, len(b) + 8)),
        ("reserved", lambda b: b.__setitem__(24, 1)),
        ("binding", lambda b: b.__setitem__(32 + 36, b[32 + 36] ^ 1)),
        ("crc", lambda b: struct.pack_into("<I", b, 20, 0)),
        ("offset", lambda b: struct.pack_into("<I", b, 100, 65)),
        ("before-index", lambda b: struct.pack_into("<I", b, 100, 40)),
        ("after-index", lambda b: struct.pack_into("<I", b, 100, 0xFFFFFFF8)),
        ("duplicate", lambda b: b.__setitem__(slice(104, 112), b[96:104])),
        ("scalar", lambda b: struct.pack_into("<I", b, 96, 0xD800)),
        ("unordered-offset", lambda b: b.__setitem__(slice(108, 112), b[100:104])),
        ("block-boundary", lambda b: b.__setitem__(slice(96 + 128 * 8, 104 + 128 * 8),
                                                  b[96 + 127 * 8:104 + 127 * 8]))):
    data = bytearray(common)
    change(data)
    if name in ("offset", "before-index", "after-index", "duplicate", "scalar",
                "unordered-offset", "block-boundary"):
        struct.pack_into("<I", data, 20, zlib.crc32(data[96:]))
    common_fixture("common-bad-" + name, data)
common_fixture("common-bad-truncated", common[:-1])
common_fixture("common-bad-short", common[:32])
font = BitmapFont(bytes(source))
too_many = encode_common_index(font, font.codepoints[:7501])
common_fixture("common-bad-limit", too_many)
# Valid scalar/order/CRC but a wrong target. Device checks target codepoints on
# use instead of reading every NBFT record again during index loading.
wrong_target = bytearray(encode_common_index(font, [ord("A")]))
a_offset = struct.unpack_from("<I", wrong_target, 100)[0]
struct.pack_into("<I", wrong_target, 100, a_offset + 24)
struct.pack_into("<I", wrong_target, 20, zlib.crc32(wrong_target[96:]))
common_fixture("common-bad-target", wrong_target)
# A payload exactly on the per-tick byte boundary must not read an NBCI
# header on the same tick after completing NBFT verification.
aligned = synthetic([
    RECORD.pack(65, 0, 128, 1022, 0, -17, 128, 0, 16352),
    RECORD.pack(66, 16352, 128, 1023, 0, -17, 128, 0, 16368)], bytes(32720))
assert len(aligned) - 64 == 32768
fixture("common-aligned", aligned)
(root / "common-aligned/Font/font-14.common.bin").write_bytes(
    encode_common_index(BitmapFont(aligned), [65, 66]))

# Reuse the firmware's actual built-in header/control font for page previews only.
source = (project / "lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c").read_text(encoding="utf-8")
start = source.index("const uint8_t u8g2_font_7x14_tf[")
end = source.index(";\n", start) + 1
(root / "fonts.c").write_text('#include "u8g2_fonts.h"\n' + source[start:end] + "\n", encoding="utf-8")
