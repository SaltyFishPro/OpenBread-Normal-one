"""Independent NBFT pixel/layout oracle for the actual ReaderPage framebuffer."""
from pathlib import Path
import re
import sys
from PIL import Image, ImageDraw, ImageFont

project = Path(__file__).resolve().parents[2]
root = project / "tmp/reader-sd-font"
sys.path.insert(0, str(project / "tools/font_builder"))
from font_bin import BitmapFont

font = BitmapFont.load(root / "valid/Font/font-14.bin")
pos = re.compile(r"(?:abbr|interj|prep|pron|conj|adj|adv|num|aux|det|art|vt|vi|int|n|v|a)\.(?=\s|$)")
line_count = min(8, 101 // font.line_height)


def strict_scalars(data):
    """Malformed bytes consume one byte each, including the existing field truncation."""
    chars = []
    i = 0
    while i < len(data):
        length = next((n for n in range(1, 5)
                       if _one_scalar(data[i:i + n])), None)
        if length:
            chars.append(data[i:i + length].decode("utf-8"))
            i += length
        else:
            chars.append("\ufffd")
            i += 1
    return "".join(chars)


def _one_scalar(data):
    try:
        return len(data.decode("utf-8")) == 1
    except UnicodeDecodeError:
        return False


def wrap(text):
    # Compute all lines independently; production keeps only visible byte spans.
    lines, current, width = [], "", 0
    for i, char in enumerate(text):
        if char == "\n" and i and text[i - 1] == "\r":
            continue
        if char in "\r\n":
            lines.append(current)
            current, width = "", 0
            continue
        glyph = font.glyph(char)
        advance = glyph.advance if glyph else font.pixel_size
        if current and (pos.match(text, i) or width + advance > 348):
            lines.append(current)
            current, width = "", 0
        current += char
        width += advance
    if current:
        lines.append(current)
    return [lines[i:i + line_count] for i in range(0, len(lines), line_count)] or [[]]


def pixels(lines):
    image = Image.new("L", (384, 168), 255)
    out = image.load()
    for i, line in enumerate(lines):
        x, baseline = 3, 42 + font.ascent + i * font.line_height
        for char in line:
            glyph = font.glyph(char)
            if glyph is None:
                left, right = x + 1, x + font.pixel_size - 3
                top, bottom = baseline - font.pixel_size + 2, baseline - 1
                coords = [(xx, yy) for yy in range(top, bottom + 1)
                          for xx in range(left, right + 1)
                          if xx in (left, right) or yy in (top, bottom)]
                advance = font.pixel_size
            else:
                ink = glyph.pixels()
                coords = [(x + glyph.x_offset + xx, baseline + glyph.y_offset + yy)
                          for yy in range(glyph.height) for xx in range(glyph.width)
                          if ink[yy * glyph.width + xx]]
                advance = glyph.advance
            for xx, yy in coords:
                if 3 <= xx <= 350 and 42 <= yy <= 142:
                    out[xx, yy] = 0
            x += advance
    return image


comparisons = 0
for word, path in enumerate(sorted(root.glob("meaning-*.bin"))):
    text = strict_scalars(path.read_bytes())
    pages = wrap(text)
    for card in ("valid", "plain"):
        prefix = f"{card}-word-{word}"
        assert int((root / f"{prefix}-pages.txt").read_text()) == len(pages), prefix
        for p, lines in enumerate(pages):
            actual = Image.open(root / f"{prefix}-page-{p}.pgm").convert("L")
            assert actual.getpixel((368, 70)) == (0 if p else 255), (prefix, p, "up arrow")
            assert actual.getpixel((368, 125)) == (0 if p + 1 < len(pages) else 255), (prefix, p, "down arrow")
            if text:  # empty meaning uses a built-in status message
                expected = pixels(lines)
                assert actual.crop((3, 42, 351, 143)).tobytes() == expected.crop((3, 42, 351, 143)).tobytes(), (prefix, p)
                comparisons += 1
            # All pixels outside the body and arrows stay identical across pages.
            reference = Image.open(root / f"{prefix}-page-0.pgm")
            for box in ((0, 0, 384, 42), (0, 143, 384, 168), (351, 42, 363, 143)):
                assert actual.crop(box).tobytes() == reference.crop(box).tobytes(), (prefix, p, box)
        first = Image.open(root / f"{prefix}-page-0.pgm")
        back = Image.open(root / f"{prefix}-back-to-first.pgm")
        assert first.tobytes() == back.tobytes(), prefix
    for p in range(len(pages)):
        assert (Image.open(root / f"valid-word-{word}-page-{p}.pgm").tobytes() ==
                Image.open(root / f"plain-word-{word}-page-{p}.pgm").tobytes())

# No partially drawn definition survives a mid-render read error.
assert Image.open(root / "read-error.pgm").tobytes() == Image.open(root / "read-error-clean.pgm").tobytes()
for case in ("missing", "crc", "wrong-size", "tall"):
    assert Image.open(root / f"recovered-{case}.pgm").tobytes() == Image.open(root / "valid-word-0-page-0.pgm").tobytes()
assert Image.open(root / "read-error-recovered.pgm").tobytes() == Image.open(root / "valid-word-1-page-0.pgm").tobytes()
assert Image.open(root / "layout-switch.pgm").tobytes() == Image.open(root / "valid-word-2-page-0.pgm").tobytes()

for path in root.glob("*.pgm"):
    im = Image.open(path).convert("RGB")
    im.save(path.with_suffix(".png"))

labels = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 19)
sheet = Image.new("RGB", (1200, 640), "#ededed")
draw = ImageDraw.Draw(sheet)
samples = [("valid-word-0-page-0", "短释义 · SD 14 号字体"),
           ("valid-word-1-page-0", "长释义 · 第 1 页"),
           ("valid-word-1-page-1", "长释义 · 第 2 页"),
           ("valid-word-2-page-0", "扩展区、重音与缺字占位")]
for i, (name, label) in enumerate(samples):
    x, y = 16 + (i % 2) * 596, 12 + (i // 2) * 316
    draw.text((x, y), label, font=labels, fill="black")
    im = Image.open(root / f"{name}.png")
    sheet.paste(im.resize((576, 252), Image.Resampling.NEAREST), (x, y + 32))
sheet.save(root / "reader-sd-font-framebuffers.png")
print(f"PASS: {comparisons} meaning body pixel comparisons, independent pagination, chrome bounds, page round trips, sidecar equivalence and error recovery")
print(root / "reader-sd-font-framebuffers.png")
