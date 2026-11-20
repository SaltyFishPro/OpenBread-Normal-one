#pragma once

#include <stdint.h>

class SdBitmapFont;
class ST7305_2p9_BW_DisplayDriver;

namespace SdBitmapText {
struct Clip { int16_t x1, y1, x2, y2; };

namespace detail {
// Runs of set bits inside one byte (MSB first), at most four per byte.
struct ByteRuns {
  uint8_t count = 0;
  uint8_t start[4] = {};
  uint8_t length[4] = {};
};

constexpr ByteRuns byteRuns(uint8_t value) {
  ByteRuns runs;
  int bit = 0;
  while (bit < 8) {
    if ((value & (0x80U >> bit)) == 0U) { ++bit; continue; }
    int end = bit;
    while (end < 8 && (value & (0x80U >> end)) != 0U) ++end;
    runs.start[runs.count] = static_cast<uint8_t>(bit);
    runs.length[runs.count] = static_cast<uint8_t>(end - bit);
    ++runs.count;
    bit = end;
  }
  return runs;
}

struct ByteRunTable {
  ByteRuns entries[256] = {};
  constexpr ByteRunTable() {
    for (int value = 0; value < 256; ++value)
      entries[value] = byteRuns(static_cast<uint8_t>(value));
  }
};
inline constexpr ByteRunTable kByteRuns{};
}  // namespace detail

// Calls emit(x1, x2) for every maximal horizontal ink run of one 1bpp row, both
// bounds inclusive and in glyph-local columns. Skips eight blank columns at a time
// and walks whole runs instead of testing every pixel. Padding bits outside
// `width` must be zero (readGlyphBitmap validates that at load time).
template <typename Emit>
inline void forEachInkRun(const uint8_t* row, uint16_t width, Emit emit) {
  int32_t runStart = -1;
  int32_t runEnd = -1;
  for (uint16_t base = 0; base < width; base = static_cast<uint16_t>(base + 8U)) {
    uint8_t bits = row[base >> 3];
    const uint16_t remaining = static_cast<uint16_t>(width - base);
    if (remaining < 8U)
      bits = static_cast<uint8_t>(bits & static_cast<uint8_t>(0xFFU << (8U - remaining)));
    const detail::ByteRuns& runs = detail::kByteRuns.entries[bits];
    if (runs.count == 0U) {
      if (runStart >= 0) { emit(runStart, runEnd); runStart = -1; }
      continue;
    }
    for (uint8_t i = 0; i < runs.count; ++i) {
      const int32_t start = static_cast<int32_t>(base) + runs.start[i];
      const int32_t end = start + runs.length[i] - 1;
      if (runStart >= 0 && start == runEnd + 1) { runEnd = end; continue; }
      if (runStart >= 0) emit(runStart, runEnd);
      runStart = start;
      runEnd = end;
    }
  }
  if (runStart >= 0) emit(runStart, runEnd);
}

// Strict Unicode scalar decoding, including four-byte UTF-8. Invalid bytes use U+FFFD.
bool nextCodepoint(const char*& text, uint32_t& codepoint);
int32_t measureUTF8(SdBitmapFont& font, const char* text);
// Transparent ink, baseline coordinates. Missing glyphs use a box and one em advance.
// When `advance` is not null it receives the pen advance (widest line for multi-line
// text) from the same glyph walk, so callers do not have to measure a second time.
bool drawUTF8(ST7305_2p9_BW_DisplayDriver& canvas, SdBitmapFont& font,
               int16_t x, int16_t baseline, const char* text, uint16_t color, Clip clip,
               int16_t* advance = nullptr);
}
