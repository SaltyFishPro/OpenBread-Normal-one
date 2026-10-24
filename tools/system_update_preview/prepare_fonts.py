from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / 'lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c').read_text(encoding='utf-8')
fonts = ['u8g2_font_7x14_tf', 'u8g2_font_7x14B_tf', 'u8g2_font_logisoso32_tr',
         'u8g2_font_logisoso24_tr']
chunks = ['#include "u8g2_fonts.h"\n']
for name in fonts:
    start = source.index(f'const uint8_t {name}[')
    end = source.index(';\n', start) + 1
    chunks.append(source[start:end] + '\n')
output = root / 'tmp/system-update-render/fonts.c'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(''.join(chunks), encoding='utf-8')
print(output)
