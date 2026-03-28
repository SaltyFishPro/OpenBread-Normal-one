#include "Segment7Font.h"

namespace Segment7Font {
namespace {
enum SegmentMask : uint8_t {
  kSegA = 1u << 0,
  kSegB = 1u << 1,
  kSegC = 1u << 2,
  kSegD = 1u << 3,
  kSegE = 1u << 4,
  kSegF = 1u << 5,
  kSegG = 1u << 6,
};

uint8_t maskForDigit(char c) {
  switch (c) {
    case '0':
      return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF;
    case '1':
      return kSegB | kSegC;
    case '2':
      return kSegA | kSegB | kSegG | kSegE | kSegD;
    case '3':
      return kSegA | kSegB | kSegC | kSegD | kSegG;
    case '4':
      return kSegF | kSegG | kSegB | kSegC;
    case '5':
      return kSegA | kSegF | kSegG | kSegC | kSegD;
    case '6':
      return kSegA | kSegF | kSegG | kSegE | kSegC | kSegD;
    case '7':
      return kSegA | kSegB | kSegC;
    case '8':
      return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG;
    case '9':
      return kSegA | kSegB | kSegC | kSegD | kSegF | kSegG;
    default:
      return 0;
  }
}

void drawRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1, int16_t x2,
              int16_t y2, uint16_t color) {
  if (x1 > x2) {
    const int16_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    const int16_t t = y1;
    y1 = y2;
    y2 = t;
  }

  const int16_t maxX = static_cast<int16_t>(canvas.getDisplayWidth() - 1);
  const int16_t maxY = static_cast<int16_t>(canvas.getDisplayHeight() - 1);
  if (maxX < 0 || maxY < 0) {
    return;
  }
  if (x2 < 0 || y2 < 0 || x1 > maxX || y1 > maxY) {
    return;
  }

  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > maxX) x2 = maxX;
  if (y2 > maxY) y2 = maxY;

  if (x1 > x2 || y1 > y2) {
    return;
  }
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1),
                             static_cast<uint>(x2), static_cast<uint>(y2), color);
}

void drawDigit(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y, char digit,
               const Style& style) {
  const uint8_t mask = maskForDigit(digit);
  if (mask == 0) {
    return;
  }

  const int16_t w = style.digitWidth;
  const int16_t h = style.digitHeight;
  const int16_t t = style.thickness;

  const int16_t midY = static_cast<int16_t>(y + h / 2);
  const int16_t gTop = static_cast<int16_t>(midY - t / 2);
  const int16_t gBottom = static_cast<int16_t>(gTop + t - 1);

  const int16_t topY1 = y;
  const int16_t topY2 = static_cast<int16_t>(y + t - 1);
  const int16_t bottomY1 = static_cast<int16_t>(y + h - t);
  const int16_t bottomY2 = static_cast<int16_t>(y + h - 1);
  const int16_t leftX1 = x;
  const int16_t leftX2 = static_cast<int16_t>(x + t - 1);
  const int16_t rightX1 = static_cast<int16_t>(x + w - t);
  const int16_t rightX2 = static_cast<int16_t>(x + w - 1);
  const int16_t hX1 = static_cast<int16_t>(x + t);
  const int16_t hX2 = static_cast<int16_t>(x + w - t - 1);
  const int16_t upperY1 = static_cast<int16_t>(y + t);
  const int16_t upperY2 = static_cast<int16_t>(midY - 1);
  const int16_t lowerY1 = midY;
  const int16_t lowerY2 = static_cast<int16_t>(y + h - t - 1);

  if (mask & kSegA) drawRect(canvas, hX1, topY1, hX2, topY2, style.onColor);
  if (mask & kSegB) drawRect(canvas, rightX1, upperY1, rightX2, upperY2, style.onColor);
  if (mask & kSegC) drawRect(canvas, rightX1, lowerY1, rightX2, lowerY2, style.onColor);
  if (mask & kSegD) drawRect(canvas, hX1, bottomY1, hX2, bottomY2, style.onColor);
  if (mask & kSegE) drawRect(canvas, leftX1, lowerY1, leftX2, lowerY2, style.onColor);
  if (mask & kSegF) drawRect(canvas, leftX1, upperY1, leftX2, upperY2, style.onColor);
  if (mask & kSegG) drawRect(canvas, hX1, gTop, hX2, gBottom, style.onColor);
}

void drawColon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y,
               const Style& style) {
  const int16_t dotSize = style.thickness;
  const int16_t cx = static_cast<int16_t>(x + (style.colonWidth - dotSize) / 2);
  const int16_t topY = static_cast<int16_t>(y + style.digitHeight / 3 - dotSize / 2);
  const int16_t bottomY = static_cast<int16_t>(y + (style.digitHeight * 2) / 3 - dotSize / 2);

  drawRect(canvas, cx, topY, static_cast<int16_t>(cx + dotSize - 1),
           static_cast<int16_t>(topY + dotSize - 1), style.onColor);
  drawRect(canvas, cx, bottomY, static_cast<int16_t>(cx + dotSize - 1),
           static_cast<int16_t>(bottomY + dotSize - 1), style.onColor);
}

int16_t glyphWidth(char c, const Style& style) {
  if (c == ':') {
    return style.colonWidth;
  }
  if (c >= '0' && c <= '9') {
    return style.digitWidth;
  }
  return 0;
}
}  // namespace

int16_t measureText(const char* text, const Style& style) {
  if (text == nullptr || text[0] == '\0') {
    return 0;
  }

  int16_t total = 0;
  bool firstGlyph = true;
  for (const char* p = text; *p != '\0'; ++p) {
    const int16_t w = glyphWidth(*p, style);
    if (w <= 0) {
      continue;
    }
    if (!firstGlyph) {
      total = static_cast<int16_t>(total + style.spacing);
    }
    total = static_cast<int16_t>(total + w);
    firstGlyph = false;
  }
  return total;
}

void drawText(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y,
              const char* text, const Style& style) {
  if (text == nullptr || text[0] == '\0') {
    return;
  }

  int16_t cursorX = x;
  bool firstGlyph = true;
  for (const char* p = text; *p != '\0'; ++p) {
    const char c = *p;
    const int16_t w = glyphWidth(c, style);
    if (w <= 0) {
      continue;
    }
    if (!firstGlyph) {
      cursorX = static_cast<int16_t>(cursorX + style.spacing);
    }
    if (c == ':') {
      drawColon(canvas, cursorX, y, style);
    } else {
      drawDigit(canvas, cursorX, y, c, style);
    }
    cursorX = static_cast<int16_t>(cursorX + w);
    firstGlyph = false;
  }
}

}  // namespace Segment7Font
