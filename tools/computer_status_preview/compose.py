from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2] / "tmp" / "computer-status-render"
names = [
    ("01-transition", "进入转场 · 半屏"),
    ("02-intro-start", "USB 插图 · 动画开始"),
    ("03-intro-middle", "USB 插图 · 动画中段"),
    ("04-waiting", "等待连接 · 动画已停止"),
    ("05-connected", "Studio 已连接 · 等待首帧"),
    ("06-reenter-transition", "再次进入 · 转场重置"),
]
for name, _ in names:
    frame = Image.open(root / f"{name}.pgm").convert("RGB")
    frame.save(root / f"{name}.png")
    frame.resize((1152, 504), Image.Resampling.NEAREST).save(root / f"{name}-3x.png")

font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 22)
sheet = Image.new("RGB", (1200, 1140), "#ededed")
draw = ImageDraw.Draw(sheet)
for index, (name, label) in enumerate(names[3:5]):
    y = 24 + index * 558
    draw.text((24, y), label, font=font, fill="black")
    frame = Image.open(root / f"{name}-3x.png")
    sheet.paste(frame, (24, y + 36))
sheet.save(root / "computer-status-waiting-framebuffers.png")

frames = [Image.open(root / f"{name}.pgm").convert("RGB").resize(
    (768, 336), Image.Resampling.NEAREST) for name in ("02-intro-start", "03-intro-middle", "04-waiting")]
frames[0].save(root / "intro-sample.gif", save_all=True, append_images=frames[1:],
               duration=[500, 500, 1500], loop=0)
print(root / "computer-status-waiting-framebuffers.png")
