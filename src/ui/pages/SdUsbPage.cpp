#include "SdUsbPage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../IconBitmap.h"

namespace {
constexpr int16_t kMargin = 16;
constexpr int16_t kIconX = 32;
constexpr int16_t kIconY = 43;
constexpr int16_t kIconSize = 88;
constexpr int16_t kTextX = 152;
constexpr int16_t kStatusY = 44;
constexpr int16_t kStatusHeight = 28;
constexpr int16_t kStatusPadding = 10;
constexpr int16_t kFooterHeight = 28;
}

void SdUsbPage::render(DisplayMonoTft& display, int16_t yOffset,
                        const IconBitmap::Anim& icon, bool enabled) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  DetailHeader::render(display, "SD卡转USB", yOffset);

  // Use the complete final frame; this static page requests no animation ticks.
  const uint16_t frame = icon.frameCount == 0U ? 0U : icon.frameCount - 1U;
  IconBitmap::drawFrame(canvas, icon, frame, kIconX, kIconY + yOffset,
                        kIconSize, kIconSize, false,
                        DetailHeader::kHeight + yOffset, height - 1);

  const char* status = enabled ? "USB 读卡已开启" : "USB 读卡模式";
  const int16_t statusWidth = text.getUTF8Width(status) + 2 * kStatusPadding;
  const int16_t statusTop = kStatusY + yOffset;
  DrawUtils::drawClippedPseudoRoundFilledRect(canvas, kTextX, statusTop,
                                              kTextX + statusWidth - 1,
                                              statusTop + kStatusHeight - 1,
                                              ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(kTextX + kStatusPadding, statusTop + 19, status);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(kTextX, yOffset + 102, "连接电脑，访问SD卡");
  text.drawUTF8(kTextX, yOffset + 126, "支持读取与写入");

  const int16_t footerTop = yOffset + height - kFooterHeight;
  canvas.drawFastHLine(kMargin, footerTop, width - 2 * kMargin, ST7305_COLOR_BLACK);
  const int16_t footerBaseline = yOffset + height - 5;
  text.drawUTF8(kMargin, footerBaseline, "Left 返回");
  const char* hint = "返回前请先安全弹出";
  text.drawUTF8(width - kMargin - text.getUTF8Width(hint), footerBaseline, hint);
}
