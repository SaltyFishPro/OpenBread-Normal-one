#include "HomePage.h"

#include <cstdio>

#include "../IconBitmap.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"
#include "../Segment7Font.h"
#include "../ThemeMono.h"
#include "../TextUtils.h"
#include "../assets/games/icons8_games.h"
#include "../assets/main_menu/alarm.h"
#include "../assets/main_menu/answers_book.h"
#include "../assets/main_menu/icons8-book.h"
#include "../assets/main_menu/calendar.h"
#include "../assets/main_menu/icons8-clock.h"
#include "../assets/main_menu/icons8-itunes.h"
#include "../assets/main_menu/icons8-settings.h"
#include "../assets/main_menu/icons8-wifi.h"
#include "../assets/main_menu/todays_draw.h"
#include "../assets/main_menu/api_stats.h"
#include "../assets/main_menu/computer_status.h"
#include "../assets/main_menu/sd_usb.h"
#include "../assets/ui/battery_levels.h"

namespace {
constexpr int16_t kXmbFocusCenterX = 48;
constexpr int16_t kXmbCategoryStep = 70;
constexpr int16_t kXmbRailTop = 29;
constexpr int16_t kXmbRailRuleY = 87;
constexpr int16_t kXmbItemsTop = 91;
constexpr int16_t kXmbItemBaseline = 112;
constexpr int16_t kXmbItemStep = 25;
constexpr int16_t kXmbIndexX = 24;
constexpr int16_t kXmbTextX = 56;
constexpr int16_t kXmbWaveBaseY = 143;
constexpr int16_t kMenuClipTop = 10;
constexpr int16_t kMenuClipBottom = 167;

const IconBitmap::Anim kBatteryLevels = {
    reinterpret_cast<const uint8_t*>(&battery_levels_frames[0][0]),
    BATTERY_LEVELS_FRAME_BYTES,
    BATTERY_LEVELS_FRAME_WIDTH,
    BATTERY_LEVELS_FRAME_HEIGHT,
    BATTERY_LEVELS_FRAME_DELAY,
    BATTERY_LEVELS_FRAME_COUNT};

const IconBitmap::Anim kMenuIcons[] = {
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
    {reinterpret_cast<const uint8_t*>(&alarm_frames[0][0]), ALARM_FRAME_BYTES,
     ALARM_FRAME_WIDTH, ALARM_FRAME_HEIGHT, ALARM_FRAME_DELAY, ALARM_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&calendar_frames[0][0]), CALENDAR_FRAME_BYTES,
     CALENDAR_FRAME_WIDTH, CALENDAR_FRAME_HEIGHT, CALENDAR_FRAME_DELAY, CALENDAR_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&api_stats_frames[0][0]), API_STATS_FRAME_BYTES,
     API_STATS_FRAME_WIDTH, API_STATS_FRAME_HEIGHT, API_STATS_FRAME_DELAY, API_STATS_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&answers_book_frames[0][0]), ANSWERS_BOOK_FRAME_BYTES,
     ANSWERS_BOOK_FRAME_WIDTH, ANSWERS_BOOK_FRAME_HEIGHT, ANSWERS_BOOK_FRAME_DELAY,
     ANSWERS_BOOK_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&todays_draw_frames[0][0]), TODAYS_DRAW_FRAME_BYTES,
     TODAYS_DRAW_FRAME_WIDTH, TODAYS_DRAW_FRAME_HEIGHT, TODAYS_DRAW_FRAME_DELAY,
     TODAYS_DRAW_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&computer_status_frames[0][0]), COMPUTER_STATUS_FRAME_BYTES,
     COMPUTER_STATUS_FRAME_WIDTH, COMPUTER_STATUS_FRAME_HEIGHT, COMPUTER_STATUS_FRAME_DELAY,
     COMPUTER_STATUS_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&sd_usb_frames[0][0]), SD_USB_FRAME_BYTES,
     SD_USB_FRAME_WIDTH, SD_USB_FRAME_HEIGHT, SD_USB_FRAME_DELAY, SD_USB_FRAME_COUNT},
};

