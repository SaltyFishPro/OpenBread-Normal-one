"""Prepare real exported fonts and UTF-8 wordbooks; never writes the SD card."""
from pathlib import Path
import argparse
import json
import shutil
import struct
import sys

project = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(project / "tools/font_builder"))
from font_bin import BitmapFont

parser = argparse.ArgumentParser()
parser.add_argument("--fonts", type=Path, default=project / "tools/font_builder/output")
args = parser.parse_args()
root = project / "tmp/reader-sd-font"
root.mkdir(parents=True, exist_ok=True)
font_path = args.fonts / "font-14.bin"
font = BitmapFont.load(font_path)
assert font.pixel_size == 14 and 0 < font.line_height <= 101
records = [
    ["bread", "/bred/", "n. 面包；生计。 v. 在食物上撒面包屑。"],
    ["explore", "/ik'splo:/", "v. 探索未知的世界，发现生活中的美好；仔细研究一个问题，尝试新的方法。 "
     "通过阅读了解历史与文化，通过实践积累经验。 n. 词义翻页测试：这里保留足够长的中文、English 123 和标点，"
     "用来观察换行与分页是否完整。 adj. 充满好奇心的；愿意继续学习的。 adv. 认真地；一步一步地。"],
    ["unicode", "", "n. 扩展区 𠮷，重音 é，缺字 " + chr(0x10FFFF) + "。"],
    ["longword", "", "ABCDEFGHIJKLMNOPQRSTUVWXYZ" * 6],
    ["empty", "", ""],
    ["truncated", "", "界" * 171],  # existing 511-byte field truncates a UTF-8 sequence
]
(root / "records.json").write_text(json.dumps(records, ensure_ascii=False), encoding="utf-8")
wordbook = "# 阅读页 SD 字库试点\n" + "\n".join("\t".join(r) for r in records) + "\n"
data = bytearray(font_path.read_bytes())
for case in ("valid", "plain", "missing", "crc", "wrong-size", "tall"):
    card = root / case
    (card / "words").mkdir(parents=True, exist_ok=True)
    (card / "words/junior.txt").write_text(wordbook, encoding="utf-8")
    (card / "Font").mkdir(exist_ok=True)
    target = card / "Font/font-14.bin"
    target.unlink(missing_ok=True)
    (card / "Font/font-14.common.bin").unlink(missing_ok=True)
    if case == "missing":
        continue
    modified = data.copy()
    if case == "crc":
        modified[-1] ^= 0x80
    if case == "wrong-size":
        struct.pack_into("<H", modified, 10, 20)
    if case == "tall":
        struct.pack_into("<h", modified, 12, 150)
    target.write_bytes(modified)
    if case == "valid" and (args.fonts / "font-14.common.bin").is_file():
        shutil.copyfile(args.fonts / "font-14.common.bin", card / "Font/font-14.common.bin")

source = (project / "lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c").read_text(encoding="utf-8")
declarations = []
for name in ("chinese_font_all", "u8g2_font_fur25_tf"):
    start = source.index(f"const uint8_t {name}[")
    declarations.append(source[start:source.index(";\n", start) + 1])
(root / "fonts.c").write_text('#include "u8g2_fonts.h"\n' + "\n".join(declarations), encoding="utf-8")
print(f"Real 14px font: {len(data)} bytes, line height {font.line_height}; fixtures: {root}")
