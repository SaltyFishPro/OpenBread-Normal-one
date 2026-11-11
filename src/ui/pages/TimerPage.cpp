#include "TimerPage.h"

#include <cstdio>
#include <cstring>

#include "../../bsp/DisplayMonoTft.h"
#include "../DrawUtils.h"
#include "../SdBitmapText.h"
#include "../SdUiText.h"
#include "../Segment7Font.h"
#include "../TextUtils.h"
#include "HomePage.h"

static_assert(TimerPage::kHomeIndex == HomePage::kTimerMenuIndex,
              "Timer home index must match the main menu slot");

namespace {

// ---- 布局（384×168，相对 yOffset） ----
constexpr int16_t kHeaderHeight = 28;
constexpr int16_t kHeaderBaseline = 21;

constexpr Segment7Font::Style kDigitStyle = {30, 54, 3, 2, 14, ST7305_COLOR_BLACK};
constexpr int16_t kBigTimeX = 12;
constexpr int16_t kBigTimeY = 55;

constexpr int16_t kSublineCenterX = 98;
constexpr int16_t kSublineBaseline = 128;

constexpr int16_t kPresetX = 222;
constexpr int16_t kPresetWidth = 150;
constexpr int16_t kPresetHeight = 26;
constexpr int16_t kPresetRadius = 13;
constexpr int16_t kPresetRowTop[3] = {32, 66, 100};

constexpr int16_t kStatusX = 196;
constexpr int16_t kStatusY = 48;
constexpr int16_t kStatusWidth = 176;
constexpr int16_t kStatusHeight = 24;
constexpr int16_t kStatusRadius = 12;

constexpr int16_t kProgressX = 196;
constexpr int16_t kProgressY = 84;
constexpr int16_t kProgressWidth = 176;
constexpr int16_t kProgressHeight = 6;

constexpr int16_t kRunInfoCenterX = 284;
constexpr int16_t kRunInfoBaseline = 112;

constexpr int16_t kRuleX1 = 16;
constexpr int16_t kRuleX2 = 367;
constexpr int16_t kRuleY = 136;

constexpr int16_t kActionRight = 16;
constexpr int16_t kActionY = 141;
constexpr int16_t kActionHeight = 24;
constexpr int16_t kActionRadius = 6;
constexpr int16_t kActionPadX = 12;

constexpr int16_t kDoneMainBaseline = 80;
constexpr int16_t kDoneSubBaseline = 106;

constexpr uint16_t kPresetSeconds[6] = {60, 300, 600, 900, 1800, 3600};
constexpr const char* kPresetLabels[7] = {"1分钟", "5分钟", "10分钟", "15分钟",
                                          "30分钟", "60分钟", "自定义"};

constexpr uint8_t kCustomMinutesMax = 99;
constexpr uint8_t kCustomMinutesMin = 1;

void drawCenteredText(DisplayMonoTft& display, const char* value, int16_t centerX,
                      int16_t baseline, uint16_t foreground, uint16_t background) {
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(foreground);
  text.setBackgroundColor(background);
  text.setFontMode(1);
  text.drawUTF8(TextUtils::centeredTextX(text, value, centerX), baseline, value);
}

void drawHeader(DisplayMonoTft& display, int16_t yOffset, const char* leftLabel,
                bool inverted) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const uint16_t bandColor = inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  const uint16_t textColor = inverted ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;

  DrawUtils::drawClippedFilledRect(canvas, 0, yOffset, width - 1,
                                   static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                                   bandColor);
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(textColor);
  text.setBackgroundColor(bandColor);
  text.setFontMode(1);
  text.drawUTF8(12, static_cast<int16_t>(yOffset + kHeaderBaseline), leftLabel);
  const char* title = "定时器";
  text.drawUTF8(TextUtils::centeredTextX(text, title, width / 2),
                static_cast<int16_t>(yOffset + kHeaderBaseline), title);
}

void drawBigTime(DisplayMonoTft& display, int16_t yOffset, uint32_t seconds) {
  char text[8];
  snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(seconds / 60U),
           static_cast<unsigned>(seconds % 60U));
  Segment7Font::drawText(display.canvas(), kBigTimeX, static_cast<int16_t>(yOffset + kBigTimeY),
                         text, kDigitStyle);
}