const char* const kMenuNamesZh[] = {"设置", "音乐", "阅读", "专注时钟", "无线功能", "游戏",
                                   "闹钟", "课程表", "Api统计板", "答案之书", "今日抽签",
                                   "电脑状态看板", "SD卡转USB"};
static_assert(sizeof(kMenuIcons) / sizeof(kMenuIcons[0]) ==
              sizeof(kMenuNamesZh) / sizeof(kMenuNamesZh[0]), "Menu icons and labels must match");
static_assert(HomePage::kSdUsbMenuIndex + 1U ==
                  sizeof(kMenuIcons) / sizeof(kMenuIcons[0]),
              "SD USB must match the last main menu slot");

void drawHomeStatus(ST7305_2p9_BW_DisplayDriver& canvas, U8G2_FOR_ST73XX& text,
                    int16_t xOffset, const HomePage::ClockData& clockData,
                    const HomePage::BatteryData& batteryData) {
  const int16_t width = static_cast<int16_t>(canvas.getDisplayWidth());
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);

  char timeText[6];
  snprintf(timeText, sizeof(timeText), "%02u:%02u",
           static_cast<unsigned>(clockData.valid ? clockData.hour : 0U),
           static_cast<unsigned>(clockData.valid ? clockData.minute : 0U));
  if (!clockData.valid) {
    snprintf(timeText, sizeof(timeText), "--:--");
  }
  const int16_t timeX = static_cast<int16_t>(
      xOffset + width / 2 - text.getUTF8Width(timeText) / 2);
  text.drawUTF8(timeX, 17, timeText);

  static const char* const kWeekdayNames[7] = {"周日", "周一", "周二", "周三",
                                                "周四", "周五", "周六"};
  char dateText[32];
  if (clockData.valid && clockData.month >= 1U && clockData.month <= 12U &&
      clockData.weekday < 7U) {
    snprintf(dateText, sizeof(dateText), "%u月%u日 %s",
             static_cast<unsigned>(clockData.month), static_cast<unsigned>(clockData.day),
             kWeekdayNames[clockData.weekday]);
  } else {
    snprintf(dateText, sizeof(dateText), "时间未校准");
  }
  const int16_t dateWidth = text.getUTF8Width(dateText);
  text.drawUTF8(static_cast<int16_t>(xOffset + width - 12 - dateWidth), 17, dateText);

  const uint8_t percent = batteryData.percent > 100U ? 100U : batteryData.percent;
  const uint16_t level = batteryData.valid
                             ? static_cast<uint16_t>(percent >= 75U ? 3U : percent / 25U)
                             : 0U;
  const IconBitmap::Anim& battery = kBatteryLevels;
  const uint16_t frame = battery.frameCount == 0U ? 0U : level % battery.frameCount;
  IconBitmap::drawFrame(canvas, battery, frame, static_cast<int16_t>(xOffset + 12), 7,
                        BATTERY_LEVELS_FRAME_WIDTH, BATTERY_LEVELS_FRAME_HEIGHT, true, 0, 24);
  char batteryText[6];
  snprintf(batteryText, sizeof(batteryText), batteryData.valid ? "%u%%" : "--%%",
           static_cast<unsigned>(percent));
  text.drawUTF8(static_cast<int16_t>(xOffset + 34), 17, batteryText);
}

void drawWave(ST7305_2p9_BW_DisplayDriver& canvas, int16_t xOffset) {
  for (int16_t x = -16; x < 400; x += 2) {
    const int16_t y = static_cast<int16_t>(kXmbWaveBaseY + ((x + 16) * (x + 16)) / 1200);
    if (y >= 0 && y < canvas.getDisplayHeight()) {
      canvas.drawFastHLine(static_cast<int16_t>(x + xOffset), y, 1, ST7305_COLOR_WHITE);
      if (y + 8 < canvas.getDisplayHeight()) {
        canvas.drawFastHLine(static_cast<int16_t>(x + xOffset), static_cast<int16_t>(y + 8), 1,
                             ST7305_COLOR_WHITE);
      }
    }
  }
}

}  // namespace

