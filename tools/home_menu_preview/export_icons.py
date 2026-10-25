"""Embed the firmware's main-menu animation frames in the standalone preview."""

import base64
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
PREVIEW = Path(__file__).with_name("main_menu_mock.html")
START = '<script id="firmware-icons" type="application/json">'
END = '</script><!-- firmware-icons -->'


def number(source, name):
    match = re.search(rf"(?:#define\s+{name}\s+|\b{name}\s*=\s*)(\d+)", source)
    if not match:
        raise ValueError(f"Missing numeric constant: {name}")
    return int(match[1])


def read_frames(header, symbol, prefix):
    source = header.read_text(encoding="utf-8")
    width = number(source, prefix + "_FRAME_WIDTH")
    height = number(source, prefix + "_FRAME_HEIGHT")
    delay = number(source, prefix + "_FRAME_DELAY")
    definition = source
    if not re.search(rf"\b{symbol}\s*\[[^;]*?=\s*\{{", definition):
        definition = header.with_suffix(".cpp").read_text(encoding="utf-8")
    match = re.search(rf"\b{symbol}\s*\[[^;]*?=\s*\{{(.*?)\}};", definition, re.S)
    if not match:
        raise ValueError(f"Missing array: {symbol}")
    values = re.sub(r"//[^\n]*|/\*.*?\*/", "", match[1], flags=re.S)
    blob = bytes(int(n, 0) for n in re.findall(r"0[xX][0-9a-fA-F]+|\d+", values))
    frame_bytes = ((width + 7) // 8) * height
    if re.search(rf"#define\s+{prefix}_FRAME_BYTES\s+0x8000U", source):
        count = number(source, prefix + "_FRAME_COUNT")
        table_bytes = (count + 1) * 2
        offsets = [int.from_bytes(blob[i:i + 2], "little") for i in range(0, table_bytes, 2)]
        if offsets[0] != 0 or table_bytes + offsets[-1] != len(blob):
            raise ValueError(f"Invalid packed offsets: {symbol}")
        frames = []
        for start, end in zip(offsets, offsets[1:]):
            encoded = blob[table_bytes + start:table_bytes + end]
            if end <= start or len(encoded) % 2:
                raise ValueError(f"Invalid RLE frame: {symbol}")
            decoded = bytearray()
            for length, value in zip(encoded[::2], encoded[1::2]):
                if not length:
                    raise ValueError(f"Empty RLE run: {symbol}")
                decoded.extend([value] * length)
            if len(decoded) != frame_bytes:
                raise ValueError(f"Invalid decoded size: {symbol}")
            frames.append(bytes(decoded))
    else:
        if len(blob) % frame_bytes:
            raise ValueError(f"Invalid raw frame size: {symbol}")
        frames = [blob[i:i + frame_bytes] for i in range(0, len(blob), frame_bytes)]
    if not frames or not delay:
        raise ValueError(f"Empty animation: {symbol}")
    return width, height, delay, frames


def scale_frame(frame, width, height, size):
    """Match IconBitmap::drawFrame's floor(dx * srcW / dstW) sampling."""
    src_stride = (width + 7) // 8
    dst_stride = (size + 7) // 8
    output = bytearray(dst_stride * size)
    for y in range(size):
        sy = y * height // size
        for x in range(size):
            sx = x * width // size
            if frame[sy * src_stride + sx // 8] & (0x80 >> (sx % 8)):
                output[y * dst_stride + x // 8] |= 0x80 >> (x % 8)
    return base64.b64encode(output).decode("ascii")


def main():
    home = ROOT / "src/ui/pages/HomePage.cpp"
    source = home.read_text(encoding="utf-8")
    headers = [(home.parent / path).resolve() for path in re.findall(r'#include "([^\"]+)"', source)]
    block = re.search(r"kMenuIcons\[\]\s*=\s*\{(.*?)\n\};", source, re.S).group(1)
    mappings = re.findall(r"&([a-z0-9_]+)\[0\]\[0\]\).*?,\s*([A-Z0-9_]+)_FRAME_BYTES", block)
    labels = re.findall(r'"([^\"]+)"', re.search(r"kMenuNamesZh\[\]\s*=\s*\{(.*?)\};", source, re.S).group(1))
    size = number((ROOT / "src/ui/ThemeMono.h").read_text(encoding="utf-8"), "kIconFocusSize")
    if len(mappings) != len(labels):
        raise ValueError("Menu labels and icon mappings differ")
    icons = []
    for label, (symbol, prefix) in zip(labels, mappings):
        candidates = [h for h in headers if re.search(rf"\b{symbol}\s*\[", h.read_text(encoding="utf-8"))]
        if len(candidates) != 1:
            raise ValueError(f"Ambiguous resource: {symbol}")
        header = candidates[0]
        width, height, delay, frames = read_frames(header, symbol, prefix)
        scaled = [scale_frame(f, width, height, size) for f in frames]
        poster = next((i for i, frame in enumerate(scaled) if any(base64.b64decode(frame))), 0)
        icons.append(dict(label=label, source=header.relative_to(ROOT).as_posix(),
                          width=size, height=size, delay=delay,
                          posterFrame=poster, frames=scaled))
    markup = START + "\n" + json.dumps(icons, ensure_ascii=False, separators=(",", ":")) + "\n" + END
    html = PREVIEW.read_text(encoding="utf-8")
    if START in html:
        html = re.sub(re.escape(START) + r".*?" + re.escape(END), lambda _: markup, html, flags=re.S)
    else:
        html = html.replace("<script>\n", markup + "\n<script>\n", 1)
    PREVIEW.write_text(html, encoding="utf-8", newline="\n")
    print(f"Embedded {len(icons)} icons / {sum(len(i['frames']) for i in icons)} frames at {size}px.")


if __name__ == "__main__":
    main()
