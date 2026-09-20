#include "HomePage.h"

#include <cstdio>

#include "../IconBitmap.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"
#include "../Segment7Font.h"
#include "../ThemeMono.h"
#include "../TextUtils.h"
#include "../assets/games/icons8_games.h"
#include "../assets/main_menu/icons8-book.h"
#include "../assets/main_menu/icons8-clock.h"
#include "../assets/main_menu/icons8-itunes.h"
#include "../assets/main_menu/icons8-settings.h"
#include "../assets/main_menu/icons8-wifi.h"
#include "../assets/ui/uncalibrated_bread.h"
#include "../assets/ui/UI_background.h"

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

const IconBitmap::Anim kMenuIcons[6] = {
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
    {reinterpret_cast<const uint8_t*>(&icons8_games_frames[0][0]), ICONS8_GAMES_FRAME_BYTES,
     ICONS8_GAMES_FRAME_WIDTH, ICONS8_GAMES_FRAME_HEIGHT, ICONS8_GAMES_FRAME_DELAY,
     ICONS8_GAMES_FRAME_COUNT},
};

const IconBitmap::Anim kHomeBackground = {
    reinterpret_cast<const uint8_t*>(&ui_background_frames[0][0]),
    UI_BACKGROUND_FRAME_BYTES,
    UI_BACKGROUND_FRAME_WIDTH,
    UI_BACKGROUND_FRAME_HEIGHT,
    UI_BACKGROUND_FRAME_DELAY,
    UI_BACKGROUND_FRAME_COUNT};

const IconBitmap::Anim kUncalibratedBread = {
    reinterpret_cast<const uint8_t*>(&uncalibrated_bread_frames[0][0]),
    UNCALIBRATED_BREAD_FRAME_BYTES,
    UNCALIBRATED_BREAD_FRAME_WIDTH,
    UNCALIBRATED_BREAD_FRAME_HEIGHT,
    UNCALIBRATED_BREAD_FRAME_DELAY,
    UNCALIBRATED_BREAD_FRAME_COUNT};

constexpr int16_t kUncalibratedBreadScale = 2;

const char* const kMenuNamesZh[6] = {"设置", "音乐", "阅读", "专注时钟", "无线功能", "游戏"};

