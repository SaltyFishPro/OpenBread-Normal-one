"""Check production pixels against independently decoded SD glyphs and SVG assets."""
from pathlib import Path
import re
import sys
from PIL import Image, ImageChops

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/font_builder'))
from font_bin import BitmapFont

out = root / 'tmp/admin-framebuffer'
fonts = {size: BitmapFont((root / f'tools/font_builder/output/font-{size}.bin').read_bytes())
         for size in (14, 20, 24)}

def check_text(state, size, text, x, baseline, box):
    expected = Image.new('L', (384, 168))
    for character in text:
        glyph = fonts[size].glyph(character)
        assert glyph is not None, f'missing {character}'
        if glyph.width and glyph.height:
            mask = Image.frombytes('L', (glyph.width, glyph.height), glyph.pixels())
            expected.paste(255, (x + glyph.x_offset, baseline + glyph.y_offset), mask)
        x += glyph.advance
    actual = Image.open(out / f'firmware-{state}.pgm')
    assert ImageChops.difference(expected.crop(box), actual.crop(box)).getbbox() is None, (state, text)

check_text('entry', 24, '设备管理', 112, 81, (112, 50, 368, 91))
check_text('starting', 24, '正在开启热点', 112, 81, (112, 50, 368, 91))
check_text('failed', 24, '热点开启失败', 112, 81, (112, 50, 368, 91))
check_text('closed', 24, '管理已关闭', 112, 81, (112, 50, 368, 91))
check_text('ready', 20, '扫码连接设备热点', 16, 57, (16, 34, 253, 62))
check_text('ready', 14, 'OpenBread-Manage-1A2B', 16, 82, (16, 65, 255, 87))
check_text('ready', 14, 'http://192.168.4.1', 16, 102, (16, 87, 255, 107))
check_text('ready', 14, '验证码 482 916', 16, 122, (16, 107, 255, 129))
for state, icon in (('entry', 'deviceIcon'), ('starting', 'loadingIcon'),
                    ('failed', 'warningIcon'), ('closed', 'doneIcon')):
    svg = Image.open(out / f'{icon}.png').convert('L').point(lambda value: 255 if value >= 128 else 0)
    frame = Image.open(out / f'firmware-{state}.pgm')
    assert ImageChops.difference(svg, frame.crop((16, 48, 92, 124))).getbbox() is None, icon

# Quiet zone and all modules must match the approved SVG at the new 100px size.
source = (root / 'tools/home_menu_preview/admin_hotspot_qr.svg').read_text(encoding='utf-8')
modules = {(int(x), int(y)) for x, y in re.findall(r'M(\d+),(\d+)H', source)}
frame = Image.open(out / 'firmware-ready.pgm')
for y in range(37):
    for x in range(37):
        # Check an interior sample of every module, including the quiet zone.
        px = 268 + 4 + (x * 92 // 37 + (x + 1) * 92 // 37 - 1) // 2
        py = 32 + 4 + (y * 92 // 37 + (y + 1) * 92 // 37 - 1) // 2
        assert frame.getpixel((px, py)) == (0 if (x, y) in modules else 255), (x, y)
print('PASS: exact SD text pixels at 14/20/24px, 4 approved icon silhouettes, QR modules and quiet zone')