void drawPresetRow(DisplayMonoTft& display, int16_t yOffset, int16_t rowTop,
                   const char* indexText, const char* label, bool focus) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t y = static_cast<int16_t>(yOffset + rowTop);
  if (focus) {
    DrawUtils::drawRoundRect(canvas, kPresetX, y, kPresetWidth, kPresetHeight, kPresetRadius,
                             ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
  }
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(focus ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(focus ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setFontMode(1);
  const int16_t baseline = static_cast<int16_t>(y + 18);
  text.drawUTF8(kPresetX + 10, baseline, indexText);
  text.drawUTF8(static_cast<int16_t>(kPresetX + 10 + text.getUTF8Width(indexText) + 8), baseline,
                label);
}

void drawThreeRows(DisplayMonoTft& display, int16_t yOffset, const char* i0, const char* l0,
                   const char* i1, const char* l1, const char* i2, const char* l2) {
  drawPresetRow(display, yOffset, kPresetRowTop[0], i0, l0, false);
  drawPresetRow(display, yOffset, kPresetRowTop[1], i1, l1, true);
  drawPresetRow(display, yOffset, kPresetRowTop[2], i2, l2, false);
}

void drawStatusChip(DisplayMonoTft& display, int16_t yOffset, bool paused) {
  auto& canvas = display.canvas();
  const int16_t y = static_cast<int16_t>(yOffset + kStatusY);
  const uint16_t fill = paused ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  DrawUtils::drawRoundRect(canvas, kStatusX, y, kStatusWidth, kStatusHeight, kStatusRadius,
                           fill, ST7305_COLOR_BLACK);
  drawCenteredText(display, paused ? "已暂停" : "计时中",
                   static_cast<int16_t>(kStatusX + kStatusWidth / 2),
                   static_cast<int16_t>(y + 16),
                   paused ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK, fill);
}

void drawProgress(DisplayMonoTft& display, int16_t yOffset, uint32_t totalMs, uint32_t remainMs) {
  auto& canvas = display.canvas();
  const int16_t y = static_cast<int16_t>(yOffset + kProgressY);
  DrawUtils::drawClippedRect(canvas, kProgressX, y, static_cast<int16_t>(kProgressX + kProgressWidth - 1),
                             static_cast<int16_t>(y + kProgressHeight - 1), ST7305_COLOR_BLACK);
  if (totalMs == 0 || remainMs >= totalMs) {
    return;
  }
  const int32_t fill = static_cast<int32_t>(kProgressWidth - 2) *
                       static_cast<int32_t>(totalMs - remainMs) / static_cast<int32_t>(totalMs);
  if (fill > 0) {
    DrawUtils::drawClippedFilledRect(canvas, kProgressX + 1, static_cast<int16_t>(y + 1),
                                     static_cast<int16_t>(kProgressX + fill),
                                     static_cast<int16_t>(y + kProgressHeight - 2),
                                     ST7305_COLOR_BLACK);
  }
}

void drawRuleAndAction(DisplayMonoTft& display, int16_t yOffset, const char* actionLabel,
                       bool inverted) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const uint16_t ruleColor = inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  DrawUtils::drawClippedFilledRect(canvas, kRuleX1, static_cast<int16_t>(yOffset + kRuleY),
                                   kRuleX2, static_cast<int16_t>(yOffset + kRuleY), ruleColor);

  text.setFont(u8g2_font_7x14_tf);
  const int16_t textWidth = text.getUTF8Width(actionLabel);
  const int16_t btnWidth = static_cast<int16_t>(textWidth + kActionPadX * 2);
  const int16_t btnX = static_cast<int16_t>(width - kActionRight - btnWidth);
  const int16_t btnY = static_cast<int16_t>(yOffset + kActionY);
  const uint16_t fill = inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  const uint16_t ink = inverted ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  DrawUtils::drawRoundRect(canvas, btnX, btnY, btnWidth, kActionHeight, kActionRadius, fill, fill);
  text.setForegroundColor(ink);
  text.setBackgroundColor(fill);
  text.setFontMode(1);
  text.drawUTF8(static_cast<int16_t>(btnX + (btnWidth - textWidth) / 2),
                static_cast<int16_t>(btnY + 16), actionLabel);
}

// 完成页主标题用 24px 字库、按字间距绘制；字库未就绪时回退 14px。
void drawLetterSpaced(DisplayMonoTft& display, SdBitmapFont* font, const char* value,
                      int16_t centerX, int16_t baseline, uint16_t color,
                      SdBitmapText::Clip clip, int16_t spacing) {
  int16_t total = 0;
  int16_t count = 0;
  for (const char* p = value; *p != '\0';) {
    const uint8_t n = TextUtils::utf8CharLen(*p);
    char glyph[5] = {};
    memcpy(glyph, p, n);
    total = static_cast<int16_t>(total + SdUiText::width(display, font, glyph));
    ++count;
    p += n;
  }
  if (count > 1) {
    total = static_cast<int16_t>(total + spacing * (count - 1));
  }
  int16_t x = static_cast<int16_t>(centerX - total / 2);
  for (const char* p = value; *p != '\0';) {
    const uint8_t n = TextUtils::utf8CharLen(*p);
    char glyph[5] = {};
    memcpy(glyph, p, n);
    SdUiText::draw(display, font, x, baseline, glyph, color, clip);
    x = static_cast<int16_t>(x + SdUiText::width(display, font, glyph) + spacing);
    p += n;
  }
}

}  // namespace

