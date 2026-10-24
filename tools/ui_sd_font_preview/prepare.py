from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/font_builder'))
from font_ui_subset import build_subset

out = root / 'tmp/ui-sd-font'
out.mkdir(parents=True, exist_ok=True)
source = root / 'tools/font_builder/output/font-14.bin'
for case in ('valid', 'crc', 'missing', 'validfull'):
    card = out / case / 'Font'
    card.mkdir(parents=True, exist_ok=True)
    if case == 'missing':
        continue
    data = bytearray(source.read_bytes())
    if case == 'crc': data[-1] ^= 0x80
    (card / source.name).write_bytes(data)
for size in (20,24):
    shutil.copyfile(root / f'tools/font_builder/output/font-{size}.bin', out/f'valid/Font/font-{size}.bin')
common = root / 'tools/font_builder/output/font-14.common.bin'
if common.is_file():
    shutil.copyfile(common, out/'valid/Font/font-14.common.bin')
subset_data, subset_missing = build_subset(source, root / 'src/services/UiFontCodepoints.h')
(out/'valid/Font/font-14.ui.bin').write_bytes(subset_data)
bad_subset = bytearray(subset_data)
bad_subset[-1] ^= 0x80
(out/'crc/Font/font-14.ui.bin').write_bytes(bytes(bad_subset))
# validfull mirrors valid but without the ui subset, for the fallback path test.
for name in ('font-14.bin', 'font-20.bin', 'font-24.bin', 'font-14.common.bin'):
    shutil.copyfile(out/f'valid/Font/{name}', out/f'validfull/Font/{name}')
(out/'valid/words').mkdir(exist_ok=True)
(out/'valid/words/junior.txt').write_text('bread\t/bred/\tn. 面包；生计。\n',encoding='utf-8')
text = (root / 'lib/ST7305_MonoTFT_Library/src/u8g2_fonts.c').read_text(encoding='utf-8')
names = ('u8g2_font_7x14_tf', 'u8g2_font_7x14B_tf', 'u8g2_font_6x12_mf',
         'u8g2_font_logisoso32_tr', 'u8g2_font_logisoso24_tr', 'u8g2_font_fur25_tf')
declarations = ['#include "u8g2_fonts.h"\n']
for name in names:
    start = text.index(f'const uint8_t {name}[')
    declarations.append(text[start:text.index(';\n', start)+1])
(out / 'fonts.c').write_text('\n'.join(declarations), encoding='utf-8')
