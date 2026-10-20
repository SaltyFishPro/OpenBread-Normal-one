"""Offline TTF/OTF/TTC -> NBFT v1 monochrome bitmap fonts and decoded previews."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct
import tempfile
from typing import Callable
import unicodedata
import zlib

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

from font_bin import BitmapFont, FileBitmapFont, Glyph, HEADER, MAGIC, RECORD, ROW_MSB, VERSION


DEFAULT_TEXT = "电脑状态看板 12:30\n系统更新  阅读  音乐\n今天也会有好事发生。\nCPU 28%  RAM 46%  26℃"
EXAMPLE_CHARSET = Path(__file__).with_name("example_charset.txt")


def parse_sizes(value: str) -> list[int]:
    try:
        sizes = [int(item.strip()) for item in value.replace("，", ",").split(",")]
    except ValueError as exc:
        raise ValueError("字号请输入用逗号分隔的整数，例如 16,20,24") from exc
    if not 1 <= len(sizes) <= 3 or len(set(sizes)) != len(sizes):
        raise ValueError("请选择 1 至 3 个不重复的字号")
    if any(size < 6 or size > 128 for size in sizes):
        raise ValueError("字号范围为 6 至 128 像素")
    return sizes


def is_drawable(cp: int) -> bool:
    return 0 <= cp <= 0x10FFFF and unicodedata.category(chr(cp)) not in ("Cc", "Cf", "Cs")


def collect_codepoints(text: str, include_ascii: bool = False) -> list[int]:
    # Line breaks, tabs, BOM, joiners and other controls are not drawable glyphs.
    points = {ord(ch) for ch in text if is_drawable(ord(ch))}
    if include_ascii:
        points.update(range(0x20, 0x7F))
    if not points:
        raise ValueError("字符集为空；请输入要生成的字符，或勾选可打印 ASCII")
    return sorted(points)


def describe_points(points: list[int], limit: int = 20) -> str:
    text = " ".join(f"{chr(cp)}(U+{cp:04X})" for cp in points[:limit])
    return text + (f" …共 {len(points)} 个" if len(points) > limit else "")


@dataclass(frozen=True)
class FontSource:
    path: Path
    index: int
    family: str
    style: str
    supported: frozenset[int]

    @classmethod
    def load(cls, path: str | Path, index: int = 0) -> FontSource:
        source = Path(path).expanduser().resolve()
        if not source.is_file():
            raise ValueError(f"找不到字体文件：{source}")
        if index < 0:
            raise ValueError("字体集合索引不能为负数")
        try:
            with TTFont(source, fontNumber=index, lazy=True) as font:
                cmap = font.getBestCmap() or {}
                supported = frozenset(cp for cp, name in cmap.items()
                                      if 0 <= cp <= 0x10FFFF and not 0xD800 <= cp <= 0xDFFF
                                      and font.getGlyphID(name) != 0)
                family = font["name"].getDebugName(1) or source.stem
                style = font["name"].getDebugName(2) or "Regular"
            # Explicit BASIC layout: exported glyphs have independent integer advances.
            ImageFont.truetype(str(source), 16, index=index, layout_engine=ImageFont.Layout.BASIC)
        except Exception as exc:
            raise ValueError(f"无法加载字体或字体集合索引 {index}：{exc}") from exc
        return cls(source, index, family, style, supported)

    def rasterizer(self, pixel_size: int) -> ImageFont.FreeTypeFont:
        return ImageFont.truetype(str(self.path), pixel_size, index=self.index,
                                  layout_engine=ImageFont.Layout.BASIC)


def rasterize(font: ImageFont.FreeTypeFont, cp: int, threshold: int) -> Glyph:
    character = chr(cp)
    left, top, right, bottom = font.getbbox(character, anchor="ls")
    advance = math.floor(font.getlength(character) + 0.5)
    width, height = right - left, bottom - top
    if max(width, height) > 1024 or min(width, height) < 0:
        raise ValueError(f"字形尺寸超出范围：U+{cp:04X}")
    if not width or not height:
        return Glyph(cp, 0, 0, 0, 0, advance, b"")
    image = Image.new("L", (width, height), 0)
    ImageDraw.Draw(image).text((-left, -top), character, font=font, fill=255, anchor="ls")
    image = image.point([0 if value < threshold else 255 for value in range(256)])
    bounds = image.getbbox()
    if bounds is None:
        return Glyph(cp, 0, 0, 0, 0, advance, b"")
    image = image.crop(bounds)
    width, height = image.size
    stride = (width + 7) // 8
    packed = bytearray(stride * height)
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            if pixels[x, y]:
                packed[y * stride + x // 8] |= 0x80 >> (x % 8)
    return Glyph(cp, width, height, left + bounds[0], top + bounds[1], advance, bytes(packed))


def encode_font(source: FontSource, points: list[int], size: int, threshold: int,
                progress: Callable[[str], None] | None = None) -> bytes:
    font = source.rasterizer(size)
    records = bytearray()
    bitmaps = bytearray()
    ascent, descent = font.getmetrics()
    for index, cp in enumerate(points):
        glyph = rasterize(font, cp, threshold)
        records.extend(RECORD.pack(cp, len(bitmaps), glyph.width, glyph.height,
                                   glyph.x_offset, glyph.y_offset, glyph.advance, 0, len(glyph.bitmap)))
        bitmaps.extend(glyph.bitmap)
        # Include accented, combining and unusually tall glyphs in the line box.
        ascent = max(ascent, -glyph.y_offset)
        descent = max(descent, glyph.y_offset + glyph.height)
        if progress and ((index + 1) % 1024 == 0 or index + 1 == len(points)):
            progress(f"生成 {size}px：{index + 1:,} / {len(points):,} 字符")
    payload = records + bitmaps
    if HEADER.size + len(payload) > 0xFFFFFFFF:
        raise ValueError("字库超出 NBFT v1 的 4 GiB 文件长度上限")
    header = HEADER.pack(MAGIC, VERSION, HEADER.size, RECORD.size, size,
                         ascent, descent, max(1, size // 8), ROW_MSB, len(points),
                         HEADER.size, HEADER.size + len(records), HEADER.size + len(payload),
                         zlib.crc32(payload), bytes(24))
    return header + payload


def draw_bin_text(canvas: Image.Image, font: BitmapFont | FileBitmapFont, text: str,
                  box: tuple[int, int, int, int]) -> list[int]:
    """Draw exclusively from decoded BIN glyphs, with baseline positioning/wrapping."""
    left, top, right, bottom = box
    x = left
    baseline = top + font.ascent
    missing: set[int] = set()
    draw = ImageDraw.Draw(canvas)
    for char in text.replace("\r\n", "\n").replace("\r", "\n"):
        if char == "\n":
            x = left
            baseline += font.line_height
            continue
        if unicodedata.category(char) in ("Cc", "Cf", "Cs"):
            continue
        glyph = font.glyph(char)
        advance = glyph.advance if glyph else max(1, font.pixel_size)
        ink_right = max(advance, glyph.x_offset + glyph.width) if glyph else advance
        if x > left and x + ink_right > right:
            x = left
            baseline += font.line_height
        if baseline + font.descent > bottom:
            break
        if glyph:
            if glyph.width and glyph.height:
                mask = Image.frombytes("L", (glyph.width, glyph.height), glyph.pixels())
                canvas.paste(0, (x + glyph.x_offset, baseline + glyph.y_offset), mask)
        else:
            missing.add(ord(char))
            draw.rectangle((x + 1, baseline - font.pixel_size + 2,
                            x + advance - 2, baseline - 1), outline=0)
        x += advance
    return sorted(missing)


def make_preview(font: BitmapFont | FileBitmapFont, text: str) -> tuple[Image.Image, list[int]]:
    canvas = Image.new("L", (384, 168), 255)
    missing = draw_bin_text(canvas, font, text, (8, 6, 376, 162))
    return canvas, missing


def build_fonts(font_path: str | Path, charset: str, sizes: list[int], output: str | Path,
                font_index: int = 0, threshold: int = 128, include_ascii: bool = False,
                skip_missing: bool = False, preview_text: str = "",
                progress: Callable[[str], None] | None = None, all_glyphs: bool = False) -> dict:
    sizes = parse_sizes(",".join(str(size) for size in sizes))
    if not 1 <= threshold <= 255:
        raise ValueError("二值化阈值范围为 1 至 255")
    notify = progress or (lambda message: None)
    notify("检查字体与字符集…")
    source = FontSource.load(font_path, font_index)
    if all_glyphs and charset.strip():
        raise ValueError("全部字符模式不能同时指定自定义字符集")
    requested = (sorted(cp for cp in source.supported if is_drawable(cp))
                 if all_glyphs else collect_codepoints(charset, include_ascii))
    missing = [cp for cp in requested if cp not in source.supported]
    if missing and not skip_missing:
        raise ValueError("字体缺少以下字符，请更换字体或勾选跳过缺字：\n" + describe_points(missing))
    points = [cp for cp in requested if cp in source.supported]
    if not points:
        raise ValueError("字体不包含所请求的任何字符")
    output_dir = Path(output).expanduser().resolve()
    # Prevent accidentally overwriting the imported source font with an output name.
    if source.path.parent == output_dir:
        raise ValueError("输出目录请与输入字体所在目录分开")
    output_dir.mkdir(parents=True, exist_ok=True)
    sample = preview_text.strip() or (DEFAULT_TEXT if all_glyphs else charset.strip())
    sample = sample or "".join(chr(cp) for cp in points[:256])
    manifest = {
        "format": "NBFT", "version": VERSION,
        "source": {"file": source.path.name, "sha256": hashlib.sha256(source.path.read_bytes()).hexdigest(),
                   "font_index": source.index, "family": source.family, "style": source.style},
        "rasterization": {"bits_per_pixel": 1, "threshold": threshold,
                          "layout": "independent-glyphs", "compression": "none"},
        "charset_mode": "all-font-glyphs" if all_glyphs else "custom",
        "coverage": {"source_mapped_count": len(source.supported),
                     "excluded_control_format_count": sum(not is_drawable(cp) for cp in source.supported),
                     "bmp_count": sum(cp <= 0xFFFF for cp in points),
                     "supplementary_count": sum(cp > 0xFFFF for cp in points),
                     "first": f"U+{points[0]:04X}", "last": f"U+{points[-1]:04X}"},
        "requested_count": len(requested), "glyph_count": len(points),
        "missing_codepoints": [f"U+{cp:04X}" for cp in missing], "fonts": [],
    }
    # Build everything in staging; invalid inputs do not replace existing exports.
    with tempfile.TemporaryDirectory(prefix=".font-builder-", dir=output_dir) as staging_name:
        staging = Path(staging_name)
        previews = []
        for size in sizes:
            notify(f"生成 {size}px，共 {len(points)} 个字符…")
            data = encode_font(source, points, size, threshold, notify)
            name = f"font-{size}.bin"
            bin_path = staging / name
            bin_path.write_bytes(data)
            decoded = BitmapFont.load(bin_path)
            preview, preview_missing = make_preview(decoded, sample)
            preview_name = f"preview-{size}.png"
            preview.save(staging / preview_name)
            previews.append(preview)
            manifest["fonts"].append({
                "file": name, "pixel_size": size, "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(), "glyph_count": len(points),
                "ascent": decoded.ascent, "descent": decoded.descent,
                "line_gap": decoded.line_gap, "line_height": decoded.line_height,
                "max_bitmap_bytes": decoded.max_bitmap_bytes,
                "preview": preview_name,
                "preview_missing_codepoints": [f"U+{cp:04X}" for cp in preview_missing],
            })
            del data, decoded
        sheet = Image.new("RGB", (792, len(previews) * 390 + 12), "#e9edf1")
        draw = ImageDraw.Draw(sheet)
        for row, preview in enumerate(previews):
            y = row * 390 + 12
            entry = manifest["fonts"][row]
            label = (f"{entry['pixel_size']}px | {len(points)} glyphs | "
                     f"{entry['bytes']:,} bytes | line {entry['line_height']}px")
            draw.text((12, y), label, fill="#182434", font_size=18)
            sheet.paste(preview.resize((768, 336), Image.Resampling.NEAREST).convert("RGB"), (12, y + 34))
        sheet.save(staging / "preview.png")
        (staging / "charset.txt").write_text("".join(chr(cp) for cp in points) + "\n", encoding="utf-8")
        (staging / "missing.txt").write_text(
            "".join(f"U+{cp:04X}\t{chr(cp)}\n" for cp in missing), encoding="utf-8")
        (staging / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        # Publish manifest last. Files with other names are never removed.
        for path in sorted(staging.iterdir(), key=lambda path: path.name == "manifest.json"):
            path.replace(output_dir / path.name)
    notify(f"已生成 {len(sizes)} 份字库，总计 {sum(item['bytes'] for item in manifest['fonts']):,} bytes")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="点阵字库生成器：TTF/OTF/TTC → NBFT BIN（固件尚未接入）")
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="生成字库、清单和从 BIN 解码的预览")
    build.add_argument("--font", required=True, type=Path)
    build.add_argument("--font-index", type=int, default=0)
    build.add_argument("--charset", default="", help="直接输入字符；换行不生成字形")
    build.add_argument("--charset-file", type=Path, help="UTF-8 TXT；可与 --charset 合并")
    build.add_argument("--all-glyphs", action="store_true", help="导出字体全部可独立绘制的 Unicode 字符")
    build.add_argument("--sizes", default="16,20,24")
    build.add_argument("--threshold", type=int, default=128)
    build.add_argument("--include-ascii", action="store_true")
    build.add_argument("--skip-missing", action="store_true")
    build.add_argument("--preview-text", default="", help="留空则预览输入字符")
    build.add_argument("--output", required=True, type=Path)
    inspect = commands.add_parser("inspect", help="验证 BIN 的格式、索引和 CRC32")
    inspect.add_argument("bin", type=Path)
    inspect.add_argument("--text", default="", help="可选：将文字从 BIN 渲染为 384×168 PNG")
    inspect.add_argument("--output", type=Path)
    inspect.add_argument("--seek", action="store_true", help="流式校验 CRC、按需读取索引/单字，不整体载入 BIN")
    args = parser.parse_args()
    try:
        if args.command == "build":
            if args.all_glyphs and (args.charset or args.charset_file):
                raise ValueError("--all-glyphs 不能与 --charset / --charset-file 同时使用")
            charset = args.charset
            if args.charset_file:
                charset += args.charset_file.read_text(encoding="utf-8-sig")
            result = build_fonts(args.font, charset, parse_sizes(args.sizes), args.output,
                                 args.font_index, args.threshold, args.include_ascii,
                                 args.skip_missing, args.preview_text, print, all_glyphs=args.all_glyphs)
            print(f"覆盖 {result['glyph_count']:,} 字符；"
                  f"其中 U+FFFF 以上 {result['coverage']['supplementary_count']:,} 字符")
            if result["missing_codepoints"]:
                print(f"跳过 {len(result['missing_codepoints'])} 个缺字；详见 missing.txt")
            for entry in result["fonts"]:
                print(f"  {entry['file']}: {entry['bytes']:,} bytes，行高 {entry['line_height']}px")
            print(f"输出：{args.output.resolve()}")
        else:
            if args.seek:
                with FileBitmapFont(args.bin) as font:
                    print(f"NBFT v1 | {font.pixel_size}px | {font.glyph_count:,} glyphs | "
                          f"seek-based | CRC32 OK（未整体载入 BIN）")
                    if args.text:
                        preview, missing = make_preview(font, args.text)
                        output = args.output or args.bin.with_suffix(".png")
                        preview.save(output)
                        print(f"预览：{output.resolve()}")
                        if missing:
                            print("预览缺字（空框）：" + describe_points(missing))
                return 0
            font = BitmapFont.load(args.bin)
            print(f"NBFT v1 | {font.pixel_size}px | {len(font.codepoints)} glyphs | "
                  f"{len(font.data):,} bytes | line {font.line_height}px | CRC32 OK")
            if args.text:
                preview, missing = make_preview(font, args.text)
                output = args.output or args.bin.with_suffix(".png")
                preview.save(output)
                print(f"预览：{output.resolve()}")
                if missing:
                    print("预览缺字（空框）：" + describe_points(missing))
        return 0
    except (OSError, ValueError, struct.error) as exc:
        parser.exit(1, f"错误：{exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
