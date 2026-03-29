#include "HomePage.h"

#include <cstdio>

#include "../DateUtils.h"
#include "../IconBitmap.h"
#include "../Segment7Font.h"
#include "../ThemeMono.h"
#include "../assets/UI_background.h"
#include "../assets/icons8-book.h"
#include "../assets/icons8-clock.h"
#include "../assets/icons8-itunes.h"
#include "../assets/icons8-settings.h"
#include "../assets/icons8-wifi.h"

namespace {
constexpr int16_t kWheelFrameWidth = 190;
constexpr int16_t kWheelFrameRightMargin = 8;
constexpr int16_t kWheelItemSpacing = 56;
constexpr int16_t kWheelItemInsetX = 10;
constexpr int16_t kWheelTextGap = 14;
constexpr int16_t kWheelFramePaddingY = 8;
constexpr int16_t kMenuClipTop = 10;
constexpr int16_t kMenuClipBottom = 160;
constexpr int16_t kMenuLabelHeight = 13;
constexpr uint16_t kEpochYear = 2026;
constexpr uint8_t kEpochWeekday = 4;  // 2026-01-01 is Thursday (SUN=0).

struct CardRect {
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
};

struct TimeCardStyle {
  CardRect rect;
  Segment7Font::Style digitStyle;
  int16_t minTextInset;
};

struct DateCardStyle {
  CardRect rect;
  int16_t outerInset;
  int16_t chipTopOffset;
  int16_t chipHeight;
  int16_t weekChipLeftInset;
  int16_t weekChipWidth;
  int16_t monthChipRightInset;
  int16_t monthChipWidth;
  int16_t chipTextInsetX;
  int16_t chipTextBaselineInset;
  Segment7Font::Style dayStyle;
  int16_t dayTopOffset;
  int16_t yearGapFromDay;
  int16_t yearRightInset;
  int16_t yearBaselineOffset;
};

constexpr TimeCardStyle kTimeCardStyle = {
    {15, 15, 140, 60},
    {24, 40, 4, 2, 6, ST7305_COLOR_WHITE},
    2};

constexpr DateCardStyle kDateCardStyle = {
    {15, 85, 140, 140},
    3,
    8,
    15,
    8,
    44,
    8,
    40,
    8,
    4,
    {14, 24, 3, 2, 4, ST7305_COLOR_WHITE},
    28,
    4,
    6,
    -2};

const IconBitmap::Anim kMenuIcons[5] = {
    {reinterpret_cast<const uint8_t*>(&setting_frames[0][0]), SETTING_FRAME_BYTES,
     SETTING_FRAME_WIDTH, SETTING_FRAME_HEIGHT, SETTING_FRAME_DELAY, SETTING_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&itunes_frames[0][0]), ITUNES_FRAME_BYTES,
     ITUNES_FRAME_WIDTH, ITUNES_FRAME_HEIGHT, ITUNES_FRAME_DELAY, ITUNES_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&book_frames[0][0]), BOOK_FRAME_BYTES,
     BOOK_FRAME_WIDTH, BOOK_FRAME_HEIGHT, BOOK_FRAME_DELAY, BOOK_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&clock_frames[0][0]), CLOCK_FRAME_BYTES,
     CLOCK_FRAME_WIDTH, CLOCK_FRAME_HEIGHT, CLOCK_FRAME_DELAY, CLOCK_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&wifi_frames[0][0]), WIFI_FRAME_BYTES,
     WIFI_FRAME_WIDTH, WIFI_FRAME_HEIGHT, WIFI_FRAME_DELAY, WIFI_FRAME_COUNT},
};

const IconBitmap::Anim kHomeBackground = {
    reinterpret_cast<const uint8_t*>(&ui_background_frames[0][0]),
    UI_BACKGROUND_FRAME_BYTES,
    UI_BACKGROUND_FRAME_WIDTH,
    UI_BACKGROUND_FRAME_HEIGHT,
    UI_BACKGROUND_FRAME_DELAY,
    UI_BACKGROUND_FRAME_COUNT};

const char* const kMenuNamesZh[5] = {"设置", "音乐", "阅读", "时钟", "无线功能"};
const char* const kMenuNamesEn[5] = {"Settings", "Music", "Reader", "Clock", "Wireless"};

