"""Independent NBFT layout/pixel checks against production framebuffer captures."""
from pathlib import Path
import re
import sys
from PIL import Image, ImageDraw

project = Path(__file__).resolve().parents[2]
root = project / 'tmp/sd-font-migration'
sys.path.insert(0, str(project / 'tools/font_builder'))
from font_bin import BitmapFont

fonts = {size: BitmapFont.load(project / f'tools/font_builder/output/font-{size}.bin')
         for size in (14, 20, 24)}
source = (project / 'src/ui/pages/AnswersBookData.h').read_text(encoding='utf-8')
answers = re.findall(r'"([^"\n]*)"', source[source.index('kAnswers'):])

def advance(font, value):
    return sum(g.advance if (g := font.glyph(c)) else font.pixel_size for c in value)

def wrap(font, text, width, capacity=79):
    lines, line = [], ''
    for c in text:
        if c in '\r\n':
            if line: lines.append(line)
            line = ''
            continue
        if line and (advance(font, line + c) > width or len((line + c).encode()) > capacity):
            lines.append(line)
            line = ''
        line += c
    if line: lines.append(line)
    return lines

def ink(image, font, text, x, baseline, clip, color=0):
    pixels = image.load()
    for c in text:
        glyph = font.glyph(c)
        assert glyph, f'missing reference glyph {c!r}'
        stride = (glyph.width + 7) // 8
        for y in range(glyph.height):
            for xx in range(glyph.width):
                px, py = x + glyph.x_offset + xx, baseline + glyph.y_offset + y
                if (clip[0] <= px <= clip[2] and clip[1] <= py <= clip[3] and
                        glyph.bitmap[y * stride + xx // 8] & (0x80 >> (xx % 8))):
                    pixels[px, py] = color
        x += glyph.advance

def equal(name, expected, box):
    actual = Image.open(root / f'{name}.pgm')
    assert actual.crop(box).tobytes() == expected.crop(box).tobytes(), f'pixels differ: {name}'

for value in answers:
    lines = wrap(fonts[20], value, 320)
    assert len(lines) <= 2 and ''.join(lines) == value, f'answer does not fit: {value}'
for index in (0, 30, 78):
    expected = Image.new('L', (384, 168), 255)
    for row, value in enumerate(wrap(fonts[20], answers[index], 320)):
        ink(expected, fonts[20], value, 192 - advance(fonts[20], value) // 2,
            94 + row * fonts[20].line_height, (32, 74, 351, 138))
    equal(f'answer-{index}', expected, (32, 74, 352, 139))
for selected in range(3):
    expected = Image.new('L', (384, 168), 255)
    for row, index in enumerate((0, 78, 267)):
        top = 43 + row * 27
        if row == selected: ImageDraw.Draw(expected).rectangle((32, top, 351, top+25), fill=0)
        text = answers[index]
        ink(expected, fonts[14], text, 192 - advance(fonts[14], text)//2, top+18,
            (32, top, 351, top+25), 255 if row == selected else 0)
    equal(f'history-{selected}', expected, (32, 43, 352, 123))

expected = Image.new('L', (384, 168), 255)
ink(expected, fonts[14], '龘 𠮷 音乐 é gyp', 3, 25, (0, 8, 383, 30))
equal('external-text', expected, (0, 0, 384, 168))

notes = '优化龘与𠮷的文字显示\n改善更新日志滚动\n这是较长的中文更新说明，用于检查换行和分页。\nEnglish 123'
lines = wrap(fonts[14], notes, 204, 63)
assert int((root / 'log-count.txt').read_text()) == len(lines)
for i, value in enumerate(lines):
    assert (root / f'log-{i}.txt').read_text(encoding='utf-8') == value, 'log wrap differs'
expected = Image.new('L', (384, 168), 255)
for row, value in enumerate(lines[:2]):
    ink(expected, fonts[14], value, 164, 65+fonts[14].ascent+row*fonts[14].line_height, (164, 65, 367, 118))
equal('update-14', expected, (164, 65, 368, 119))
expected = Image.new('L', (384, 168), 255)
ink(expected, fonts[24], 'bread', 3, 28, (3, 0, 232, 34))
phonetic_x = 3 + advance(fonts[24], 'bread') + 6
ink(expected, fonts[14], '/bred/', phonetic_x, 27, (phonetic_x, 0, 379, 34))
equal('reader-24', expected, (0, 0, 384, 35))

shots = [('answer-30', 'Answer / SD 20'), ('history-0', 'History / SD 14'),
         ('reader-24', 'Reader title / SD 24'), ('update-14', 'Release notes / SD 14'),
         ('update-slide', 'Release notes / scrolling'), ('update-offline', 'Without SD / Latin only, Chinese pending')]
sheet = Image.new('RGB', (384*2+30, (168+24)*3+20), '#ededed')
draw = ImageDraw.Draw(sheet)
for i, (name, label) in enumerate(shots):
    x, y = 10 + (i%2)*394, 8+(i//2)*192
    draw.text((x, y), label, fill='black')
    sheet.paste(Image.open(root / f'{name}.pgm').convert('RGB'), (x, y+18))
sheet.resize((sheet.width*3, sheet.height*3), Image.Resampling.NEAREST).save(root / 'sd-font-stages-3-5.png')
print(f'PASS: {len(answers)} answers fit; answer/history/reader/log pixels and UTF32 wrapping match independent NBFT reference')