bool HomePage::begin() {
  static_assert(kMenuCount == sizeof(kMenuIcons) / sizeof(kMenuIcons[0]),
                "Main menu count must match icon resources");
  focusIndex_ = 0;
  targetIndex_ = 0;
  submenuIndex_ = 0;
  submenuCount_ = 0;
  slideState_ = SlideState::Idle;
  animFromOffsetY_ = 0;
  animToOffsetY_ = 0;
  animFromOffsetX_ = 0;
  animToOffsetX_ = 0;
  animStartMs_ = millis();
  lastFocusFrame_ = 0;
  lastInteractionMs_ = animStartMs_;
  animationTimeMs_ = animStartMs_;
  return true;
}

void HomePage::setSubmenuLabels(const char* const* labels, uint8_t count) {
  const uint8_t safeCount = count > 16U ? 16U : count;
  submenuCount_ = safeCount;
  for (uint8_t i = 0; i < safeCount; ++i) submenuLabels_[i] = labels[i];
  for (uint8_t i = safeCount; i < 16U; ++i) submenuLabels_[i] = nullptr;
  if (submenuCount_ == 0U) submenuIndex_ = 0;
  else if (submenuIndex_ >= submenuCount_) submenuIndex_ = static_cast<uint8_t>(submenuCount_ - 1U);
}

bool HomePage::handleInput(bool leftEdge, bool rightEdge, bool upEdge, bool downEdge,
                           bool okEdge, uint32_t nowMs) {
  if (slideState_ == SlideState::SlidingCategory) {
    focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetX_ = 0;
    animToOffsetX_ = 0;
  }
  if (okEdge && slideState_ == SlideState::Idle) {
    lastInteractionMs_ = nowMs;
    animationTimeMs_ = nowMs;
    return true;
  }

  if (leftEdge || rightEdge) {
    const int8_t direction = leftEdge ? -1 : 1;
    lastInteractionMs_ = nowMs;
    animationTimeMs_ = nowMs;
    beginSlide(direction, nowMs);
    return false;
  }

  int8_t submenuDirection = 0;
  if (upEdge) submenuDirection = -1;
  else if (downEdge) submenuDirection = 1;
  if (submenuDirection == 0 || submenuCount_ == 0U) {
    return false;
  }

  lastInteractionMs_ = nowMs;
  animationTimeMs_ = nowMs;
  const int16_t next = static_cast<int16_t>(submenuIndex_) + submenuDirection;
  if (next < 0 || next >= submenuCount_) return false;
  submenuIndex_ = static_cast<uint8_t>(next);
  animFromOffsetY_ = 0;
  animToOffsetY_ = static_cast<int16_t>(-submenuDirection * kXmbItemStep);
  animStartMs_ = nowMs;
  slideState_ = SlideState::SlidingSubmenu;
  return false;
}

void HomePage::beginSlide(int8_t direction, uint32_t nowMs) {
  targetIndex_ = static_cast<uint8_t>((focusIndex_ + kMenuCount + direction) % kMenuCount);
  animFromOffsetX_ = 0;
  animToOffsetX_ = static_cast<int16_t>(-direction * kXmbCategoryStep);
  animFromOffsetY_ = 0;
  animToOffsetY_ = 0;
  submenuIndex_ = 0;
  animStartMs_ = nowMs;
  slideState_ = SlideState::SlidingCategory;
}

void HomePage::update(uint32_t nowMs) {
  if (slideState_ != SlideState::Idle || isInteractiveAnimationWindow(nowMs)) {
    animationTimeMs_ = nowMs;
  }

  if (slideState_ == SlideState::Idle) {
    return;
  }

  if (nowMs - animStartMs_ >= ThemeMono::kSlideDurationMs) {
    if (slideState_ == SlideState::SlidingCategory) focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetX_ = 0;
    animToOffsetX_ = 0;
    animFromOffsetY_ = 0;
    animToOffsetY_ = 0;
  }
}

bool HomePage::isSliding() const { return slideState_ != SlideState::Idle; }

