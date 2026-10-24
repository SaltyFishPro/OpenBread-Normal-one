"""Extract the retained resident fonts for SD migration regression tests."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/sd-font-migration'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c').read_text(encoding='utf-8')
names = ['u8g2_font_fur25_tf', 'u8g2_font_7x14_tf', 'u8g2_font_6x12_mf',
         'u8g2_font_logisoso32_tr', 'u8g2_font_logisoso24_tr']
chunks = ['#include "u8g2_fonts.h"\n']
for name in names:
    start = source.index(f'const uint8_t {name}[')
    chunks.append(source[start:source.index(';\n', start) + 1])
(out / 'fonts.c').write_text('\n'.join(chunks), encoding='utf-8')
