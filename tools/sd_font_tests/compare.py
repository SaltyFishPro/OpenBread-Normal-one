from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont

project = Path(__file__).resolve().parents[2]
root = project / "tmp/sd-font-test"
sys.path.insert(0, str(project / "tools/font_builder"))
from font_bin import BitmapFont


def expected_text(image, font, x, baseline, text, color, clip):
    pen_x, pen_y = x, baseline
    pixels = image.load()
    for char in text:
        if char == "\n":
            pen_x, pen_y = x, pen_y + font.line_height
            continue
        glyph = font.glyph(char)
        if glyph is None:
            for yy in range(pen_y - font.pixel_size + 2, pen_y):
                for xx in range(pen_x + 1, pen_x + font.pixel_size - 2):
                    if (xx in (pen_x + 1, pen_x + font.pixel_size - 3) or
                            yy in (pen_y - font.pixel_size + 2, pen_y - 1)):
                        if clip[0] <= xx <= clip[2] and clip[1] <= yy <= clip[3]:
                            pixels[xx, yy] = color
            pen_x += font.pixel_size
            continue
        bits = glyph.pixels()
        for yy in range(glyph.height):
            for xx in range(glyph.width):
                if not bits[yy * glyph.width + xx]:
                    continue
                dx, dy = pen_x + glyph.x_offset + xx, pen_y + glyph.y_offset + yy
                if clip[0] <= dx <= clip[2] and clip[1] <= dy <= clip[3]:
                    pixels[dx, dy] = color
        pen_x += glyph.advance


def compare(name, expected):
    actual = Image.open(root / f"{name}.pgm").convert("L")
    assert actual.size == expected.size and actual.tobytes() == expected.tobytes(), name


for size in (14, 20, 24):
    font = BitmapFont.load(root / "valid/Font" / f"font-{size}.bin")
    for rotation in range(4):
        width, height = (384, 168) if rotation % 2 else (168, 384)
        image = Image.new("L", (width, height), 255)
        for i, text in enumerate(("你好，面包！Normal", "0123456789 Aa", "Unicode 𠮷 é")):
            expected_text(image, font, 16, 84 + i * 26, text, 0, (16, 56, width - 17, 140))
        compare(f"render-{size}-r{rotation}", image)
    image = Image.new("L", (384, 168), 0)
    expected_text(image, font, -8, 13, "Aé你好", 255, (3, 2, 100, 29))
    compare(f"crop-{size}", image)
    image = Image.new("L", (384, 168), 255)
    expected_text(image, font, 4, 60, chr(0x10FFFF), 0, (0, 0, 383, 167))
    compare(f"missing-{size}", image)

for file in root.glob("*.pgm"):
    image = Image.open(file).convert("RGB")
    image.save(file.with_suffix(".png"))

label_font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 22)
sheet = Image.new("RGB", (1200, 1700), "#ededed")
draw = ImageDraw.Draw(sheet)
for i, size in enumerate((14, 20, 24)):
    y = 20 + 558 * i
    draw.text((24, y), f"SD 字库 · {size} px · 从实际 BIN 读取", font=label_font, fill="black")
    image = Image.open(root / f"page-{size}.png")
    sheet.paste(image.resize((1152, 504), Image.Resampling.NEAREST), (24, y + 36))
sheet.save(root / "sd-font-framebuffers.png")
print("PASS: 18 exact pixel comparisons against Python NBFT reference, including all rotations and clipping")
print(root / "sd-font-framebuffers.png")
