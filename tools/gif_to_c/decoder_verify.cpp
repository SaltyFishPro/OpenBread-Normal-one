// Exercise the production decoder against raw pixels on the real ST7305 buffer.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/bsp/BoardConfig.h"
#include "src/ui/IconBitmap.h"
#include "raw.h"
#include "rle.h"
#include "fixed_raw.h"
#include "fixed_rle.h"
#include "large_raw.h"
#include "large_rle.h"
#include "legacy_rle.h"

#define ANIMATION(name, upper) \
  {&name##_frames[0][0], upper##_FRAME_BYTES, upper##_FRAME_WIDTH, \
   upper##_FRAME_HEIGHT, upper##_FRAME_DELAY, upper##_FRAME_COUNT}

struct Placement {
  int16_t x, y, w, h, top, bottom;
  bool invert;
};

static void compare(ST7305_2p9_BW_DisplayDriver& canvas, const IconBitmap::Anim& raw,
                    const IconBitmap::Anim& encoded) {
  constexpr Placement placements[] = {
      {20, 30, 100, 100, 0, 167, false},
      {350, -8, 44, 44, 10, 160, true},
      {-5, 120, 150, 80, 0, 167, false},
      {50, 0, 150, 168, 0, 167, true}};
  uint8_t expected[8064];
  uint8_t actual[8064];
  // Repeat and cross the final frame to check cache hits and modulo wrap.
  for (uint16_t i = 0; i < raw.frameCount * 2 + 1; ++i) {
    assert(IconBitmap::frameAt(encoded, i * encoded.frameDelayMs) == i % raw.frameCount);
    assert(IconBitmap::frameAt(encoded, i * encoded.frameDelayMs + encoded.frameDelayMs - 1)
           == i % raw.frameCount);
    for (const auto& p : placements) {
      canvas.clearDisplay();
      IconBitmap::drawFrame(canvas, raw, i, p.x, p.y, p.w, p.h, p.invert, p.top, p.bottom);
      canvas.copyFrameBufferTo(expected);
      canvas.clearDisplay();
      IconBitmap::drawFrame(canvas, encoded, i, p.x, p.y, p.w, p.h, p.invert, p.top, p.bottom);
      canvas.copyFrameBufferTo(actual);
      assert(std::memcmp(expected, actual, sizeof(actual)) == 0);
    }
  }
}

int main() {
  const IconBitmap::Anim raw = ANIMATION(sample_raw, SAMPLE_RAW);
  const IconBitmap::Anim packed = ANIMATION(sample_rle, SAMPLE_RLE);
  const IconBitmap::Anim legacy = ANIMATION(legacy_rle, LEGACY_RLE);
  const IconBitmap::Anim fixedRaw = ANIMATION(fixed_raw, FIXED_RAW);
  const IconBitmap::Anim fixed = ANIMATION(fixed_rle, FIXED_RLE);
  const IconBitmap::Anim largeRaw = ANIMATION(large_raw, LARGE_RAW);
  const IconBitmap::Anim large = ANIMATION(large_rle, LARGE_RLE);
  static_assert(LARGE_RLE_FRAME_BYTES == IconBitmap::kPackedRleFrameBytes);
  ST7305_2p9_BW_DisplayDriver canvas(SPI);
  canvas.setRotation(BoardConfig::kDisplayRotation);
  compare(canvas, raw, packed);
  compare(canvas, fixedRaw, fixed);
  compare(canvas, largeRaw, large);  // More than 1300 bytes: row decode fallback.
  compare(canvas, raw, legacy);

  // Invalid or empty descriptors leave the buffer untouched.
  canvas.clearDisplay();
  uint8_t expected[8064], actual[8064];
  canvas.copyFrameBufferTo(expected);
  IconBitmap::Anim empty;
  assert(IconBitmap::frameAt(empty, 1000) == 0);
  IconBitmap::drawFrame(canvas, empty, 0, 0, 0, 100, 100, false, 0, 167);
  IconBitmap::drawFrame(canvas, packed, 0, 0, 0, 0, 100, false, 0, 167);
  const uint8_t badOffsets[] = {2, 0, 0, 0};
  const IconBitmap::Anim bad = {badOffsets, IconBitmap::kPackedRleFrameBytes, 13, 4, 73, 1};
  IconBitmap::drawFrame(canvas, bad, 0, 0, 0, 100, 100, false, 0, 167);
  canvas.copyFrameBufferTo(actual);
  assert(std::memcmp(expected, actual, sizeof(actual)) == 0);
  std::puts("PASS: production ST7305 raw/RLE buffers match, including cache/fallback, scaling, clipping, timing and wrap");
}
