#include "SdUiText.h"
#include "../bsp/DisplayMonoTft.h"
#include "DrawUtils.h"
#include <algorithm>

namespace SdUiText {
namespace {
bool hasUnicode(const char* value) {
  for (; value && *value; ++value) if (static_cast<uint8_t>(*value) >= 0x80U) return true;
  return false;
}

bool hasLineBreak(const char* value) {
  for (; value && *value; ++value) if (*value == '\n') return true;
  return false;
}
}
bool ready(const SdBitmapFont* font, uint16_t maxLineHeight) {
  return font && font->state() == SdBitmapFont::State::Ready &&
         font->metrics().lineHeight() > 0 && font->metrics().lineHeight() <= maxLineHeight;
}
int16_t width(DisplayMonoTft& display, SdBitmapFont* font, const char* value) {
  return ready(font) ? static_cast<int16_t>(std::min<int32_t>(32767, SdBitmapText::measureUTF8(*font, value)))
                     : display.text().getUTF8Width(value);
}
void draw(DisplayMonoTft& display, SdBitmapFont* font, int16_t x, int16_t baseline,
          const char* value, uint16_t color, SdBitmapText::Clip clip) {
  if (ready(font)) {
    (void)SdBitmapText::drawUTF8(display.canvas(), *font, x, baseline, value, color, clip);
    return;
  }
  // The outer fixed UI scope can supply cached Chinese after the file closes.
  // If startup loading failed, the resident Latin/number face remains usable.
  display.text().setForegroundColor(color);
  display.text().drawUTF8(x, baseline, value);
}
void centered(DisplayMonoTft& display, SdBitmapFont* font, int16_t centerX, int16_t baseline,
              const char* value, uint16_t color, SdBitmapText::Clip clip) {
  const int16_t x = centerX - width(display, font, value) / 2;
  draw(display, font, x, baseline, value, color, clip);
}
Scope::Scope(DisplayMonoTft& display, SdBitmapFont* font)
    : display_(display), font_(font), previous_(display.text().utf8Renderer()) {
  // NBFT line height includes leading (the current 14px full font is 28px).
  // Compact pages keep their own clipping cells; do not reject the full font by
  // confusing line height with the visible 14px glyph size.
  if (ready(font_, 32))
    display_.text().setUTF8Renderer(u8g2_font_7x14_tf, this, measure, render);
  else font_ = nullptr;
}
Scope::~Scope() {
  display_.text().setUTF8Renderer(previous_);
}
int16_t Scope::measure(void* self, const char* value) {
  auto& scope = *static_cast<Scope*>(self);
  return static_cast<int16_t>(std::min<int32_t>(32767, SdBitmapText::measureUTF8(*scope.font_, value)));
}
int16_t Scope::render(void* self, int16_t x, int16_t y, const char* value,
                       uint16_t fg, uint16_t bg, bool transparent) {
  auto& scope = *static_cast<Scope*>(self);
  auto& canvas = scope.display_.canvas();
  // Compact external-text pages have at least 24px rows; keep descenders inside the cell.
  const SdBitmapText::Clip clip = {0, static_cast<int16_t>(y - 17),
                                    static_cast<int16_t>(scope.display_.width() - 1),
                                    static_cast<int16_t>(y + 5)};
  // The draw walk already advances the pen, so transparent text reuses that width
  // instead of looking every glyph up a second time. Opaque text clears the whole
  // advance before the ink, so it still needs the measured width up front; so does
  // a clip whose rows all fall outside the panel (drawUTF8 then reports no width).
  const bool rowsVisible = y >= -5 && y - 17 <= scope.display_.height() - 1;
  if (transparent && rowsVisible) {
    int16_t advance = 0;
    (void)SdBitmapText::drawUTF8(canvas, *scope.font_, x, y, value, fg, clip, &advance);
    return advance;
  }
  const int16_t advance = measure(self, value);
  if (!transparent && advance > 0)
    DrawUtils::drawClippedFilledRect(canvas, x, y - 17, x + advance - 1, y + 5, bg);
  (void)SdBitmapText::drawUTF8(canvas, *scope.font_, x, y, value, fg, clip);
  return advance;
}

FixedScope::FixedScope(DisplayMonoTft& display, const UiFontCache& cache, bool includeAscii)
    : display_(display), cache_(cache), includeAscii_(includeAscii), previous_(display.text().utf8Renderer()) {
  if (cache.state() == UiFontCache::State::Ready)
    display.text().setUTF8Renderer(u8g2_font_7x14_tf, this, measure, render);
}
FixedScope::~FixedScope() { display_.text().setUTF8Renderer(previous_); }

int16_t FixedScope::measure(void* self, const char* value) {
  auto& scope = *static_cast<FixedScope*>(self);
  if (!scope.includeAscii_ && !hasUnicode(value)) {
    auto& text = scope.display_.text();
    const auto renderer = text.utf8Renderer();
    text.setUTF8Renderer(nullptr, nullptr, nullptr, nullptr);
    const auto result = text.getUTF8Width(value);
    text.setUTF8Renderer(renderer);
    return result;
  }
  int32_t width = 0;
  uint32_t cp;
  while (SdBitmapText::nextCodepoint(value, cp)) {
    const auto* glyph = scope.cache_.glyph(cp);
    if (cp != '\r' && cp != '\n') width += glyph ? glyph->advance : 14;
  }
  return static_cast<int16_t>(std::min<int32_t>(width, 32767));
}

int16_t FixedScope::render(void* self, int16_t x, int16_t y, const char* value,
                            uint16_t fg, uint16_t bg, bool transparent) {
  auto& scope = *static_cast<FixedScope*>(self);
  auto& canvas = scope.display_.canvas();
  if (!scope.includeAscii_ && !hasUnicode(value)) {
    auto& text = scope.display_.text();
    const auto renderer = text.utf8Renderer();
    text.setUTF8Renderer(nullptr, nullptr, nullptr, nullptr);
    const auto result = text.drawUTF8(x, y, value);
    text.setUTF8Renderer(renderer);
    return result;
  }
  // The pen walk below knows the advance; measuring first would look every glyph
  // up twice. Line breaks (measure reports the widest line) and fully clipped rows
  // keep the measured width.
  const bool measuredAdvance = hasLineBreak(value);
  const int16_t advance = measuredAdvance ? measure(self, value) : 0;
  const int32_t top = std::max<int32_t>(0, y - 17);
  const int32_t bottom = std::min<int32_t>(scope.display_.height() - 1, y + 5);
  const int32_t right = scope.display_.width() - 1;
  if (top > bottom) return measuredAdvance ? advance : measure(self, value);
  int32_t pen = x;
  uint32_t cp;
  while (SdBitmapText::nextCodepoint(value, cp)) {
    if (cp == '\r' || cp == '\n') continue;  // Fixed UI calls are single-line.
    const auto* glyph = scope.cache_.glyph(cp);
    const int16_t step = glyph ? glyph->advance : 14;
    const bool missing = !glyph || glyph->length == UINT16_MAX;
    // Match native U8g2 opaque glyph bounds instead of clearing an entire line
    // cell: small chips/footers may have borders immediately above/below ink.
    if (!transparent) {
      const int32_t left = std::max<int32_t>(0, pen + (missing ? 1 : glyph->xOffset));
      const int32_t upper = std::max<int32_t>(top, y + (missing ? -12 : glyph->yOffset));
      const int32_t lower = std::min<int32_t>(bottom,
          y + (missing ? -1 : glyph->yOffset + glyph->height - 1));
      const int32_t end = std::min<int32_t>(right,
          pen + (missing ? 11 : glyph->xOffset + glyph->width - 1));
      if (left <= end && upper <= lower)
        DrawUtils::drawClippedFilledRect(canvas, left, upper, end, lower, bg);
    }
    if (missing) {
      for (int32_t row = std::max<int32_t>(top, y - 12); row <= std::min<int32_t>(bottom, y - 1); ++row) {
        for (int32_t col = 1; col < 12; ++col) {
          if ((row == y - 12 || row == y - 1 || col == 1 || col == 11) &&
              pen + col >= 0 && pen + col <= right) canvas.drawFastHLine(pen + col, row, 1, fg);
        }
      }
    } else {
      const size_t stride = (glyph->width + 7U) / 8U;
      const int32_t penX = pen + glyph->xOffset;
      for (int32_t row = 0; row < glyph->height; ++row) {
        const int32_t py = y + glyph->yOffset + row;
        if (py < top || py > bottom) continue;
        SdBitmapText::forEachInkRun(glyph->bitmap + static_cast<size_t>(row) * stride,
                                    glyph->width, [&](int32_t x1, int32_t x2) {
          int32_t start = penX + x1;
          int32_t end = penX + x2;
          if (start < 0) start = 0;
          if (end > right) end = right;
          if (start <= end) canvas.drawFastHLine(start, py, end - start + 1, fg);
        });
      }
    }
    pen += step;
  }
  if (measuredAdvance) return advance;
  return static_cast<int16_t>(std::min<int32_t>(32767, pen - x));
}
}
