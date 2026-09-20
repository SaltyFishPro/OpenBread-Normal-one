#pragma once

#include <Arduino.h>

#include <ST7305_2p9_BW_DisplayDriver.h>

#include <stdint.h>

// Shared drawing primitives for pages that previously reimplemented their own
// clipped-rectangle and rounded-rectangle helpers.
namespace DrawUtils {

// ---- Clipped rectangles ------------------------------------------------

inline bool clipRectToDisplay(ST7305_2p9_BW_DisplayDriver& canvas, int16_t& x1, int16_t& y1,
                              int16_t& x2, int16_t& y2) {
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
    return false;
  }
  if (x2 < 0 || y2 < 0 || x1 > maxX || y1 > maxY) {
    return false;
  }
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > maxX) x2 = maxX;
  if (y2 > maxY) y2 = maxY;
  return true;
}

inline void drawClippedRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2, uint16_t color) {
  if (!clipRectToDisplay(canvas, x1, y1, x2, y2)) {
    return;
  }
  canvas.drawRectangle(static_cast<uint>(x1), static_cast<uint>(y1),
                       static_cast<uint>(x2), static_cast<uint>(y2), color);
}

inline void drawClippedFilledRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                                  int16_t x2, int16_t y2, uint16_t color) {
  if (!clipRectToDisplay(canvas, x1, y1, x2, y2)) {
    return;
  }
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1),
                             static_cast<uint>(x2), static_cast<uint>(y2), color);
}

// Cheap 1px corner cut used by static home-screen cards. This is intentionally
// not the same geometry as fillRoundRect(): it must stay pixel-identical to the
// original home layout.
inline void drawClippedPseudoRoundFilledRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1,
                                             int16_t y1, int16_t x2, int16_t y2,
                                             uint16_t color) {
  if ((x2 - x1) < 2 || (y2 - y1) < 2) {
    drawClippedFilledRect(canvas, x1, y1, x2, y2, color);
    return;
  }
  drawClippedFilledRect(canvas, static_cast<int16_t>(x1 + 1), y1,
                        static_cast<int16_t>(x2 - 1), y2, color);
  drawClippedFilledRect(canvas, x1, static_cast<int16_t>(y1 + 1), x2,
                        static_cast<int16_t>(y2 - 1), color);
}

inline void drawClippedPseudoRoundOutline(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1,
                                          int16_t y1, int16_t x2, int16_t y2,
                                          uint16_t borderColor, uint16_t innerColor) {
  drawClippedPseudoRoundFilledRect(canvas, x1, y1, x2, y2, borderColor);
  drawClippedPseudoRoundFilledRect(canvas, static_cast<int16_t>(x1 + 1),
                                   static_cast<int16_t>(y1 + 1),
                                   static_cast<int16_t>(x2 - 1),
                                   static_cast<int16_t>(y2 - 1), innerColor);
}

// ---- Circle-corner rounded rectangles ----------------------------------

// Previously duplicated verbatim in UiManager.cpp and ReaderPage.cpp.
inline void fillRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, int16_t radius, uint16_t color) {
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

  const int16_t w = static_cast<int16_t>(x2 - x1 + 1);
  const int16_t h = static_cast<int16_t>(y2 - y1 + 1);
  if (w <= 0 || h <= 0) {
    return;
  }

  int16_t r = radius;
  if (r < 0) {
    r = 0;
  }
  if (r > w / 2) {
    r = static_cast<int16_t>(w / 2);
  }
  if (r > h / 2) {
    r = static_cast<int16_t>(h / 2);
  }

  if (r == 0) {
    canvas.drawFilledRectangle(x1, y1, x2, y2, color);
    return;
  }

  canvas.drawFilledRectangle(static_cast<uint>(x1 + r), static_cast<uint>(y1),
                             static_cast<uint>(x2 - r), static_cast<uint>(y2), color);
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x1 + r - 1), static_cast<uint>(y2 - r), color);
  canvas.drawFilledRectangle(static_cast<uint>(x2 - r + 1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x2), static_cast<uint>(y2 - r), color);

  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
}

// Outline-only variant used by the reader's level cards (white page background).
inline void strokeRoundRectOnWhite(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y,
                                   int16_t w, int16_t h, int16_t radius, uint16_t color) {
  fillRoundRect(canvas, x, y, static_cast<int16_t>(x + w - 1),
                static_cast<int16_t>(y + h - 1), radius, color);
  if (w <= 2 || h <= 2) {
    return;
  }
  fillRoundRect(canvas, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                static_cast<int16_t>(x + w - 2), static_cast<int16_t>(y + h - 2),
                static_cast<int16_t>(radius - 1), ST7305_COLOR_WHITE);
}

// ---- Scanline rounded rectangle ----------------------------------------

inline int16_t roundRectInset(int16_t row, int16_t height, int16_t radius) {
  const int16_t edgeDistance = row < height / 2 ? row : height - row - 1;
  if (edgeDistance >= radius) {
    return 0;
  }
  const int16_t dy = static_cast<int16_t>(radius - edgeDistance);
  int16_t dx = radius;
  while (dx * dx + dy * dy > radius * radius) {
    --dx;
  }
  return static_cast<int16_t>(radius - dx);
}

// Fill + outline with per-row clipping. Keeps the original shape in card
// coordinates so an animated card slides off-screen instead of shrinking.
inline void drawRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y,
                          int16_t width, int16_t height, int16_t radius, uint16_t fill,
                          uint16_t outline) {
  if (width <= 0 || height <= 0) {
    return;
  }
  if (x + width <= 0 || y + height <= 0 || x >= canvas.getDisplayWidth() ||
      y >= canvas.getDisplayHeight()) {
    return;
  }
  int16_t r = radius;
  if (r > (width - 1) / 2) {
    r = static_cast<int16_t>((width - 1) / 2);
  }
  if (r > (height - 1) / 2) {
    r = static_cast<int16_t>((height - 1) / 2);
  }
  if (r < 0) r = 0;

  const int16_t firstRow = y < 0 ? static_cast<int16_t>(-y) : 0;
  const int16_t visibleHeight = static_cast<int16_t>(canvas.getDisplayHeight() - y);
  const int16_t endRow = height < visibleHeight ? height : visibleHeight;
  for (int16_t row = firstRow; row < endRow; ++row) {
    const int16_t inset = roundRectInset(row, height, r);
    const int16_t lineY = static_cast<int16_t>(y + row);
    if (fill == outline || row == 0 || row == height - 1 || width <= 2) {
      canvas.drawFastHLine(x + inset, lineY, width - 2 * inset, outline);
      continue;
    }
    const int16_t innerRadius = r > 0 ? static_cast<int16_t>(r - 1) : 0;
    const int16_t innerInset =
        static_cast<int16_t>(1 + roundRectInset(row - 1, height - 2, innerRadius));
    const int16_t borderWidth = static_cast<int16_t>(innerInset - inset);
    canvas.drawFastHLine(x + inset, lineY, borderWidth, outline);
    canvas.drawFastHLine(x + innerInset, lineY, width - 2 * innerInset, fill);
    canvas.drawFastHLine(x + width - innerInset, lineY, borderWidth, outline);
  }
}

}  // namespace DrawUtils
