from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2] / 'tmp/system-update-render'
for file in root.glob('*.pgm'):
    frame = Image.open(file).convert('RGB')
    frame.save(file.with_suffix('.png'))
    frame.resize((1152, 504), Image.Resampling.NEAREST).save(root / (file.stem + '-3x.png'))
names = [
    ('02-idle', '未检查 · 本版日志'), ('06-available', '发现更新 · 远端日志'),
    ('07-download-68', '下载 · 真实进度布局'), ('09-ready', '校验通过 · 等待重启'),
    ('10-confirm-cancel', '统一确认弹窗 · 默认取消'), ('13-success', '重启后版本核验成功'),
]
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 22)
sheet = Image.new('RGB', (1584, 1212), '#ededed')
draw = ImageDraw.Draw(sheet)
for i, (name, label) in enumerate(names):
    x, y = 16 + (i % 2) * 792, 16 + (i // 2) * 404
    draw.text((x, y), label, font=font, fill='black')
    frame = Image.open(root / (name + '.png')).resize((768, 336), Image.Resampling.NEAREST)
    sheet.paste(frame, (x, y + 38))
sheet.save(root / 'system-update-framebuffers.png')
print(root / 'system-update-framebuffers.png')
