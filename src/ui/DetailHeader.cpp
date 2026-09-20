#include "DetailHeader.h"

#include "../bsp/DisplayMonoTft.h"
#include "TextUtils.h"

namespace DetailHeader {

void render(DisplayMonoTft& display, const char* title, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kHeight - 1),
                             ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  const int16_t titleX = TextUtils::centeredTextXInBox(text, title, 0, width);
  text.drawUTF8(titleX, static_cast<int16_t>(yOffset + 22), title);

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

}  // namespace DetailHeader
