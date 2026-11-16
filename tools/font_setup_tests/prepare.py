from pathlib import Path
root = Path(__file__).resolve().parents[2]
source = (root / 'lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c').read_text(encoding='utf-8')
declarations = ['#include "u8g2_fonts.h"']
for name in ('u8g2_font_7x14_tf', 'u8g2_font_10x20_tf', 'u8g2_font_6x12_mf'):
    start = source.index('const uint8_t ' + name + '[')
    declarations.append(source[start:source.index(';\n', start)+1])
(root / 'tmp/boot-fonts.c').write_text('\n'.join(declarations), encoding='utf-8')