bool clipRectToDisplay(ST7305_2p9_BW_DisplayDriver& canvas, int16_t& x1, int16_t& y1,
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

void drawClippedRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, uint16_t color) {
  if (!clipRectToDisplay(canvas, x1, y1, x2, y2)) {
    return;
  }
  canvas.drawRectangle(static_cast<uint>(x1), static_cast<uint>(y1),
                       static_cast<uint>(x2), static_cast<uint>(y2), color);
}

void drawClippedFilledRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, uint16_t color) {
  if (!clipRectToDisplay(canvas, x1, y1, x2, y2)) {
    return;
  }
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1),
                             static_cast<uint>(x2), static_cast<uint>(y2), color);
}

void drawClippedPseudoRoundFilledRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1,
                                      int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  if ((x2 - x1) < 2 || (y2 - y1) < 2) {
    drawClippedFilledRect(canvas, x1, y1, x2, y2, color);
    return;
  }
  drawClippedFilledRect(canvas, static_cast<int16_t>(x1 + 1), y1,
                        static_cast<int16_t>(x2 - 1), y2, color);
  drawClippedFilledRect(canvas, x1, static_cast<int16_t>(y1 + 1), x2,
                        static_cast<int16_t>(y2 - 1), color);
}

void drawClippedPseudoRoundOutline(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1,
                                   int16_t y1, int16_t x2, int16_t y2, uint16_t borderColor,
                                   uint16_t innerColor) {
  drawClippedPseudoRoundFilledRect(canvas, x1, y1, x2, y2, borderColor);
  drawClippedPseudoRoundFilledRect(canvas, static_cast<int16_t>(x1 + 1),
                                   static_cast<int16_t>(y1 + 1),
                                   static_cast<int16_t>(x2 - 1),
                                   static_cast<int16_t>(y2 - 1), innerColor);
}

void drawHomeTimePreview(ST7305_2p9_BW_DisplayDriver& canvas, uint32_t nowMs,
                         int16_t xOffset) {
  const int16_t boxX1 = static_cast<int16_t>(kTimeCardStyle.rect.x1 + xOffset);
  const int16_t boxY1 = kTimeCardStyle.rect.y1;
  const int16_t boxX2 = static_cast<int16_t>(kTimeCardStyle.rect.x2 + xOffset);
  const int16_t boxY2 = kTimeCardStyle.rect.y2;
  drawClippedRect(canvas, boxX1, boxY1, boxX2, boxY2, ST7305_COLOR_BLACK);
  drawClippedFilledRect(canvas, static_cast<int16_t>(boxX1 + 1),
                        static_cast<int16_t>(boxY1 + 1), static_cast<int16_t>(boxX2 - 1),
                        static_cast<int16_t>(boxY2 - 1), ST7305_COLOR_BLACK);

  const uint32_t totalSeconds = nowMs / 1000U;
  const uint8_t hour = static_cast<uint8_t>((totalSeconds / 3600U) % 24U);
  const uint8_t minute = static_cast<uint8_t>((totalSeconds / 60U) % 60U);

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02u:%02u", static_cast<unsigned>(hour),
           static_cast<unsigned>(minute));

  const Segment7Font::Style& style = kTimeCardStyle.digitStyle;

  const int16_t textW = Segment7Font::measureText(hhmm, style);
  const int16_t boxW = static_cast<int16_t>(boxX2 - boxX1 + 1);
  const int16_t boxH = static_cast<int16_t>(boxY2 - boxY1 + 1);
  int16_t textX = static_cast<int16_t>(boxX1 + (boxW - textW) / 2);
  int16_t textY = static_cast<int16_t>(boxY1 + (boxH - style.digitHeight) / 2);
  if (textX < boxX1 + kTimeCardStyle.minTextInset) {
    textX = static_cast<int16_t>(boxX1 + kTimeCardStyle.minTextInset);
  }
  if (textY < boxY1 + kTimeCardStyle.minTextInset) {
    textY = static_cast<int16_t>(boxY1 + kTimeCardStyle.minTextInset);
  }

  Segment7Font::drawText(canvas, textX, textY, hhmm, style);
}