int16_t HomePage::currentCategoryOffset(uint32_t nowMs) const {
  if (slideState_ != SlideState::SlidingCategory) return 0;
  const float t = static_cast<float>(nowMs - animStartMs_) /
                  static_cast<float>(ThemeMono::kSlideDurationMs);
  const float clamped = t > 1.0f ? 1.0f : t;
  return AnimMath::easeOutCubicInt16(animFromOffsetX_, animToOffsetX_, clamped);
}

int16_t HomePage::currentSubmenuOffset(uint32_t nowMs) const {
  if (slideState_ != SlideState::SlidingSubmenu) return 0;
  const float t = static_cast<float>(nowMs - animStartMs_) /
                  static_cast<float>(ThemeMono::kSlideDurationMs);
  const float clamped = t > 1.0f ? 1.0f : t;
  return AnimMath::easeOutCubicInt16(animFromOffsetY_, animToOffsetY_, clamped);
}

bool HomePage::hasUncalibratedBreadTick(uint32_t nowMs) const {
  (void)nowMs;
  return false;
}

bool HomePage::hasMenuAnimationTick(uint32_t nowMs) const {
  if (!isAnimationActive(nowMs)) {
    return false;
  }

  const uint32_t animNowMs = animationRenderTime(nowMs);
  const IconBitmap::Anim& anim = kMenuIcons[focusIndex_];
  if (anim.frameCount == 0 || anim.frameDelayMs == 0) return false;

  const uint16_t frame = IconBitmap::frameAt(anim, animNowMs);
  if (frame != lastFocusFrame_) {
    return true;
  }

  return false;
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
  return {0, 0, 383, 167};
}

void HomePage::renderTimeCardOnly(DisplayMonoTft& display, uint32_t nowMs) {
  render(display, 0, nowMs);
  frameUsedStaticLayer_ = false;
}

bool HomePage::isMenuIconsOnlyAnimationTick(uint32_t) const {
  return false;
}

bool HomePage::menuLayoutMatchesLastFullRender(uint32_t nowMs) const {
  return focusIndex_ == lastRenderedFocusIndex_ &&
         currentCategoryOffset(nowMs) == lastRenderedMenuOffsetY_;
}

HomePage::Rect HomePage::menuIconBounds(const DisplayMonoTft& display) const {
  return {0, 24, static_cast<int16_t>(display.width() - 1), 90};
}

HomePage::Rect HomePage::menuColumnBounds(const DisplayMonoTft& display) const {
  return {0, 0, static_cast<int16_t>(display.width() - 1),
          static_cast<int16_t>(display.height() - 1)};
}

bool HomePage::frameUsedStaticLayer() const { return frameUsedStaticLayer_; }

void HomePage::renderMenuIconsOnly(DisplayMonoTft& display, uint32_t nowMs) {
  render(display, 0, nowMs);
}

bool HomePage::isAnimationActive(uint32_t nowMs) const {
  return slideState_ != SlideState::Idle || isInteractiveAnimationWindow(nowMs);
}

uint8_t HomePage::focusIndex() const { return focusIndex_; }

const char* HomePage::menuLabel(uint8_t idx) const {
  const uint8_t safe = static_cast<uint8_t>(idx % kMenuCount);
  return kMenuNamesZh[safe];
}

const char* HomePage::focusName() const { return menuLabel(focusIndex_); }

const IconBitmap::Anim& HomePage::computerStatusIcon() {
  return kMenuIcons[kComputerStatusMenuIndex];
}

const IconBitmap::Anim& HomePage::sdUsbIcon() {
  return kMenuIcons[kSdUsbMenuIndex];
}

void HomePage::setClockData(const ClockData& data) {
  clockData_ = data;
}

bool HomePage::setBatteryData(const BatteryData& data) {
  const uint8_t percent = data.percent > 100U ? 100U : data.percent;
  if (batteryData_.valid == data.valid && batteryData_.percent == percent) return false;
  batteryData_ = data;
  batteryData_.percent = percent;
  return true;
}

