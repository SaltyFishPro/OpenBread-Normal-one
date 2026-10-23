#!/usr/bin/env python3
"""Compare production large-image RLE drawing to raw pixels and count stream reads."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, default=ROOT / "src/ui/IconBitmap.cpp")
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/large-rle-test")
    parser.add_argument("--allow-rescan", action="store_true", help="Measure an older decoder")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    includes, entries = [], []
    for path in sorted((ROOT / "src/ui/assets").rglob("*.h")):
        source = path.read_text(encoding="utf-8", errors="ignore")
        width = re.search(r"#define (\w+)_FRAME_WIDTH (\d+)", source)
        height = re.search(r"#define \w+_FRAME_HEIGHT (\d+)", source)
        if not width or not height or "_FRAME_RLE_BYTES" not in source:
            continue
        if ((int(width[2]) + 7) // 8) * int(height[1]) <= 1300:
            continue
        symbol = re.search(r"const byte PROGMEM (\w+)_frames", source)
        assert symbol, path
        upper, name = width[1], symbol[1]
        includes.append(f'#include "{path.relative_to(ROOT).as_posix()}"')
        entries.append(f'  {{"{name}", {{&{name}_frames[0][0], {upper}_FRAME_BYTES, '
                       f'{upper}_FRAME_WIDTH, {upper}_FRAME_HEIGHT, '
                       f'{upper}_FRAME_DELAY, {upper}_FRAME_COUNT}}}},')
    assert entries, "No large RLE resources found"
    (output / "large_assets.h").write_text(
        "\n".join(includes + ["struct Asset { const char* name; IconBitmap::Anim anim; };",
                              "const Asset assets[] = {"] + entries + ["};", ""]), encoding="utf-8")
    command = ["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang", "c++",
               "-std=c++17", "-O1", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
               "-I", str(ROOT / "tools/answers_book_preview/stubs"), "-I", str(ROOT),
               "-I", str(ROOT / "src/ui"), "-I", str(output),
               "-I", str(ROOT / "lib/ST7305_MonoTFT_Library/src"),
               "-include", str(Path(__file__).with_name("read_counter.h")),
               str(Path(__file__).with_name("large_decoder_verify.cpp")), str(args.decoder.resolve()),
               str(ROOT / "lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp"),
               str(ROOT / "lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp"),
               "-o", str(output / "verify.exe")]
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(output / "verify.exe"), str(output),
                    "measure" if args.allow_rescan else "linear"], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
