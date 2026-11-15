"""Assemble native browser crops and actual production framebuffer exports."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/admin-framebuffer'
states = ('entry', 'starting', 'ready', 'failed', 'closed', 'no-sd')
scale = 2
sheet = Image.new('RGB', (1200, 62 + len(states) * 202), '#25302b')
draw = ImageDraw.Draw(sheet)
for column, title in enumerate(('HTML reference', 'Before: production buffer', 'After: production buffer')):
    draw.text((column * 400 + 14, 14), title, fill='white')
for row, state in enumerate(states):
    top = 48 + row * 202
    draw.text((14, top), state, fill='white')
    for column, prefix in enumerate(('html', 'before', 'firmware')):
        path = out / f'{prefix}-{state}.png'
        if prefix == 'firmware':
            im = Image.open(out / f'{prefix}-{state}.pgm').convert('RGB')
            im.save(path)
        elif path.exists():
            im = Image.open(path).convert('RGB')
        else:
            continue
        sheet.paste(im, (column * 400 + 8, top + 20))
sheet = sheet.crop((0, 0, 1200, 48 + len(states) * 202))
sheet.save(out / 'comparison.png')
# Larger side by side images make individual pixels inspectable.
for state in states:
    pair = Image.new('RGB', (384 * 2, 168))
    for col, prefix in enumerate(('html', 'firmware')):
        pair.paste(Image.open(out / f'{prefix}-{state}.png'), (col * 384, 0))
    pair.resize((1536, 336), Image.Resampling.NEAREST).save(out / f'compare-{state}.png')
print(out / 'comparison.png')