void drawHomeDatePreview(ST7305_2p9_BW_DisplayDriver& canvas, U8G2_FOR_ST73XX& text,
                         uint32_t nowMs, int16_t xOffset) {
  static const char* const kWeekdayAbbr[7] = {"SUN", "MON", "TUE", "WED",
                                               "THU", "FRI", "SAT"};
  static const char* const kMonthAbbr[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

  const int16_t boxX1 = static_cast<int16_t>(kDateCardStyle.rect.x1 + xOffset);
  const int16_t boxY1 = kDateCardStyle.rect.y1;
  const int16_t boxX2 = static_cast<int16_t>(kDateCardStyle.rect.x2 + xOffset);
  const int16_t boxY2 = kDateCardStyle.rect.y2;

  drawClippedRect(canvas, boxX1, boxY1, boxX2, boxY2, ST7305_COLOR_BLACK);
  drawClippedFilledRect(canvas, static_cast<int16_t>(boxX1 + 1),
                        static_cast<int16_t>(boxY1 + 1), static_cast<int16_t>(boxX2 - 1),
                        static_cast<int16_t>(boxY2 - 1), ST7305_COLOR_BLACK);
  drawClippedPseudoRoundOutline(
      canvas, static_cast<int16_t>(boxX1 + kDateCardStyle.outerInset),
      static_cast<int16_t>(boxY1 + kDateCardStyle.outerInset),
      static_cast<int16_t>(boxX2 - kDateCardStyle.outerInset),
      static_cast<int16_t>(boxY2 - kDateCardStyle.outerInset), ST7305_COLOR_WHITE,
      ST7305_COLOR_BLACK);

  const uint32_t totalSeconds = nowMs / 1000U;
  const uint32_t days = totalSeconds / 86400U;
  uint16_t year = 0;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t weekday = 0;
  DateUtils::daysToDate(days, kEpochYear, kEpochWeekday, year, month, day, weekday);

  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setFont(u8g2_font_6x12_mf);

  const int16_t chipTop = static_cast<int16_t>(boxY1 + kDateCardStyle.chipTopOffset);
  const int16_t chipBottom = static_cast<int16_t>(chipTop + kDateCardStyle.chipHeight);

  const int16_t weekChipX1 = static_cast<int16_t>(boxX1 + kDateCardStyle.weekChipLeftInset);
  const int16_t weekChipX2 = static_cast<int16_t>(weekChipX1 + kDateCardStyle.weekChipWidth);
  drawClippedPseudoRoundOutline(canvas, weekChipX1, chipTop, weekChipX2, chipBottom,
                                ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(weekChipX1 + kDateCardStyle.chipTextInsetX),
                static_cast<int16_t>(chipBottom - kDateCardStyle.chipTextBaselineInset),
                kWeekdayAbbr[weekday]);

  const int16_t monthChipX2 =
      static_cast<int16_t>(boxX2 - kDateCardStyle.monthChipRightInset);
  const int16_t monthChipX1 =
      static_cast<int16_t>(monthChipX2 - kDateCardStyle.monthChipWidth);
  drawClippedPseudoRoundOutline(canvas, monthChipX1, chipTop, monthChipX2, chipBottom,
                                ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(monthChipX1 + kDateCardStyle.chipTextInsetX),
                static_cast<int16_t>(chipBottom - kDateCardStyle.chipTextBaselineInset),
                kMonthAbbr[month - 1]);

  char dayText[3];
  snprintf(dayText, sizeof(dayText), "%02u", static_cast<unsigned>(day));
  const Segment7Font::Style& dayStyle = kDateCardStyle.dayStyle;
  const int16_t dayW = Segment7Font::measureText(dayText, dayStyle);
  const int16_t dayX = static_cast<int16_t>(boxX1 + ((boxX2 - boxX1 + 1) - dayW) / 2);
  const int16_t dayY = static_cast<int16_t>(boxY1 + kDateCardStyle.dayTopOffset);
  Segment7Font::drawText(canvas, dayX, dayY, dayText, dayStyle);

  char yearText[8];
  snprintf(yearText, sizeof(yearText), "%04u", static_cast<unsigned>(year));
  text.setFont(u8g2_font_6x12_mf);
  const int16_t yearW = text.getUTF8Width(yearText);
  int16_t yearX = static_cast<int16_t>(dayX + dayW + kDateCardStyle.yearGapFromDay);
  const int16_t yearMaxX =
      static_cast<int16_t>(boxX2 - kDateCardStyle.yearRightInset - yearW);
  if (yearX > yearMaxX) {
    yearX = yearMaxX;
  }
  const int16_t yearY = static_cast<int16_t>(dayY + dayStyle.digitHeight +
                                             kDateCardStyle.yearBaselineOffset);
  text.drawUTF8(yearX, yearY, yearText);

  // Restore default menu text style to avoid leaking date-card text state.
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}
}  // namespace

