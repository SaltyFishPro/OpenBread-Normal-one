#include "ComputerStatusPage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../IconBitmap.h"

namespace {
constexpr uint32_t kIntroDurationMs = 1000U;
constexpr int16_t kMargin = 16;
constexpr int16_t kIconX = 32;
constexpr int16_t kIconY = 43;
constexpr int16_t kIconSize = 88;
constexpr int16_t kTextX = 152;
constexpr int16_t kStatusY = 40;
constexpr int16_t kStatusHeight = 28;
constexpr int16_t kStatusPadding = 10;
constexpr int16_t kFooterHeight = 28;
}

void ComputerStatusPage::prepareEnter() {
  entered_ = false;
  lastRenderedFrame_ = 0xFFFFU;
  lastStudioConnected_ = false;
}

void ComputerStatusPage::begin(uint32_t nowMs) {
  prepareEnter();
  enteredMs_ = nowMs;
  entered_ = true;
}

uint16_t ComputerStatusPage::iconFrame(uint32_t nowMs, const IconBitmap::Anim& icon) const {
  if (!entered_ || icon.frameCount == 0U) return 0U;
  const uint32_t elapsed = nowMs - enteredMs_;
  if (elapsed >= kIntroDurationMs) return icon.frameCount - 1U;
  // Play the existing asset once in one second, then retain its final frame.
  return static_cast<uint16_t>(elapsed * icon.frameCount / kIntroDurationMs);
}

bool ComputerStatusPage::needsAnimationFrame(uint32_t nowMs,
                                             const IconBitmap::Anim& icon) const {
  return entered_ && iconFrame(nowMs, icon) != lastRenderedFrame_;
}

bool ComputerStatusPage::needsRedraw(uint32_t nowMs, const IconBitmap::Anim& icon,
                                     bool studioConnected) const {
  return iconFrame(nowMs, icon) != lastRenderedFrame_ ||
         studioConnected != lastStudioConnected_;
}

void ComputerStatusPage::render(DisplayMonoTft& display, int16_t yOffset,
                                 const IconBitmap::Anim& icon, bool studioConnected,
                                 uint32_t nowMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  DetailHeader::render(display, "电脑状态看板", yOffset);

  const uint16_t frame = iconFrame(nowMs, icon);
  IconBitmap::drawFrame(canvas, icon, frame, kIconX, kIconY + yOffset,
                        kIconSize, kIconSize, false,
                        DetailHeader::kHeight + yOffset, height - 1);
  lastRenderedFrame_ = frame;
  lastStudioConnected_ = studioConnected;

  const char* status = studioConnected ? "Studio 已连接" : "等待 Studio 连接";
  const int16_t statusWidth = text.getUTF8Width(status) + 2 * kStatusPadding;
  const int16_t statusTop = kStatusY + yOffset;
  DrawUtils::drawClippedPseudoRoundFilledRect(canvas, kTextX, statusTop,
                                              kTextX + statusWidth - 1,
                                              statusTop + kStatusHeight - 1,
                                              ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(kTextX + kStatusPadding, yOffset + 59, status);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);

  if (studioConnected) {
    text.drawUTF8(kTextX, yOffset + 99, "等待电脑发送画面");
    text.drawUTF8(kTextX, yOffset + 123, "收到画面后自动显示");
  } else {
    text.drawUTF8(kTextX, yOffset + 92, "在电脑上打开");
    text.drawUTF8(kTextX, yOffset + 112, "Normal One Studio");
    text.drawUTF8(kTextX, yOffset + 132, "选择设备并连接");
  }

  const int16_t footerTop = yOffset + height - kFooterHeight;
  canvas.drawFastHLine(kMargin, footerTop, width - 2 * kMargin, ST7305_COLOR_BLACK);
  const int16_t footerBaseline = yOffset + height - 5;
  text.drawUTF8(kMargin, footerBaseline, "USB 画面同步");
  const char* back = "Left 返回";
  text.drawUTF8(width - kMargin - text.getUTF8Width(back), footerBaseline, back);
}
