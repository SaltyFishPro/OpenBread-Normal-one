"""Extract the retained resident fonts for SD migration regression tests."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/sd-font-migration'
out.mkdir(parents=True, exist_ok=True)
# The reader fixture reopens font 14 from this directory. Keep it bound to the
# current font/common-index pair used by the independent pixel reference.
reader_fonts = root / 'tmp/reader-sd-font/valid/Font'
reader_fonts.mkdir(parents=True, exist_ok=True)
for name in ('font-14.bin', 'font-14.common.bin'):
    shutil.copy2(root / 'tools/font_builder/output' / name, reader_fonts / name)
source = (root / 'lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c').read_text(encoding='utf-8')
names = ['u8g2_font_fur25_tf', 'u8g2_font_7x14_tf', 'u8g2_font_6x12_mf',
         'u8g2_font_logisoso32_tr', 'u8g2_font_logisoso24_tr']
chunks = ['#include "u8g2_fonts.h"\n']
for name in names:
    start = source.index(f'const uint8_t {name}[')
    chunks.append(source[start:source.index(';\n', start) + 1])
(out / 'fonts.c').write_text('\n'.join(chunks), encoding='utf-8')
