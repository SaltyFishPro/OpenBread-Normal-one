#include "GamesPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../IconBitmap.h"
#include "../assets/games/icons8-tapthewoodenfish.h"

namespace {
constexpr int16_t kDetailHeaderHeight = 28;
constexpr int16_t kFishSize = 84;
constexpr int16_t kFishPressedSize = static_cast<int16_t>((kFishSize * 70) / 100);
constexpr int16_t kFishBaseY = 76;
constexpr int16_t kCountX = 8;
constexpr int16_t kCountBaselineY = 52;
constexpr int16_t kMessageTopBaselineY = 46;
constexpr int16_t kMessageBottomBaselineY = 68;
constexpr int16_t kMessageMinX = 72;

const IconBitmap::Anim kWoodenFishIcon = {
    reinterpret_cast<const uint8_t*>(&icons8_tapthewoodenfish_frames[0][0]),
    ICONS8_TAPTHEWOODENFISH_FRAME_BYTES,
    ICONS8_TAPTHEWOODENFISH_FRAME_WIDTH,
    ICONS8_TAPTHEWOODENFISH_FRAME_HEIGHT,
    ICONS8_TAPTHEWOODENFISH_FRAME_DELAY,
    ICONS8_TAPTHEWOODENFISH_FRAME_COUNT};

void renderDetailHeader(DisplayMonoTft& display, const char* title, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kDetailHeaderHeight - 1),
                             ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  const int16_t titleW = text.getUTF8Width(title);
  const int16_t titleX = static_cast<int16_t>((width - titleW) / 2);
  text.drawUTF8(titleX, static_cast<int16_t>(yOffset + 22), title);

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

const char* woodenFishTextsZh(uint8_t idx) {
  static const char* const kTexts[] = {"功德+1", "前途似锦+1", "静心+1", "智力+1",
                                       "精神+1", "小钱包+1", "头发+1"};
  return kTexts[idx % (sizeof(kTexts) / sizeof(kTexts[0]))];
}

const char* woodenFishTextsEn(uint8_t idx) {
  static const char* const kTexts[] = {"Merit+1",      "Bright Future+1", "Calm+1",
                                       "Wisdom+1",     "Spirit+1",        "Wallet+1",
                                       "Hair+1"};
  return kTexts[idx % (sizeof(kTexts) / sizeof(kTexts[0]))];
}
}  // namespace

bool GamesPage::isWoodenFishSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kWoodenFishItemIndex;
}

bool GamesPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                                  bool okPressed, bool okChanged, uint32_t nowMs) {
  if (!isWoodenFishSelection(homeFocus, sectionFocus)) {
    return false;
  }

  bool changed = false;
  if (okChanged) {
    iconPressed_ = okPressed;
    changed = true;
  }

  if (okEdge) {
    registerKnock(nowMs);
    changed = true;
  }

  return changed;
}

bool GamesPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isWoodenFishSelection(homeFocus, sectionFocus)) {
    return false;
  }
  iconPressed_ = false;
  return false;
}

bool GamesPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                             DisplayMonoTft& display, HomePage::Language language) const {
  if (!isWoodenFishSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const bool zh = language == HomePage::Language::Zh;

  renderDetailHeader(display, zh ? "敲木鱼" : "Wooden Fish", yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char countLine[32];
  snprintf(countLine, sizeof(countLine), zh ? "已敲：%lu" : "Hits: %lu",
           static_cast<unsigned long>(knockCount_));
  text.drawUTF8(kCountX, static_cast<int16_t>(yOffset + kCountBaselineY), countLine);

  if (hasMessage_) {
    const char* message =
        zh ? woodenFishTextsZh(lastMessageIndex_) : woodenFishTextsEn(lastMessageIndex_);
    const int16_t messageW = text.getUTF8Width(message);
    const int16_t countW = text.getUTF8Width(countLine);
    const int16_t rangeX1 =
        static_cast<int16_t>((kCountX + countW + 10) > kMessageMinX ? (kCountX + countW + 10)
                                                                    : kMessageMinX);
    const int16_t rangeX2 = static_cast<int16_t>(width - messageW - 8);
    const int16_t rangeY1 = static_cast<int16_t>(yOffset + kMessageTopBaselineY);
    const int16_t rangeY2 = static_cast<int16_t>(yOffset + kMessageBottomBaselineY);

    int16_t messageX = rangeX1;
    if (rangeX2 > rangeX1) {
      const uint32_t spanX = static_cast<uint32_t>(rangeX2 - rangeX1 + 1);
      messageX = static_cast<int16_t>(rangeX1 + static_cast<int16_t>(messageSeed_ % spanX));
    }

    int16_t messageY = rangeY1;
    if (rangeY2 > rangeY1) {
      const uint32_t spanY = static_cast<uint32_t>(rangeY2 - rangeY1 + 1);
      messageY =
          static_cast<int16_t>(rangeY1 + static_cast<int16_t>((messageSeed_ >> 8U) % spanY));
    }

    text.drawUTF8(messageX, messageY, message);
  }

  const int16_t iconSize = iconPressed_ ? kFishPressedSize : kFishSize;
  const int16_t fishBaseX = static_cast<int16_t>((width - kFishSize) / 2);
  const int16_t iconX = static_cast<int16_t>(fishBaseX + ((kFishSize - iconSize) / 2));
  const int16_t iconY = static_cast<int16_t>(kFishBaseY + ((kFishSize - iconSize) / 2));
  IconBitmap::drawFrame(canvas, kWoodenFishIcon, 0, iconX, iconY, iconSize, iconSize, false,
                        static_cast<int16_t>(yOffset + 32),
                        static_cast<int16_t>(yOffset + height - 5));

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
  return true;
}

void GamesPage::registerKnock(uint32_t nowMs) {
  ++knockCount_;
  messageSeed_ = messageSeed_ * 1664525UL + 1013904223UL + nowMs + knockCount_;
  lastMessageIndex_ = static_cast<uint8_t>(messageSeed_ % 7U);
  hasMessage_ = true;
}
