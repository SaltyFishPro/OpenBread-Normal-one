"""Place reference screenshots next to pixels from the production framebuffer."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--city-reference", type=Path, required=True)
    parser.add_argument("--tunnel-reference", type=Path, required=True)
    args = parser.parse_args()
    out_dir = Path("tmp/racing-render")
    rows = [
        ("City", args.city_reference, out_dir / "01-city.pgm"),
        ("Tunnel", args.tunnel_reference, out_dir / "02-tunnel.pgm"),
    ]
    canvas = Image.new("RGB", (1024, 850), "#dddddd")
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    for row_index, (name, reference_path, capture_path) in enumerate(rows):
        reference = Image.open(reference_path).convert("RGB")
        capture = Image.open(capture_path).convert("RGB")
        capture.save(out_dir / f"{capture_path.stem}.png")
        reference = reference.resize((480, 360), Image.Resampling.NEAREST)
        capture = capture.resize((480, 210), Image.Resampling.NEAREST)
        top = 32 + row_index * 420
        draw.text((12, top - 22), f"{name}: reference 320x240 RGB", fill="black", font=font)
        draw.text((524, top - 22), "OpenBread: ST7305 framebuffer 384x168 BW", fill="black", font=font)
        canvas.paste(reference, (12, top))
        canvas.paste(capture, (524, top))
    output = out_dir / "racing-framebuffer-comparison.png"
    canvas.save(output)
    print(output.resolve())


if __name__ == "__main__":
    main()
