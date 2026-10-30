"""Export the Chinese glyphs needed before the SD font is available."""

from pathlib import Path
import re

from font_bin import BitmapFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/font_builder/output/Font"
OUTPUT = ROOT / "src/ui/assets/BootFontData.h"
TEXT14 = (
    "SD卡字库网络设置"
    "正在检查请稍候插入后按重新需要下载字体包可用缺少必要文件损坏获取"
    "连接设备热点成功将开始下载校验完成自动重启进入主菜单"
    "未网络中断重试请保持供电及连接准备就绪空间不足写入失败"
    "接收超时验证失败稍后再试月日周一二三四五六确认和通过已"
)
TEXT20 = "正在检查SD卡请插入需要下载字体包连接设备热点正在连接WiFi下载校验已准备就绪重新启动进入主菜单未完成"


def glyphs(size: int):
    font = BitmapFont.load(SOURCE / f"font-{size}.bin")
    found = []
    payload = bytearray()
    for character in sorted(set(TEXT14 if size == 14 else TEXT20)):
        if ord(character) < 128:
            continue
        glyph = font.glyph(character)
        if glyph is None:
            raise ValueError(f"missing {character!r} in {size}px font")
        found.append((ord(character), len(payload), glyph))
        payload.extend(glyph.bitmap)
    return found, payload


def main():
    page = (ROOT / "src/ui/pages/FirstBootPage.cpp").read_text(encoding="utf-8")
    literals = "".join(re.findall(r'"(?:[^"\\]|\\.)*"', page))
    missing = {char for char in literals if ord(char) >= 128} - set(TEXT14)
    if missing:
        raise ValueError("missing boot UI glyphs: " + "".join(sorted(missing)))
    lines = [
        "#pragma once", "", "#include <stdint.h>", "",
        "namespace BootFontData {",
        "struct Glyph { uint32_t codepoint; uint16_t offset; uint8_t width; uint8_t height;",
        "               int8_t xOffset; int8_t yOffset; uint8_t advance; };",
    ]
    for size in (14, 20):
        found, payload = glyphs(size)
        lines.append(f"inline constexpr Glyph kGlyphs{size}[] = {{")
        for codepoint, offset, glyph in found:
            lines.append(
                f"  {{{codepoint}U, {offset}U, {glyph.width}U, {glyph.height}U, "
                f"{glyph.x_offset}, {glyph.y_offset}, {glyph.advance}U}},"
            )
        lines.append("};")
        lines.append(f"inline constexpr uint8_t kBitmap{size}[] = {{")
        for start in range(0, len(payload), 16):
            lines.append("  " + ", ".join(f"0x{item:02X}" for item in payload[start:start + 16]) + ",")
        lines.extend(("};", ""))
    lines.append("}  // namespace BootFontData")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