bool HomePage::begin() {
  focusIndex_ = 0;
  targetIndex_ = 0;
  slideState_ = SlideState::Idle;
  animFromOffsetY_ = 0;
  animToOffsetY_ = 0;
  animStartMs_ = millis();
  lastFocusFrame_ = 0;
  return true;
}

bool HomePage::handleInput(bool upEdge, bool downEdge, bool okEdge, uint32_t nowMs) {
  if (okEdge && slideState_ == SlideState::Idle) {
    return true;
  }

  int8_t direction = 0;
  if (upEdge) {
    direction = -1;
  } else if (downEdge) {
    direction = 1;
  }

  if (direction == 0) {
    return false;
  }

  if (slideState_ == SlideState::Sliding) {
    focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetY_ = 0;
    animToOffsetY_ = 0;
  }

  beginSlide(direction, nowMs);
  return false;
}

void HomePage::beginSlide(int8_t direction, uint32_t nowMs) {
  targetIndex_ = static_cast<uint8_t>((focusIndex_ + kMenuCount + direction) % kMenuCount);
  animFromOffsetY_ = 0;
  animToOffsetY_ = static_cast<int16_t>(-direction * kWheelItemSpacing);
  animStartMs_ = nowMs;
  slideState_ = SlideState::Sliding;
}

void HomePage::update(uint32_t nowMs) {
  if (slideState_ != SlideState::Sliding) {
    return;
  }

  if (nowMs - animStartMs_ >= ThemeMono::kSlideDurationMs) {
    focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetY_ = 0;
    animToOffsetY_ = 0;
  }
}

bool HomePage::isSliding() const { return slideState_ == SlideState::Sliding; }

int16_t HomePage::currentMenuOffset(uint32_t nowMs) const {
  if (slideState_ != SlideState::Sliding) {
    return 0;
  }

  const float t = static_cast<float>(nowMs - animStartMs_) /
                  static_cast<float>(ThemeMono::kSlideDurationMs);
  const float clamped = t > 1.0f ? 1.0f : t;
  return easeOutCubic(animFromOffsetY_, animToOffsetY_, clamped);
}

bool HomePage::hasAnimationTick(uint32_t nowMs) const {
  const IconBitmap::Anim& anim = kMenuIcons[focusIndex_];
  if (anim.frameCount == 0 || anim.frameDelayMs == 0) {
    return false;
  }
  const uint16_t frame = IconBitmap::frameAt(anim, nowMs);
  return frame != lastFocusFrame_;
}

uint8_t HomePage::focusIndex() const { return focusIndex_; }

const char* HomePage::menuLabel(uint8_t idx) const {
  const uint8_t safe = static_cast<uint8_t>(idx % kMenuCount);
  return (language_ == Language::Zh) ? kMenuNamesZh[safe] : kMenuNamesEn[safe];
}

const char* HomePage::focusName() const { return menuLabel(focusIndex_); }

void HomePage::setLanguage(Language language) { language_ = language; }

void HomePage::render(DisplayMonoTft& display, int16_t pageOffsetX, uint32_t nowMs) {
  renderTransition(display, pageOffsetX, pageOffsetX, 0, 0, 0, nowMs);
}

