#!/usr/bin/env python3
"""Verify raw/RLE shared exports by linking two independent consumers."""

from dataclasses import replace
from pathlib import Path
import subprocess

import gif_to_c as core


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/animation-export-test"


def expect_error(action, message: str) -> None:
    try:
        action()
    except RuntimeError:
        return
    raise AssertionError(message)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    frames = [[0] * 8, [0xAA, 0xA8, 0x55, 0x50] * 2, [0xFF, 0xF8] * 4]
    options = core.ConvertOptions("sample_raw", 13, 4, 128, False, "nearest", False, 255,
                                  frame_delay=73)
    core.write_output(OUT / "raw.h", options, frames)
    core.write_output(OUT / "rle.h", replace(options, name="sample_rle", rle=True), frames)
    equal_frames = [[0] * 8, [0xFF] * 8]
    core.write_output(OUT / "fixed_raw.h", replace(options, name="fixed_raw"), equal_frames)
    core.write_output(OUT / "fixed_rle.h", replace(options, name="fixed_rle", rle=True), equal_frames)
    core.write_output(OUT / "legacy_rle.h",
                      replace(options, name="legacy_rle", rle=True, header_only=True), frames)
    large_frames = [[0] * 1900, [i % 256 for i in range(1900)]]
    large_options = replace(options, width=150, height=100)
    core.write_output(OUT / "large_raw.h", replace(large_options, name="large_raw"), large_frames)
    core.write_output(OUT / "large_rle.h", replace(large_options, name="large_rle", rle=True), large_frames)

    # uint16 offsets are relative to the payload, even if table + data exceed 64 KiB.
    long_frame = [1, 0] * 16383
    boundary = core.pack_rle_frames([long_frame, long_frame, [1, 0]], len(long_frame))
    assert boundary[:8] == [0, 0, 254, 127, 252, 255, 254, 255]
    assert len(boundary) == 65542
    assert not core.pack_rle_frames([long_frame, long_frame, [1, 0] * 2], len(long_frame)), "Offsets overflowed"
    assert not core.pack_rle_frames([[8, 0], [8, 255]], 2), "Unprofitable packing selected"
    original = (OUT / "rle.cpp").read_bytes()
    core.write_output(OUT / "rle.h", replace(options, name="sample_rle", rle=True), frames)
    assert (OUT / "rle.cpp").read_bytes() == original, "Re-export changed shared data"
    core.write_output(OUT / "legacy.h", replace(options, name="legacy", header_only=True), frames)
    assert not (OUT / "legacy.cpp").exists(), "Legacy mode emitted a companion source"
    expect_error(lambda: core.write_output(OUT / "raw.h", replace(options, header_only=True), frames),
                 "Legacy mode accepted a stale companion source")
    expect_error(lambda: core.write_output(OUT / "invalid.cpp", options, frames),
                 "Shared mode accepted a non-header path")
    (OUT / "handwritten.cpp").write_text("// Handwritten file\n", encoding="utf-8")
    expect_error(lambda: core.write_output(OUT / "handwritten.h", options, frames),
                 "Shared mode overwrote an unrelated source")
    assert not (OUT / "handwritten.h").exists(), "Rejected export still wrote a header"

    # Exercise the actual GIF and still-image export entry points as well.
    from PIL import Image
    images = [Image.new("RGB", (13, 4), color) for color in ("white", "black")]
    gif = OUT / "input.gif"
    images[0].save(gif, save_all=True, append_images=images[1:], duration=100, loop=0)
    count, _ = core.convert_gif_to_c(gif, OUT / "gif.h", replace(options, name="gif", rle=True))
    assert count == 2 and (OUT / "gif.cpp").exists(), "GIF did not export a shared pair"
    images[1].save(OUT / "input.png")
    count, _ = core.convert_image_to_c(OUT / "input.png", OUT / "image.h", replace(options, name="image"))
    assert count == 1 and (OUT / "image.cpp").exists(), "Image did not export a shared pair"

    (OUT / "consumer.cpp").write_text('''#include "raw.h"
#include "rle.h"
const void* rawFromOtherFile() { return sample_raw_frames; }
const void* rleFromOtherFile() { return sample_rle_frames; }
''', encoding="utf-8")
    expected = ",".join(str(value) for frame in frames for value in frame)
    (OUT / "main.cpp").write_text('''#include <cassert>
#include "raw.h"
#include "rle.h"
#include "legacy.h"
#include "gif.h"
#include "image.h"
#include "fixed_rle.h"
#include "legacy_rle.h"
const void* rawFromOtherFile();
const void* rleFromOtherFile();
int main() {
  static_assert(SAMPLE_RAW_FRAME_COUNT == 3 && SAMPLE_RLE_FRAME_COUNT == 3);
  static_assert(SAMPLE_RAW_FRAME_DELAY == 73 && SAMPLE_RLE_FRAME_DELAY == 73);
  static_assert(sizeof(sample_raw_frames) == 24 && sizeof(legacy_frames) == 24);
  static_assert(SAMPLE_RLE_FRAME_BYTES == 0x8000U);
  static_assert(sizeof(sample_rle_frames) == 42 && sizeof(legacy_rle_frames) == 42);
  static_assert(FIXED_RLE_FRAME_BYTES == (0x8000U | 2U));
  assert(rawFromOtherFile() == sample_raw_frames);
  assert(rleFromOtherFile() == sample_rle_frames);
  const unsigned char expected[] = {''' + expected + '''};
  assert(std::memcmp(sample_raw_frames, expected, sizeof(expected)) == 0);
  assert(std::memcmp(legacy_frames, expected, sizeof(expected)) == 0);
  for (size_t frame = 0; frame < SAMPLE_RLE_FRAME_COUNT; ++frame) {
    const auto* blob = &sample_rle_frames[0][0];
    const size_t start = blob[frame * 2] | (blob[frame * 2 + 1] << 8);
    const size_t end = blob[frame * 2 + 2] | (blob[frame * 2 + 3] << 8);
    const auto* data = blob + (SAMPLE_RLE_FRAME_COUNT + 1) * 2;
    assert(end > start && end <= sizeof(sample_rle_frames) - 8);
    size_t decoded = 0;
    for (size_t pos = start; pos + 1 < end; pos += 2) {
      const auto count = data[pos];
      assert(count);
      for (size_t i = 0; i < count; ++i) {
        assert(decoded < 8);
        assert(data[pos + 1] == expected[frame * 8 + decoded++]);
      }
    }
    assert(decoded == 8);
  }
  assert(gif_frames[0][1] == 0 && gif_frames[1][1] == 255);
  assert(image_frames[0][0] == 255 && image_frames[0][1] == 248);
}
''', encoding="utf-8")
    subprocess.run(["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang",
                    "c++", "-std=c++17", "-O1", "-I", str(ROOT / "tools/answers_book_preview/stubs"),
                    *[str(OUT / name) for name in ("consumer.cpp", "main.cpp", "raw.cpp", "rle.cpp", "gif.cpp", "image.cpp", "fixed_rle.cpp")],
                    "-o", str(OUT / "verify.exe")], check=True, cwd=ROOT)
    subprocess.run([str(OUT / "verify.exe")], check=True, cwd=ROOT)
    subprocess.run(["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang",
                    "c++", "-std=c++17", "-O1", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                    "-I", str(ROOT / "tools/answers_book_preview/stubs"), "-I", str(ROOT), "-I", str(OUT),
                    "-I", str(ROOT / "lib/ST7305_MonoTFT_Library/src"),
                    str(Path(__file__).with_name("decoder_verify.cpp")),
                    str(ROOT / "src/ui/IconBitmap.cpp"),
                    str(ROOT / "lib/ST7305_MonoTFT_Library/src/ST73XX_UI.cpp"),
                    str(ROOT / "lib/ST7305_MonoTFT_Library/src/ST7305_2p9_BW_DisplayDriver.cpp"),
                    *[str(OUT / name) for name in ("raw.cpp", "rle.cpp", "fixed_raw.cpp", "fixed_rle.cpp",
                                                  "large_raw.cpp", "large_rle.cpp")],
                    "-o", str(OUT / "decoder-verify.exe")], check=True, cwd=ROOT)
    subprocess.run([str(OUT / "decoder-verify.exe")], check=True, cwd=ROOT)
    print("PASS: raw/fixed/packed RLE, offset limits/fallback, shared addresses, metadata, GIF/image export and legacy guards")


if __name__ == "__main__":
    main()
