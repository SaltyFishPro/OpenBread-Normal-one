from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


root = Path(__file__).resolve().parents[2] / "tmp" / "answers-book-render"
names = [
    ("01-prompt", "提示页"),
    ("02-thinking", "思考中 · 约剩 3 秒"),
    ("03-reveal", "翻页提示"),
    ("04-answer", "答案页"),
    ("05-history-first", "记录页 · 首项选中"),
    ("06-history-third", "记录页 · 第三项选中"),
]
scale = 2
tile_w, tile_h = 384 * scale, 168 * scale
gap, label_h = 24, 34
sheet = Image.new("RGB", (tile_w * 2 + gap * 3,
                          (tile_h + label_h) * 3 + gap * 4), "#e8e8e8")
draw = ImageDraw.Draw(sheet)
font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 18)

for index, (name, label) in enumerate(names):
    frame = Image.open(root / f"{name}.pgm").convert("RGB")
    frame.resize((tile_w, tile_h), Image.Resampling.NEAREST).save(root / f"{name}.png")
    row, col = divmod(index, 2)
    x = gap + col * (tile_w + gap)
    y = gap + row * (tile_h + label_h + gap)
    draw.text((x, y), label, font=font, fill="black")
    sheet.paste(frame.resize((tile_w, tile_h), Image.Resampling.NEAREST),
                (x, y + label_h))

sheet.save(root / "answers-book-real-framebuffers.png")
print(root / "answers-book-real-framebuffers.png")
