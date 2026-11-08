#include "GamesPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../IconBitmap.h"
#include "../assets/games/icons8-tapthewoodenfish.h"

namespace {
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

const char* woodenFishTextsZh(uint8_t idx) {
  static const char* const kTexts[] = {"功德+1", "前途似锦+1", "静心+1", "智力+1",
                                       "精神+1", "小钱包+1", "头发+1"};
  return kTexts[idx % (sizeof(kTexts) / sizeof(kTexts[0]))];
}

}  // namespace

bool GamesPage::isWoodenFishSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kWoodenFishItemIndex;
}

bool GamesPage::isAirplaneWarSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kAirplaneWarItemIndex;
}

bool GamesPage::isRacingSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kRacingItemIndex;
}

bool GamesPage::is2048Selection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == k2048ItemIndex;
}

bool GamesPage::isHypnosisSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kHypnosisItemIndex;
}

void GamesPage::handle2048Left(uint32_t nowMs) {
  game2048_.handleLeft(nowMs);
}

void GamesPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus) {
  if (isAirplaneWarSelection(homeFocus, sectionFocus)) {
    airplaneWar_.enter();
  } else if (isRacingSelection(homeFocus, sectionFocus)) {
    racing_.enter();
    (void)racingSteering_.start(millis());
  } else if (is2048Selection(homeFocus, sectionFocus)) {
    game2048_.enter();
  } else if (isHypnosisSelection(homeFocus, sectionFocus)) {
    hypnosis_.enter(millis());
  }
}

bool GamesPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                                  bool okPressed, bool okChanged, uint32_t nowMs) {
  if (isAirplaneWarSelection(homeFocus, sectionFocus)) {
    if (okEdge) {
      airplaneWar_.handleOk();
      return true;
    }
    return false;
  }
  if (isRacingSelection(homeFocus, sectionFocus)) {
    if (okEdge) {
      racing_.handleOk();
      if (racing_.paused()) racingSteering_.stop();
      else (void)racingSteering_.start(nowMs);
      return true;
    }
    return false;
  }
  if (is2048Selection(homeFocus, sectionFocus)) {
    if (okEdge) {
      game2048_.handleOk();
      return true;
    }
    return false;
  }
  if (isHypnosisSelection(homeFocus, sectionFocus)) {
    if (okEdge) {
      hypnosis_.replay(nowMs);
      return true;
    }
    return false;
  }
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
  if (isAirplaneWarSelection(homeFocus, sectionFocus)) {
    airplaneWar_.exit();
  } else if (isRacingSelection(homeFocus, sectionFocus)) {
    racingSteering_.stop();
    racing_.exit();
  } else if (is2048Selection(homeFocus, sectionFocus)) {
    game2048_.exit();
  } else if (isHypnosisSelection(homeFocus, sectionFocus)) {
    hypnosis_.exit();
  } else if (isWoodenFishSelection(homeFocus, sectionFocus)) {
    iconPressed_ = false;
  }
  return false;
}

void GamesPage::tick(uint32_t nowMs, bool leftPressed, bool rightPressed, bool upPressed,
                     bool downPressed) {
  airplaneWar_.tick(nowMs, upPressed, downPressed);
  racingSteering_.tick(nowMs);
  racing_.tick(nowMs, leftPressed, rightPressed, racingSteering_.steering());
  game2048_.tick(nowMs, leftPressed, rightPressed, upPressed, downPressed);
  hypnosis_.tick(nowMs);
}

bool GamesPage::consumeChanged() {
  const bool airplaneChanged = airplaneWar_.consumeChanged();
  const bool racingChanged = racing_.consumeChanged();
  const bool game2048Changed = game2048_.consumeChanged();
  const bool hypnosisChanged = hypnosis_.consumeChanged();
  return airplaneChanged || racingChanged || game2048Changed || hypnosisChanged;
}

bool GamesPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus) const {
  if (isAirplaneWarSelection(homeFocus, sectionFocus)) return airplaneWar_.running();
  if (isRacingSelection(homeFocus, sectionFocus)) return racing_.running();
  if (is2048Selection(homeFocus, sectionFocus)) return game2048_.needsAnimationFrame();
  if (isHypnosisSelection(homeFocus, sectionFocus)) return hypnosis_.needsAnimationFrame();
  return false;
}

bool GamesPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                             DisplayMonoTft& display) const {
  if (isAirplaneWarSelection(homeFocus, sectionFocus)) {
    airplaneWar_.render(display, yOffset);
    return true;
  }
  if (isRacingSelection(homeFocus, sectionFocus)) {
    racing_.render(display, yOffset);
    return true;
  }
  if (is2048Selection(homeFocus, sectionFocus)) {
    game2048_.render(display, yOffset);
    return true;
  }
  if (isHypnosisSelection(homeFocus, sectionFocus)) {
    hypnosis_.render(display, yOffset, millis());
    return true;
  }
  if (!isWoodenFishSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  DetailHeader::render(display, "敲木鱼", yOffset);

  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char countLine[32];
  snprintf(countLine, sizeof(countLine), "已敲：%lu",
           static_cast<unsigned long>(knockCount_));
  text.drawUTF8(kCountX, static_cast<int16_t>(yOffset + kCountBaselineY), countLine);

  if (hasMessage_) {
    const char* message = woodenFishTextsZh(lastMessageIndex_);
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
