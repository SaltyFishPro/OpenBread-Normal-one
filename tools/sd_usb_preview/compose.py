from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2] / "tmp" / "sd-usb-render"
names = ["01-home", "02-transition", "03-active", "04-inactive"]
for name in names:
    frame = Image.open(root / f"{name}.pgm").convert("RGB")
    frame.save(root / f"{name}.png")
    frame.resize((1152, 504), Image.Resampling.NEAREST).save(root / f"{name}-3x.png")

font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 22)
sheet = Image.new("RGB", (1200, 1140), "#ededed")
draw = ImageDraw.Draw(sheet)
for index, (name, label) in enumerate([
    ("01-home", "主菜单 · SD卡转USB"),
    ("03-active", "读卡页面 · 开启状态"),
]):
    y = 24 + index * 558
    draw.text((24, y), label, font=font, fill="black")
    sheet.paste(Image.open(root / f"{name}-3x.png"), (24, y + 36))
sheet.save(root / "sd-usb-framebuffers.png")
print(root / "sd-usb-framebuffers.png")