void HomePage::renderTransition(DisplayMonoTft& display, int16_t backgroundOffsetX,
                                int16_t menuBaseOffsetX, int16_t menuUpExtraOffsetX,
                                int16_t menuFocusExtraOffsetX,
                                int16_t menuDownExtraOffsetX, uint32_t nowMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t menuOffsetY = currentMenuOffset(nowMs);
  const int16_t frameBaseX = static_cast<int16_t>(
      menuBaseOffsetX + width - kWheelFrameWidth - kWheelFrameRightMargin);
  const int16_t centerY = static_cast<int16_t>(height / 2);

  const uint16_t bgFrame = IconBitmap::frameAt(kHomeBackground, nowMs);
  IconBitmap::drawFrame(canvas, kHomeBackground, bgFrame, backgroundOffsetX, 0, width,
                        height, false, 0, static_cast<int16_t>(height - 1));
  drawHomeTimePreview(canvas, nowMs, backgroundOffsetX);
  drawHomeDatePreview(canvas, text, nowMs, backgroundOffsetX);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  for (uint8_t i = 0; i < kMenuCount; ++i) {
    int8_t delta = static_cast<int8_t>(i) - static_cast<int8_t>(focusIndex_);
    if (delta > 2) {
      delta -= kMenuCount;
    } else if (delta < -2) {
      delta += kMenuCount;
    }

    const bool isFocus = (delta == 0);
    const IconBitmap::Anim& anim = kMenuIcons[i];

    int16_t iconSize = isFocus ? static_cast<int16_t>(ThemeMono::kIconFocusSize)
                               : static_cast<int16_t>(ThemeMono::kIconSideSize);
    const int16_t rowCenterY =
        static_cast<int16_t>(centerY + delta * kWheelItemSpacing + menuOffsetY);
    const int16_t rowTop = static_cast<int16_t>(rowCenterY - iconSize / 2);
    const int16_t rowBottom = static_cast<int16_t>(rowTop + iconSize - 1);
    if (rowBottom < kMenuClipTop || rowTop > kMenuClipBottom) {
      continue;
    }

    int16_t rowOffsetX = 0;
    if (delta == -1) {
      rowOffsetX = menuUpExtraOffsetX;
    } else if (delta == 0) {
      rowOffsetX = menuFocusExtraOffsetX;
    } else if (delta == 1) {
      rowOffsetX = menuDownExtraOffsetX;
    }

    const int16_t frameX = static_cast<int16_t>(frameBaseX + rowOffsetX);

    if (isFocus) {
      int16_t focusTop = static_cast<int16_t>(rowCenterY - (iconSize / 2 + kWheelFramePaddingY));
      int16_t focusBottom = static_cast<int16_t>(rowCenterY + (iconSize / 2 + kWheelFramePaddingY));
      if (focusTop < kMenuClipTop) {
        focusTop = kMenuClipTop;
      }
      if (focusBottom > kMenuClipBottom) {
        focusBottom = kMenuClipBottom;
      }
      canvas.drawRectangle(frameX, focusTop, frameX + kWheelFrameWidth, focusBottom,
                           ST7305_COLOR_BLACK);
    }

    const uint16_t frame = IconBitmap::frameAt(anim, nowMs);
    if (isFocus) {
      lastFocusFrame_ = frame;
    }

    const int16_t iconX = static_cast<int16_t>(frameX + kWheelItemInsetX);
    const int16_t iconY = rowTop;
    IconBitmap::drawFrame(canvas, anim, frame, iconX, iconY, iconSize, iconSize, false,
                          kMenuClipTop, kMenuClipBottom);

    const char* label = menuLabel(i);
    const int16_t labelW = text.getUTF8Width(label);
    const int16_t textBlockX = static_cast<int16_t>(iconX + iconSize + kWheelTextGap);
    const int16_t textBlockW = static_cast<int16_t>(frameX + kWheelFrameWidth - textBlockX - kWheelItemInsetX);
    int16_t labelX = static_cast<int16_t>(textBlockX + (textBlockW - labelW) / 2);
    if (labelX < textBlockX) {
      labelX = textBlockX;
    }
    const int16_t labelY = static_cast<int16_t>(rowCenterY + 6);
    if (labelY >= kMenuClipTop + kMenuLabelHeight && labelY <= kMenuClipBottom) {
      text.drawUTF8(labelX, labelY, label);
    }
  }
}

int16_t HomePage::easeOutCubic(int16_t from, int16_t to, float t) const {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  const float inv = 1.0f - t;
  const float eased = 1.0f - (inv * inv * inv);
  return static_cast<int16_t>(from + (to - from) * eased);
}