void HomePage::render(DisplayMonoTft& display, int16_t pageOffsetX, uint32_t nowMs) {
  renderTransition(display, pageOffsetX, pageOffsetX, 0, 0, 0, nowMs);
}

bool HomePage::setFontStatus(FontStatus status) {
  if (fontStatus_ == status) return false;
  fontStatus_ = status;
  invalidateTextLayer();
  return true;
}

void HomePage::renderTransition(DisplayMonoTft& display, int16_t backgroundOffsetX,
                                int16_t, int16_t, int16_t, int16_t, uint32_t nowMs) {
  activeRenderOffsetX_ = backgroundOffsetX;
  auto& canvas = display.canvas();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  canvas.drawFilledRectangle(0, 0, width - 1, height - 1, ST7305_COLOR_BLACK);
  drawWave(canvas, backgroundOffsetX);
  drawHomeStatus(canvas, display.text(), backgroundOffsetX, clockData_, batteryData_);
  drawMenuWheel(display, 0, static_cast<int16_t>(height / 2), 0, 0, 0, 0, nowMs, false);
  lastRenderedFocusIndex_ = focusIndex_;
  lastRenderedMenuOffsetY_ = currentCategoryOffset(nowMs);
  frameUsedStaticLayer_ = false;
}

void HomePage::drawMenuWheel(DisplayMonoTft& display, int16_t, int16_t, int16_t, int16_t,
                             int16_t, int16_t, uint32_t animNowMs, bool) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_WHITE);

  const int16_t categoryOffset = static_cast<int16_t>(
      activeRenderOffsetX_ + currentCategoryOffset(animNowMs));
  for (uint8_t offset = 0; offset <= 4U; ++offset) {
    const uint8_t index = static_cast<uint8_t>((focusIndex_ + offset) % kMenuCount);
    const bool focus = offset == 0U;
    const int16_t iconSize = focus ? static_cast<int16_t>(ThemeMono::kIconFocusSize)
                                   : static_cast<int16_t>(ThemeMono::kIconSideSize);
    const int16_t centerX = static_cast<int16_t>(kXmbFocusCenterX +
                                                 offset * kXmbCategoryStep + categoryOffset);
    const int16_t iconX = static_cast<int16_t>(centerX - iconSize / 2);
    const int16_t iconY = static_cast<int16_t>(kXmbRailTop + (focus ? 0 : 5));
    const uint16_t frame = IconBitmap::frameAt(kMenuIcons[index], animNowMs);
    if (focus) lastFocusFrame_ = frame;
    IconBitmap::drawFrame(canvas, kMenuIcons[index], frame, iconX, iconY, iconSize, iconSize,
                          true, kXmbRailTop, kXmbRailRuleY - 2);
    if (focus) {
      const char* label = menuLabel(index);
      const int16_t labelX = static_cast<int16_t>(centerX - text.getUTF8Width(label) / 2);
      text.drawUTF8(labelX, 83, label);
      canvas.drawFastHLine(static_cast<int16_t>(centerX - 10), kXmbRailRuleY, 20,
                           ST7305_COLOR_WHITE);
    }
  }

  canvas.drawFastHLine(static_cast<int16_t>(14 + activeRenderOffsetX_), kXmbRailRuleY,
                       356, ST7305_COLOR_WHITE);
  if (submenuCount_ == 0U) return;

  const int16_t submenuOffset = currentSubmenuOffset(animNowMs);
  for (int8_t offset = -2; offset <= 1; ++offset) {
    const int16_t candidate = static_cast<int16_t>(submenuIndex_) + offset;
    if (candidate < 0 || candidate >= submenuCount_) continue;
    const int16_t baseline = static_cast<int16_t>(
        kXmbItemBaseline + offset * kXmbItemStep + submenuOffset);
    if (baseline < kXmbItemsTop - 14 || baseline > display.height() + 14) continue;
    char indexText[3];
    snprintf(indexText, sizeof(indexText), "%02u", static_cast<unsigned>(candidate + 1));
    text.drawUTF8(static_cast<int16_t>(kXmbIndexX + activeRenderOffsetX_), baseline, indexText);
    text.drawUTF8(static_cast<int16_t>(kXmbTextX + activeRenderOffsetX_), baseline,
                  submenuLabels_[candidate]);
  }
}