bool TimerPage::isSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kMenuItemIndex;
}

void TimerPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return;
  }
  resetSession();
}

void TimerPage::resetSession() {
  presetIndex_ = 0;
  customMinutes_ = 5;
  stepMinutes_ = 1;
  stopToPreset();
}

void TimerPage::stopToPreset() {
  view_ = View::Preset;
  totalMs_ = 0;
  remainMs_ = 0;
  deadlineMs_ = 0;
  paused_ = false;
  lastShownSec_ = 0xFFFFFFFFU;
  abandonConfirm_ = PopupView::ConfirmState{};
}

void TimerPage::startTimer(uint32_t nowMs, uint32_t totalSeconds) {
  totalMs_ = totalSeconds * 1000UL;
  remainMs_ = totalMs_;
  deadlineMs_ = nowMs + totalMs_;
  paused_ = false;
  lastShownSec_ = 0xFFFFFFFFU;
  abandonConfirm_ = PopupView::ConfirmState{};
  view_ = View::Running;
}

uint32_t TimerPage::remainingMs(uint32_t nowMs) const {
  if (paused_) {
    return remainMs_;
  }
  const int32_t diff = static_cast<int32_t>(nowMs - deadlineMs_);
  return diff >= 0 ? 0 : static_cast<uint32_t>(-diff);
}

uint32_t TimerPage::presetSeconds(uint8_t index) {
  return index < 6U ? kPresetSeconds[index] : 0U;
}

const char* TimerPage::presetLabel(uint8_t index) {
  return index < kPresetCount ? kPresetLabels[index] : "";
}

bool TimerPage::update(uint32_t nowMs) {
  bool changed = false;
  const PopupView::Result result = PopupView::update(abandonConfirm_, nowMs);
  if (result == PopupView::Result::Confirmed) {
    stopToPreset();
    return true;
  }
  if (result == PopupView::Result::Cancelled) {
    changed = true;
  }

  if (view_ != View::Running || paused_) {
    return changed;
  }

  const uint32_t remaining = remainingMs(nowMs);
  if (remaining == 0) {
    abandonConfirm_ = PopupView::ConfirmState{};
    view_ = View::Done;
    remainMs_ = 0;
    paused_ = false;
    return true;
  }
  const uint32_t second = (remaining + 999U) / 1000U;
  if (second != lastShownSec_) {
    lastShownSec_ = second;
    changed = true;
  }
  return changed;
}

