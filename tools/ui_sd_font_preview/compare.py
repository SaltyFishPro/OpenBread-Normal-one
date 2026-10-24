"""Independent NBFT reference for cached metrics, pixels and page contact sheets."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/ui-sd-font'
sys.path.insert(0, str(root / 'tools/font_builder'))
from font_bin import BitmapFont

font = BitmapFont.load(root / 'tools/font_builder/output/font-14.bin')
count = 0
for line in (out / 'glyphs.txt').read_text().splitlines():
    parts = line.split()
    cp = int(parts[0])
    glyph = font.glyph(cp)
    if parts[1] == 'missing':
        assert glyph is None, f'cache lost glyph U+{cp:X}'
    else:
        assert glyph is not None, f'cache invented glyph U+{cp:X}'
        assert tuple(map(int, parts[1:6])) == (
            glyph.width, glyph.height, glyph.x_offset, glyph.y_offset, glyph.advance), f'metrics U+{cp:X}'
        assert bytes.fromhex(parts[6] if len(parts)>6 else '') == glyph.bitmap, f'bitmap U+{cp:X}'
    count += 1

def expected(text, x, baseline, size=(384,168), color=0, opaque=False, result=None):
    if result is None:
        result = Image.new('L', size, 255)
    pix = result.load()
    for c in text:
        glyph = font.glyph(c)
        assert glyph
        stride = (glyph.width+7)//8
        for row in range(glyph.height):
            for col in range(glyph.width):
                px,py = x+glyph.x_offset+col,baseline+glyph.y_offset+row
                if 0<=px<size[0] and max(0,baseline-17)<=py<=min(size[1]-1,baseline+5):
                    if glyph.bitmap[row*stride+col//8] & (0x80>>(col%8)):
                        pix[px,py] = color
                    elif opaque:
                        pix[px,py] = 255-color
        x += glyph.advance
    return result

assert Image.open(out/'reference-text.pgm').tobytes() == expected('设置 面包 123',3,28).tobytes()
assert Image.open(out/'opaque-text.pgm').tobytes() == expected('面包 ABC',5,28,color=255,opaque=True).tobytes()
for rotation,size in ((0,(168,384)),(1,(384,168))):
    result = Image.new('L',size,255)
    for x,y in ((-8,6),(size[0]-12,24),(7,size[1]+3)):
        expected('面包 ABC',x,y,size,color=255,opaque=True,result=result)
    assert Image.open(out/f'clip-{rotation}.pgm').tobytes() == result.tobytes(), f'clipping/rotation {rotation}'

# Footer ink stays within its black strip, away from the bottom rule.
reader = Image.open(out/'reader.pgm')
assert all(reader.getpixel((x,y))==255 for x in range(8,120) for y in (165,166)), 'footer below strip'
assert font.glyph('面').advance+font.glyph('包').advance+14*font.glyph('W').advance <= 204
assert font.glyph('面').advance+font.glyph('包').advance+15*font.glyph('W').advance > 204
log_box = (164,70,384,114)
assert Image.open(out/'update-offline.pgm').crop(log_box).tobytes() == Image.open(out/'update-offline-reference.pgm').crop(log_box).tobytes(), 'mixed offline notes layout'
for pgm in out.glob('*.pgm'):
    Image.open(pgm).resize(tuple(v*3 for v in Image.open(pgm).size),Image.Resampling.NEAREST).save(pgm.with_suffix('.png'))

names = [('home-0','主菜单'),('home-9','答案之书入口'),('home-11','电脑看板入口'),
         ('usb','SD 卡交给 USB 后'),('popup','确认弹窗'),('time','时间校准'),
         ('focus','专注时钟'),('bluetooth','蓝牙连接'),('wifi-on','WiFi 状态'),
         ('about','关于设备'),('computer','电脑看板等待'),('wifi-off','WiFi 配网'),
         ('answer-prompt','答案之书提问'),('answer','答案正文 / 20 号'),('answer-history','答案记录 / 14 号'),
         ('update','系统更新'),('update-progress','下载进度'),('reader','阅读 / 14 与 24 号')]
label_font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',15)
sheet = Image.new('RGB',(3*404,6*200+410),'#e9e9e9')
draw = ImageDraw.Draw(sheet)
for i,(name,title) in enumerate(names):
    x,y=10+(i%3)*404,5+(i//3)*200
    draw.text((x,y),title,font=label_font,fill='black')
    sheet.paste(Image.open(out/f'{name}.pgm').convert('RGB'),(x,y+24))
for i,(name,title) in enumerate((('camera','蓝牙拍照'),('pager','翻页器'))):
    x=10+i*200
    draw.text((x,1203),title,font=label_font,fill='black')
    sheet.paste(Image.open(out/f'{name}.pgm').convert('RGB'),(x,1226))
sheet.save(out/'ui-sd-font-pages.png')
print(f'PASS: {count} cached records match independent NBFT glyph metrics/pixels; real framebuffer text matches')
print(out/'ui-sd-font-pages.png')