void HomePage::renderSleepClock(DisplayMonoTft& display, uint32_t, int16_t) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  canvas.drawFilledRectangle(0, 0, width - 1, height - 1, ST7305_COLOR_BLACK);

  constexpr int16_t x = 256;
  DrawUtils::drawClippedPseudoRoundOutline(canvas, x + 8, 16, x + 119, 59,
                                           ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  char timeText[6];
  snprintf(timeText, sizeof(timeText), clockData_.valid ? "%02u:%02u" : "--:--",
           static_cast<unsigned>(clockData_.hour), static_cast<unsigned>(clockData_.minute));
  const Segment7Font::Style timeStyle = {24, 40, 4, 2, 4, ST7305_COLOR_WHITE};
  const int16_t timeX = static_cast<int16_t>(x + 64 -
      Segment7Font::measureText(timeText, timeStyle) / 2);
  Segment7Font::drawText(canvas, timeX, 28, timeText, timeStyle);

  DrawUtils::drawClippedPseudoRoundOutline(canvas, x + 8, 72, x + 119, 135,
                                           ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  char dayText[3];
  snprintf(dayText, sizeof(dayText), "%02u", static_cast<unsigned>(clockData_.day));
  const Segment7Font::Style dayStyle = {24, 40, 4, 2, 4, ST7305_COLOR_WHITE};
  Segment7Font::drawText(canvas, static_cast<int16_t>(x + 18), 84, dayText, dayStyle);

  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  static const char* const weekdayNames[7] = {"周日", "周一", "周二", "周三",
                                               "周四", "周五", "周六"};
  const char* weekday = clockData_.valid && clockData_.weekday < 7U
                            ? weekdayNames[clockData_.weekday] : "--";
  DrawUtils::drawClippedFilledRect(canvas, x + 73, 84, x + 111, 106, ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(x + 78), 100, weekday);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  char yearText[8];
  snprintf(yearText, sizeof(yearText), "%04u", static_cast<unsigned>(clockData_.year));
  text.drawUTF8(static_cast<int16_t>(x + 18), 126, yearText);
  char monthText[8];
  snprintf(monthText, sizeof(monthText), "%u月", static_cast<unsigned>(clockData_.month));
  text.drawUTF8(static_cast<int16_t>(x + 78), 126, monthText);

  const uint8_t percent = batteryData_.percent > 100U ? 100U : batteryData_.percent;
  const uint16_t level = batteryData_.valid
                             ? static_cast<uint16_t>(percent >= 75U ? 3U : percent / 25U) : 0U;
  IconBitmap::drawFrame(canvas, kBatteryLevels, level % kBatteryLevels.frameCount,
                        static_cast<int16_t>(x + 16), 146,
                        BATTERY_LEVELS_FRAME_WIDTH, BATTERY_LEVELS_FRAME_HEIGHT, true, 140, 167);
  char batteryText[6];
  snprintf(batteryText, sizeof(batteryText), batteryData_.valid ? "%u%%" : "--%%",
           static_cast<unsigned>(percent));
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(static_cast<int16_t>(x + 40), 159, batteryText);
}

void HomePage::renderSleepTransition(DisplayMonoTft& display, uint32_t nowMs,
                                      uint8_t progress) const {
  renderSleepClock(display, nowMs);
  if (progress < 100U) {
    const int16_t lineX = static_cast<int16_t>(progress * display.width() / 100U);
    display.canvas().drawFastVLine(lineX, 12, display.height() - 24, ST7305_COLOR_WHITE);
  }
}

bool HomePage::isInteractiveAnimationWindow(uint32_t nowMs) const {
  return (nowMs - lastInteractionMs_) < kIdleAnimationTimeoutMs;
}

uint32_t HomePage::animationRenderTime(uint32_t nowMs) const {
  return isAnimationActive(nowMs) ? nowMs : animationTimeMs_;
}