bool TimerPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                                  bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                                  uint32_t nowMs) {
  (void)leftEdge;
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (PopupView::isVisible(abandonConfirm_.anim)) {
    return PopupView::handleInput(abandonConfirm_, false, rightEdge, upEdge, downEdge, okEdge,
                                  nowMs);
  }

  if (view_ == View::Done) {
    if (rightEdge || upEdge || downEdge || okEdge) {
      stopToPreset();
      return true;
    }
    return false;
  }

  if (view_ == View::Preset) {
    if (upEdge) {
      presetIndex_ = static_cast<uint8_t>((presetIndex_ + kPresetCount - 1U) % kPresetCount);
      return true;
    }
    if (downEdge) {
      presetIndex_ = static_cast<uint8_t>((presetIndex_ + 1U) % kPresetCount);
      return true;
    }
    if (okEdge) {
      if (presetIndex_ == kCustomPresetIndex) {
        stepMinutes_ = 1;
        view_ = View::Custom;
      } else {
        startTimer(nowMs, presetSeconds(presetIndex_));
      }
      return true;
    }
    if (rightEdge) {
      stepMinutes_ = 1;
      view_ = View::Custom;
      return true;
    }
    return false;
  }

  if (view_ == View::Custom) {
    if (upEdge) {
      if (customMinutes_ < kCustomMinutesMax) {
        const uint16_t next = static_cast<uint16_t>(customMinutes_) + stepMinutes_;
        customMinutes_ = next > kCustomMinutesMax ? kCustomMinutesMax : static_cast<uint8_t>(next);
      }
      return true;
    }
    if (downEdge) {
      customMinutes_ = customMinutes_ > stepMinutes_
                           ? static_cast<uint8_t>(customMinutes_ - stepMinutes_)
                           : kCustomMinutesMin;
      return true;
    }
    if (rightEdge) {
      stepMinutes_ = stepMinutes_ == 1U ? 5U : 1U;
      return true;
    }
    if (okEdge) {
      startTimer(nowMs, static_cast<uint32_t>(customMinutes_) * 60U);
      return true;
    }
    return false;
  }

  // Running / Paused
  if (okEdge) {
    if (paused_) {
      deadlineMs_ = nowMs + remainMs_;
      paused_ = false;
      lastShownSec_ = 0xFFFFFFFFU;
    } else {
      remainMs_ = remainingMs(nowMs);
      paused_ = true;
    }
    return true;
  }
  return rightEdge || upEdge || downEdge;
}

bool TimerPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (PopupView::isVisible(abandonConfirm_.anim)) {
    if (!abandonConfirm_.anim.closing) {
      PopupView::requestClose(abandonConfirm_, false, nowMs);
    }
    return true;
  }
  if (view_ == View::Done) {
    stopToPreset();
    return true;
  }
  if (view_ == View::Running) {
    PopupView::open(abandonConfirm_, nowMs);
    return true;
  }
  if (view_ == View::Custom) {
    view_ = View::Preset;
    return true;
  }
  // 预设页返回上级，由 UiManager 处理。
  return false;
}

bool TimerPage::isAnimating(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const {
  (void)nowMs;
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  return PopupView::isAnimating(abandonConfirm_.anim);
}

bool TimerPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const {
  return isAnimating(homeFocus, sectionFocus, nowMs);
}

bool TimerPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                             DisplayMonoTft& display, uint32_t nowMs,
                             SdBitmapFont* titleFont) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (yOffset >= display.height()) {
    return true;
  }
  switch (view_) {
    case View::Preset:
      drawPreset(display, yOffset);
      break;
    case View::Custom:
      drawCustom(display, yOffset);
      break;
    case View::Running:
      drawRunning(display, yOffset, nowMs);
      break;
    case View::Done:
      drawDone(display, yOffset, titleFont);
      break;
  }
  PopupView::drawConfirm(display, yOffset, abandonConfirm_, "放弃当前计时？", "取消", "放弃",
                         nowMs);
  return true;
}

void TimerPage::drawPreset(DisplayMonoTft& display, int16_t yOffset) const {
  drawHeader(display, yOffset, "Left 返回", false);

  const uint32_t seconds = presetIndex_ == kCustomPresetIndex
                               ? static_cast<uint32_t>(customMinutes_) * 60U
                               : presetSeconds(presetIndex_);
  drawBigTime(display, yOffset, seconds);

  drawCenteredText(display, presetIndex_ == kCustomPresetIndex ? "OK 进入自定义时长"
                                                               : "选择时长，OK 立即开始",
                   kSublineCenterX, static_cast<int16_t>(yOffset + kSublineBaseline),
                   ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);

  const uint8_t prev = static_cast<uint8_t>((presetIndex_ + kPresetCount - 1U) % kPresetCount);
  const uint8_t next = static_cast<uint8_t>((presetIndex_ + 1U) % kPresetCount);
  char i0[3] = {};
  char i1[3] = {};
  char i2[3] = {};
  snprintf(i0, sizeof(i0), "%02u", static_cast<unsigned>(prev + 1U));
  snprintf(i1, sizeof(i1), "%02u", static_cast<unsigned>(presetIndex_ + 1U));
  snprintf(i2, sizeof(i2), "%02u", static_cast<unsigned>(next + 1U));
  drawThreeRows(display, yOffset, i0, presetLabel(prev), i1, presetLabel(presetIndex_), i2,
                presetLabel(next));

  drawRuleAndAction(display, yOffset,
                    presetIndex_ == kCustomPresetIndex ? "OK 进入" : "OK 开始", false);
}

