#include "SdBitmapText.h"

#include <ST7305_2p9_BW_DisplayDriver.h>
#include <algorithm>
#include "../services/SdBitmapFont.h"

namespace SdBitmapText {
bool nextCodepoint(const char*& text, uint32_t& codepoint) {
  if (!text || !*text) return false;
  const auto* p = reinterpret_cast<const uint8_t*>(text);
  const uint8_t first = *p;
  if (first < 0x80U) { codepoint = first; ++text; return true; }
  uint8_t count;
  uint32_t minimum;
  if (first >= 0xC2U && first <= 0xDFU) { count = 2; minimum = 0x80U; }
  else if (first >= 0xE0U && first <= 0xEFU) { count = 3; minimum = 0x800U; }
  else if (first >= 0xF0U && first <= 0xF4U) { count = 4; minimum = 0x10000U; }
  else { codepoint = 0xFFFDU; ++text; return true; }
  uint32_t cp = first & (0x7FU >> count);
  for (uint8_t i = 1; i < count; ++i) {
    if (p[i] < 0x80U || p[i] > 0xBFU) { codepoint = 0xFFFDU; ++text; return true; }
    cp = (cp << 6U) | (p[i] & 0x3FU);
  }
  if (cp < minimum || cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) {
    codepoint = 0xFFFDU;
    ++text;
    return true;
  }
  text += count;
  codepoint = cp;
  return true;
}

int32_t measureUTF8(SdBitmapFont& font, const char* text) {
  if (font.state() != SdBitmapFont::State::Ready) return 0;
  int32_t width = 0, maximum = 0;
  uint32_t cp;
  while (nextCodepoint(text, cp)) {
    if (cp == '\r') continue;
    if (cp == '\n') { maximum = std::max(maximum, width); width = 0; continue; }
    SdBitmapFont::Glyph glyph;
    const bool found = font.glyph(cp, glyph);
    if (font.state() != SdBitmapFont::State::Ready) return 0;
    width += found ? glyph.advance : font.metrics().pixelSize;
  }
  return std::max(maximum, width);
}

namespace {
void line(ST7305_2p9_BW_DisplayDriver& canvas, int32_t x1, int32_t x2, int32_t y,
          uint16_t color, Clip clip) {
  if (y < clip.y1 || y > clip.y2) return;
  x1 = std::max(x1, int32_t(clip.x1));
  x2 = std::min(x2, int32_t(clip.x2));
  if (x1 <= x2) canvas.drawFastHLine(x1, y, x2 - x1 + 1, color);
}

void missing(ST7305_2p9_BW_DisplayDriver& canvas, int32_t x, int32_t baseline,
             uint16_t size, uint16_t color, Clip clip) {
  const int32_t top = baseline - size + 2;
  const int32_t right = x + size - 3;
  line(canvas, x + 1, right, top, color, clip);
  line(canvas, x + 1, right, baseline - 1, color, clip);
  for (int32_t y = std::max(top + 1, int32_t(clip.y1));
       y < baseline - 1 && y <= clip.y2; ++y) {
    line(canvas, x + 1, x + 1, y, color, clip);
    line(canvas, right, right, y, color, clip);
  }
}
}

bool drawUTF8(ST7305_2p9_BW_DisplayDriver& canvas, SdBitmapFont& font,
               int16_t x, int16_t baseline, const char* text, uint16_t color, Clip clip) {
  if (font.state() != SdBitmapFont::State::Ready) return false;
  clip.x1 = std::max<int16_t>(0, clip.x1);
  clip.y1 = std::max<int16_t>(0, clip.y1);
  clip.x2 = std::min<int16_t>(canvas.getDisplayWidth() - 1, clip.x2);
  clip.y2 = std::min<int16_t>(canvas.getDisplayHeight() - 1, clip.y2);
  if (clip.x1 > clip.x2 || clip.y1 > clip.y2) return true;
  int32_t penX = x, penY = baseline;
  uint32_t cp;
  uint8_t row[128];  // NBFT v1 permits up to 1024 bits per row.
  while (nextCodepoint(text, cp)) {
    if (cp == '\r') continue;
    if (cp == '\n') { penX = x; penY += font.metrics().lineHeight(); continue; }
    SdBitmapFont::Glyph glyph;
    const bool found = font.glyph(cp, glyph);
    if (font.state() != SdBitmapFont::State::Ready) return false;
    if (!found) {
      missing(canvas, penX, penY, font.metrics().pixelSize, color, clip);
      penX += font.metrics().pixelSize;
      continue;
    }
    const int32_t left = penX + glyph.xOffset;
    const int32_t top = penY + glyph.yOffset;
    const int32_t firstRow = std::max<int32_t>(0, clip.y1 - top);
    const int32_t lastRow = std::min<int32_t>(glyph.height - 1, clip.y2 - top);
    if (left <= clip.x2 && left + glyph.width > clip.x1) {
      for (int32_t y = firstRow; y <= lastRow; ++y) {
        if (!font.readRow(glyph, y, row, sizeof(row))) return false;
        int32_t runStart = -1;
        for (uint16_t col = 0; col <= glyph.width; ++col) {
          const bool ink = col < glyph.width && (row[col / 8U] & (0x80U >> (col % 8U)));
          if (ink && runStart < 0) runStart = col;
          if (!ink && runStart >= 0) {
            line(canvas, left + runStart, left + col - 1, top + y, color, clip);
            runStart = -1;
          }
        }
      }
    }
    penX += glyph.advance;
  }
  return true;
}
}