void drawHomeTimePreview(ST7305_2p9_BW_DisplayDriver& canvas, uint32_t nowMs, int16_t xOffset,
                         const HomePage::ClockData& clockData) {
  const int16_t boxX1 = static_cast<int16_t>(kTimeCardStyle.rect.x1 + xOffset);
  const int16_t boxY1 = kTimeCardStyle.rect.y1;
  const int16_t boxX2 = static_cast<int16_t>(kTimeCardStyle.rect.x2 + xOffset);
  const int16_t boxY2 = kTimeCardStyle.rect.y2;
  DrawUtils::drawClippedRect(canvas, boxX1, boxY1, boxX2, boxY2, ST7305_COLOR_BLACK);
  DrawUtils::drawClippedFilledRect(canvas, static_cast<int16_t>(boxX1 + 1),
                                   static_cast<int16_t>(boxY1 + 1),
                                   static_cast<int16_t>(boxX2 - 1),
                                   static_cast<int16_t>(boxY2 - 1), ST7305_COLOR_BLACK);

  if (!clockData.valid) {
    if (boxX1 >= 0 && boxY1 >= 0 && boxX2 < canvas.getDisplayWidth() &&
        boxY2 < canvas.getDisplayHeight()) {
      const int16_t imageW = static_cast<int16_t>(UNCALIBRATED_BREAD_FRAME_WIDTH *
                                                   kUncalibratedBreadScale);
      const int16_t imageH = static_cast<int16_t>(UNCALIBRATED_BREAD_FRAME_HEIGHT *
                                                   kUncalibratedBreadScale);
      const int16_t breadX = static_cast<int16_t>(boxX1 + (boxX2 - boxX1 + 1 - imageW) / 2);
      const int16_t breadY = static_cast<int16_t>(boxY1 + (boxY2 - boxY1 + 1 - imageH) / 2);
      const uint16_t frame = IconBitmap::frameAt(kUncalibratedBread, nowMs);
      IconBitmap::drawFrame(canvas, kUncalibratedBread, frame, breadX, breadY, imageW,
                            imageH, true, boxY1, boxY2);
    }
    return;
  }

  const uint8_t hour = clockData.hour;
  const uint8_t minute = clockData.minute;

  char hhmm[6];
  if (clockData.valid) {
    snprintf(hhmm, sizeof(hhmm), "%02u:%02u", static_cast<unsigned>(hour),
             static_cast<unsigned>(minute));
  } else {
    snprintf(hhmm, sizeof(hhmm), "--:--");
  }

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
                         uint32_t nowMs, int16_t xOffset,
                         const HomePage::ClockData& clockData) {
  static const char* const kWeekdayAbbr[7] = {"SUN", "MON", "TUE", "WED",
                                               "THU", "FRI", "SAT"};
  static const char* const kMonthAbbr[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

  const int16_t boxX1 = static_cast<int16_t>(kDateCardStyle.rect.x1 + xOffset);
  const int16_t boxY1 = kDateCardStyle.rect.y1;
  const int16_t boxX2 = static_cast<int16_t>(kDateCardStyle.rect.x2 + xOffset);
  const int16_t boxY2 = kDateCardStyle.rect.y2;

  DrawUtils::drawClippedRect(canvas, boxX1, boxY1, boxX2, boxY2, ST7305_COLOR_BLACK);
  DrawUtils::drawClippedFilledRect(canvas, static_cast<int16_t>(boxX1 + 1),
                                   static_cast<int16_t>(boxY1 + 1),
                                   static_cast<int16_t>(boxX2 - 1),
                                   static_cast<int16_t>(boxY2 - 1), ST7305_COLOR_BLACK);
  DrawUtils::drawClippedPseudoRoundOutline(
      canvas, static_cast<int16_t>(boxX1 + kDateCardStyle.outerInset),
      static_cast<int16_t>(boxY1 + kDateCardStyle.outerInset),
      static_cast<int16_t>(boxX2 - kDateCardStyle.outerInset),
      static_cast<int16_t>(boxY2 - kDateCardStyle.outerInset), ST7305_COLOR_WHITE,
      ST7305_COLOR_BLACK);

  uint16_t year = 0;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t weekday = 0;
  if (clockData.valid) {
    year = clockData.year;
    month = clockData.month;
    day = clockData.day;
    weekday = clockData.weekday;
  }

  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setFont(u8g2_font_6x12_mf);

  if (!clockData.valid) {
    text.setFont(chinese_font_all);
    const char* placeholder = "时间需校准";
    const int16_t textX = TextUtils::centeredTextXInBox(
        text, placeholder, boxX1, static_cast<int16_t>(boxX2 - boxX1 + 1));
    const int16_t textY = static_cast<int16_t>(boxY1 + ((boxY2 - boxY1 + 1) / 2) + 5);
    text.drawUTF8(textX, textY, placeholder);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.setForegroundColor(ST7305_COLOR_BLACK);
    text.setFontMode(1);
    return;
  }

  const int16_t chipTop = static_cast<int16_t>(boxY1 + kDateCardStyle.chipTopOffset);
  const int16_t chipBottom = static_cast<int16_t>(chipTop + kDateCardStyle.chipHeight);

  const int16_t weekChipX1 = static_cast<int16_t>(boxX1 + kDateCardStyle.weekChipLeftInset);
  const int16_t weekChipX2 = static_cast<int16_t>(weekChipX1 + kDateCardStyle.weekChipWidth);
  DrawUtils::drawClippedPseudoRoundOutline(canvas, weekChipX1, chipTop, weekChipX2, chipBottom,
                                ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(weekChipX1 + kDateCardStyle.chipTextInsetX),
                static_cast<int16_t>(chipBottom - kDateCardStyle.chipTextBaselineInset),
                weekday < 7U ? kWeekdayAbbr[weekday] : "---");

  const int16_t monthChipX2 =
      static_cast<int16_t>(boxX2 - kDateCardStyle.monthChipRightInset);
  const int16_t monthChipX1 =
      static_cast<int16_t>(monthChipX2 - kDateCardStyle.monthChipWidth);
  DrawUtils::drawClippedPseudoRoundOutline(canvas, monthChipX1, chipTop, monthChipX2, chipBottom,
                                ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(monthChipX1 + kDateCardStyle.chipTextInsetX),
                static_cast<int16_t>(chipBottom - kDateCardStyle.chipTextBaselineInset),
                (month >= 1U && month <= 12U) ? kMonthAbbr[month - 1U] : "---");

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
  lastBackgroundFrame_ = 0;
  lastUncalibratedFrame_ = 0;
  lastInteractionMs_ = animStartMs_;
  animationTimeMs_ = animStartMs_;
  return true;
}

bool HomePage::handleInput(bool upEdge, bool downEdge, bool okEdge, uint32_t nowMs) {
  if (okEdge && slideState_ == SlideState::Idle) {
    lastInteractionMs_ = nowMs;
    animationTimeMs_ = nowMs;
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

  lastInteractionMs_ = nowMs;
  animationTimeMs_ = nowMs;

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
  if (slideState_ == SlideState::Sliding || isInteractiveAnimationWindow(nowMs)) {
    animationTimeMs_ = nowMs;
  }

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
  return AnimMath::easeOutCubicInt16(animFromOffsetY_, animToOffsetY_, clamped);
}

bool HomePage::hasUncalibratedBreadTick(uint32_t nowMs) const {
  if (clockData_.valid) {
    return false;
  }
  return IconBitmap::frameAt(kUncalibratedBread, nowMs) != lastUncalibratedFrame_;
}

bool HomePage::hasMenuAnimationTick(uint32_t nowMs) const {
  if (!isAnimationActive(nowMs)) {
    return false;
  }

  const uint32_t animNowMs = animationRenderTime(nowMs);
  const IconBitmap::Anim& anim = kMenuIcons[focusIndex_];
  if (anim.frameCount == 0 || anim.frameDelayMs == 0) {
    const uint16_t bgFrame = IconBitmap::frameAt(kHomeBackground, animNowMs);
    return bgFrame != lastBackgroundFrame_;
  }

  const uint16_t frame = IconBitmap::frameAt(anim, animNowMs);
  if (frame != lastFocusFrame_) {
    return true;
  }

  const uint16_t bgFrame = IconBitmap::frameAt(kHomeBackground, animNowMs);
  return bgFrame != lastBackgroundFrame_;
}

bool HomePage::hasAnimationTick(uint32_t nowMs) const {
  // 轮盘布局刚变化（滑动结束那一帧）时，局部刷新无法更新焦点框与标签，
  // 必须强制走一次整屏重绘把布局落到屏幕上。
  if (!menuLayoutMatchesLastFullRender(nowMs)) {
    return true;
  }
  return hasUncalibratedBreadTick(nowMs) || hasMenuAnimationTick(nowMs);
}

bool HomePage::isBreadOnlyAnimationTick(uint32_t nowMs) const {
  if (slideState_ != SlideState::Idle) {
    return false;
  }
  if (!menuLayoutMatchesLastFullRender(nowMs)) {
    return false;
  }
  if (!hasUncalibratedBreadTick(nowMs)) {
    return false;
  }
  return !hasMenuAnimationTick(nowMs);
}

HomePage::Rect HomePage::timeCardBounds() const {
  return {kTimeCardStyle.rect.x1, kTimeCardStyle.rect.y1, kTimeCardStyle.rect.x2,
          kTimeCardStyle.rect.y2};
}

void HomePage::renderTimeCardOnly(DisplayMonoTft& display, uint32_t nowMs) {
  drawHomeTimePreview(display.canvas(), nowMs, 0, clockData_);
  lastUncalibratedFrame_ = IconBitmap::frameAt(kUncalibratedBread, nowMs);
  // 卡片已在帧缓冲里被改写，静态图层需要重建。
  staticLayerValid_ = false;
  frameUsedStaticLayer_ = false;
}

bool HomePage::isMenuIconsOnlyAnimationTick(uint32_t nowMs) const {
  if (slideState_ != SlideState::Idle) {
    return false;
  }
  if (!menuLayoutMatchesLastFullRender(nowMs)) {
    return false;
  }
  // 小面包帧同时到期时退回整屏重绘，避免两条局部路径互相覆盖。
  if (hasUncalibratedBreadTick(nowMs)) {
    return false;
  }
  return hasMenuAnimationTick(nowMs);
}

bool HomePage::menuLayoutMatchesLastFullRender(uint32_t nowMs) const {
  return focusIndex_ == lastRenderedFocusIndex_ &&
         currentMenuOffset(nowMs) == lastRenderedMenuOffsetY_;
}

HomePage::Rect HomePage::menuIconBounds(const DisplayMonoTft& display) const {
  const int16_t iconX = static_cast<int16_t>(
      display.width() - kWheelFrameWidth - kWheelFrameRightMargin + kWheelItemInsetX);
  return {iconX, kMenuClipTop, static_cast<int16_t>(iconX + ThemeMono::kIconFocusSize - 1),
          kMenuClipBottom};
}

HomePage::Rect HomePage::menuColumnBounds(const DisplayMonoTft& display) const {
  const int16_t frameBaseX =
      static_cast<int16_t>(display.width() - kWheelFrameWidth - kWheelFrameRightMargin);
  return {frameBaseX, kMenuClipTop, static_cast<int16_t>(display.width() - 1), kMenuClipBottom};
}

bool HomePage::frameUsedStaticLayer() const { return frameUsedStaticLayer_; }

void HomePage::renderMenuIconsOnly(DisplayMonoTft& display, uint32_t nowMs) {
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t frameBaseX =
      static_cast<int16_t>(width - kWheelFrameWidth - kWheelFrameRightMargin);
  drawMenuWheel(display, frameBaseX, static_cast<int16_t>(height / 2), 0, 0, 0, 0, nowMs, true);
}

bool HomePage::isAnimationActive(uint32_t nowMs) const {
  return slideState_ == SlideState::Sliding || isInteractiveAnimationWindow(nowMs);
}

uint8_t HomePage::focusIndex() const { return focusIndex_; }

const char* HomePage::menuLabel(uint8_t idx) const {
  const uint8_t safe = static_cast<uint8_t>(idx % kMenuCount);
  return kMenuNamesZh[safe];
}

const char* HomePage::focusName() const { return menuLabel(focusIndex_); }

void HomePage::setClockData(const ClockData& data) {
  const bool displayChanged =
      data.valid != clockData_.valid || data.hour != clockData_.hour ||
      data.minute != clockData_.minute || data.day != clockData_.day ||
      data.weekday != clockData_.weekday || data.month != clockData_.month ||
      data.year != clockData_.year;
  clockData_ = data;
  if (displayChanged) {
    // 卡片内容变化，静态图层需要重建。
    staticLayerValid_ = false;
  }
}

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
  lastRenderedFocusIndex_ = focusIndex_;
  lastRenderedMenuOffsetY_ = menuOffsetY;
  const int16_t centerY = static_cast<int16_t>(height / 2);
  const bool transitionVisualActive =
      (backgroundOffsetX != menuBaseOffsetX) || (menuUpExtraOffsetX != 0) ||
      (menuFocusExtraOffsetX != 0) || (menuDownExtraOffsetX != 0);
  const uint32_t animNowMs = (slideState_ == SlideState::Sliding || transitionVisualActive)
                                 ? nowMs
                                 : animationRenderTime(nowMs);

  const uint16_t bgFrame = IconBitmap::frameAt(kHomeBackground, animNowMs);
  lastBackgroundFrame_ = bgFrame;

  // 静止直出时整帧回填静态图层（背景 + 时间/日期卡片），避免每帧重画整屏背景；
  // 有横向偏移（过渡动画）时背景位置在变，必须实时绘制。
  // 时间未校准时分面包每 220ms 换帧、卡片一直在变，也不能复用缓存。
  const bool canUseStaticLayer =
      (backgroundOffsetX == 0) && (menuBaseOffsetX == 0) && clockData_.valid;
  if (canUseStaticLayer && staticLayerValid_) {
    display.restoreFrame(staticLayer_);
    frameUsedStaticLayer_ = true;
  } else {
    IconBitmap::drawFrame(canvas, kHomeBackground, bgFrame, backgroundOffsetX, 0, width,
                          height, false, 0, static_cast<int16_t>(height - 1));
    drawHomeTimePreview(canvas, nowMs, backgroundOffsetX, clockData_);
    if (!clockData_.valid && backgroundOffsetX == 0) {
      lastUncalibratedFrame_ = IconBitmap::frameAt(kUncalibratedBread, nowMs);
    }
    drawHomeDatePreview(canvas, text, nowMs, backgroundOffsetX, clockData_);
    frameUsedStaticLayer_ = false;
    if (canUseStaticLayer && display.frameBufferBytes() == kStaticLayerBytes) {
      display.captureFrame(staticLayer_);
      staticLayerValid_ = true;
    }
  }

  drawMenuWheel(display, frameBaseX, centerY, menuOffsetY, menuUpExtraOffsetX,
                menuFocusExtraOffsetX, menuDownExtraOffsetX, animNowMs, false);
}

void HomePage::drawMenuWheel(DisplayMonoTft& display, int16_t frameBaseX, int16_t centerY,
                             int16_t menuOffsetY, int16_t menuUpExtraOffsetX,
                             int16_t menuFocusExtraOffsetX, int16_t menuDownExtraOffsetX,
                             uint32_t animNowMs, bool iconsOnly) {
  auto& canvas = display.canvas();
  auto& text = display.text();

  if (!iconsOnly) {
    text.setFont(chinese_font_all);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.setFontMode(1);
    text.setForegroundColor(ST7305_COLOR_BLACK);
  }

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

    if (!iconsOnly && isFocus) {
      int16_t focusTop = static_cast<int16_t>(rowCenterY - (iconSize / 2 + kWheelFramePaddingY));
      int16_t focusBottom =
          static_cast<int16_t>(rowCenterY + (iconSize / 2 + kWheelFramePaddingY));
      if (focusTop < kMenuClipTop) {
        focusTop = kMenuClipTop;
      }
      if (focusBottom > kMenuClipBottom) {
        focusBottom = kMenuClipBottom;
      }
      canvas.drawRectangle(frameX, focusTop, frameX + kWheelFrameWidth, focusBottom,
                           ST7305_COLOR_BLACK);
    }

    const uint16_t frame = IconBitmap::frameAt(anim, animNowMs);
    if (isFocus) {
      lastFocusFrame_ = frame;
    }

    const int16_t iconX = static_cast<int16_t>(frameX + kWheelItemInsetX);
    IconBitmap::drawFrame(canvas, anim, frame, iconX, rowTop, iconSize, iconSize, false,
                          kMenuClipTop, kMenuClipBottom);

    if (iconsOnly) {
      continue;
    }

    const char* label = menuLabel(i);
    const int16_t textBlockX = static_cast<int16_t>(iconX + iconSize + kWheelTextGap);
    const int16_t textBlockW =
        static_cast<int16_t>(frameX + kWheelFrameWidth - textBlockX - kWheelItemInsetX);
    int16_t labelX = TextUtils::centeredTextXInBox(text, label, textBlockX, textBlockW);
    if (labelX < textBlockX) {
      labelX = textBlockX;
    }
    const int16_t labelY = static_cast<int16_t>(rowCenterY + 6);
    if (labelY >= kMenuClipTop + kMenuLabelHeight && labelY <= kMenuClipBottom) {
      text.drawUTF8(labelX, labelY, label);
    }
  }
}

bool HomePage::isInteractiveAnimationWindow(uint32_t nowMs) const {
  return (nowMs - lastInteractionMs_) < kIdleAnimationTimeoutMs;
}

uint32_t HomePage::animationRenderTime(uint32_t nowMs) const {
  return isAnimationActive(nowMs) ? nowMs : animationTimeMs_;
}