void TimerPage::drawCustom(DisplayMonoTft& display, int16_t yOffset) const {
  drawHeader(display, yOffset, "Left 返回", false);

  drawBigTime(display, yOffset, static_cast<uint32_t>(customMinutes_) * 60U);

  drawCenteredText(display, "Right 切换步进", kSublineCenterX,
                   static_cast<int16_t>(yOffset + kSublineBaseline), ST7305_COLOR_BLACK,
                   ST7305_COLOR_WHITE);

  char upLabel[16] = {};
  char downLabel[16] = {};
  snprintf(upLabel, sizeof(upLabel), "增加 %u 分钟", static_cast<unsigned>(stepMinutes_));
  snprintf(downLabel, sizeof(downLabel), "减少 %u 分钟", static_cast<unsigned>(stepMinutes_));
  drawThreeRows(display, yOffset, "UP", upLabel, "07", "自定义", "DN", downLabel);

  drawRuleAndAction(display, yOffset, "OK 开始", false);
}

void TimerPage::drawRunning(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const {
  drawHeader(display, yOffset, "Left 放弃", false);

  const uint32_t remaining = remainingMs(nowMs);
  drawBigTime(display, yOffset, (remaining + 999U) / 1000U);

  drawStatusChip(display, yOffset, paused_);
  drawProgress(display, yOffset, totalMs_, remaining);

  char info[48] = {};
  const uint32_t remainSec = (remaining + 999U) / 1000U;
  const uint32_t totalSec = totalMs_ / 1000U;
  snprintf(info, sizeof(info), "剩余 %02u:%02u / 共 %02u:%02u",
           static_cast<unsigned>(remainSec / 60U), static_cast<unsigned>(remainSec % 60U),
           static_cast<unsigned>(totalSec / 60U), static_cast<unsigned>(totalSec % 60U));
  drawCenteredText(display, info, kRunInfoCenterX,
                   static_cast<int16_t>(yOffset + kRunInfoBaseline), ST7305_COLOR_BLACK,
                   ST7305_COLOR_WHITE);

  drawRuleAndAction(display, yOffset, paused_ ? "OK 继续" : "OK 暂停", false);
}

void TimerPage::drawDone(DisplayMonoTft& display, int16_t yOffset, SdBitmapFont* titleFont) const {
  auto& canvas = display.canvas();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  DrawUtils::drawClippedFilledRect(canvas, 0, yOffset, width - 1, height - 1,
                                   ST7305_COLOR_BLACK);
  drawHeader(display, yOffset, "Left 返回", true);

  const char* main = "时间到";
  if (titleFont != nullptr && SdUiText::ready(titleFont, 60)) {
    const SdBitmapText::Clip clip = {0, static_cast<int16_t>(yOffset + kHeaderHeight),
                                     static_cast<int16_t>(width - 1),
                                     static_cast<int16_t>(height - 1)};
    drawLetterSpaced(display, titleFont, main, width / 2,
                     static_cast<int16_t>(yOffset + kDoneMainBaseline), ST7305_COLOR_WHITE, clip,
                     5);
  } else {
    drawCenteredText(display, main, width / 2, static_cast<int16_t>(yOffset + kDoneMainBaseline),
                     ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  }
  drawCenteredText(display, "任意按键停止", width / 2,
                   static_cast<int16_t>(yOffset + kDoneSubBaseline), ST7305_COLOR_WHITE,
                   ST7305_COLOR_BLACK);

  drawRuleAndAction(display, yOffset, "OK 停止", true);
}
