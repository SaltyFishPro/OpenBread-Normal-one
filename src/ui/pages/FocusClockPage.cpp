#include "FocusClockPage.h"

#include <cstdlib>

#include "../../bsp/DisplayMonoTft.h"

namespace {
constexpr int16_t kHeaderHeight = 28;
constexpr int16_t kFooterBaseline = 160;
constexpr int16_t kCenterX = 132;
constexpr int16_t kCenterY = 89;
constexpr int16_t kOptionStepY = 24;
constexpr int16_t kRailX = 24;
constexpr int16_t kPanelLeft = 238;
constexpr int16_t kPanelTop = 46;
constexpr int16_t kPanelRight = 375;
constexpr int16_t kPanelBottom = 132;
constexpr int16_t kSelectionLeft = 49;
constexpr int16_t kSelectionRight = 218;
constexpr uint32_t kAnimationDurationMs = 170;

const char* const kOptionNamesZh[] = {
    "开始专注",
    "专注 25 分钟",
    "专注 50 分钟",
    "短休息 5 分钟",
    "长休息 15 分钟",
    "自定义时长",
};

const char* const kOptionValues[] = {
    "READY",
    "25:00",
    "50:00",
    "05:00",
    "15:00",
    "--:--",
};

int16_t clampInt16(int16_t value, int16_t low, int16_t high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

void drawDetailHeader(DisplayMonoTft& display, const char* title, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  const int16_t titleWidth = text.getUTF8Width(title);
  text.drawUTF8(static_cast<int16_t>((width - titleWidth) / 2),
                static_cast<int16_t>(yOffset + 22), title);

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
}

void drawOption(DisplayMonoTft& display, const char* label, int16_t x, int16_t baseline,
                bool selected, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t labelWidth = text.getUTF8Width(label);
  const int16_t textHeight = 15;
  const int16_t left = selected ? kSelectionLeft : x;
  const int16_t right = selected ? kSelectionRight : x + labelWidth + 8;
  const int16_t top = static_cast<int16_t>(baseline - textHeight - 5);
  const int16_t bottom = static_cast<int16_t>(baseline + 5);
  const uint16_t background = selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  const uint16_t foreground = selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;

  if (top < yOffset + kHeaderHeight || bottom >= display.height()) {
    return;
  }

  if (selected) {
    canvas.drawFilledRectangle(left, top, right, bottom, background);
  }

  text.setFont(chinese_font_all);
  text.setFontMode(selected ? 0 : 1);
  text.setBackgroundColor(background);
  text.setForegroundColor(foreground);
  const int16_t textX = selected ? static_cast<int16_t>(left + (right - left + 1 - labelWidth) / 2)
                                 : x;
  text.drawUTF8(textX, baseline, label);
}
}  // namespace

bool FocusClockPage::isSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kMenuItemIndex;
}

bool FocusClockPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool upEdge,
                                       bool downEdge, bool okEdge, uint32_t nowMs) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (upEdge) {
    moveSelection(-1, nowMs);
    return true;
  }
  if (downEdge) {
    moveSelection(1, nowMs);
    return true;
  }

  // The preview consumes OK so it cannot fall through to the generic detail pager.
  return okEdge;
}

void FocusClockPage::moveSelection(int8_t direction, uint32_t nowMs) {
  if (direction < 0) {
    selectedIndex_ = selectedIndex_ == 0 ? static_cast<uint8_t>(kOptionCount - 1U)
                                         : static_cast<uint8_t>(selectedIndex_ - 1U);
  } else {
    selectedIndex_ = static_cast<uint8_t>((selectedIndex_ + 1U) % kOptionCount);
  }
  animationDirection_ = direction < 0 ? -1 : 1;
  animationStartMs_ = nowMs;
}

int8_t FocusClockPage::relativeIndex(uint8_t optionIndex) const {
  int8_t relative = static_cast<int8_t>(optionIndex) - static_cast<int8_t>(selectedIndex_);
  if (relative > static_cast<int8_t>(kOptionCount / 2U)) {
    relative = static_cast<int8_t>(relative - kOptionCount);
  } else if (relative < -static_cast<int8_t>(kOptionCount / 2U)) {
    relative = static_cast<int8_t>(relative + kOptionCount);
  }
  return relative;
}

bool FocusClockPage::isAnimating(uint8_t homeFocus, uint8_t sectionFocus,
                                  uint32_t nowMs) const {
  return isSelection(homeFocus, sectionFocus) && animationDirection_ != 0 &&
         nowMs - animationStartMs_ < kSelectionSlideMs;
}

bool FocusClockPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                         uint32_t nowMs) const {
  return isAnimating(homeFocus, sectionFocus, nowMs);
}

bool FocusClockPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                                  DisplayMonoTft& display, HomePage::Language language,
                                  uint32_t nowMs) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (yOffset >= display.height()) {
    return true;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const bool zh = language == HomePage::Language::Zh;
  drawDetailHeader(display, zh ? "专注时钟" : "Focus Clock", yOffset);

  const bool animationActive = animationDirection_ != 0 &&
                               nowMs - animationStartMs_ < kAnimationDurationMs;
  uint16_t progress = 100;
  if (animationActive) {
    progress = static_cast<uint16_t>(((nowMs - animationStartMs_) * 100U) /
                                     kAnimationDurationMs);
    if (progress > 100U) {
      progress = 100;
    }
  }
  const int16_t slide = static_cast<int16_t>(animationDirection_ * (100U - progress));

  canvas.drawLine(kRailX, static_cast<int16_t>(yOffset + 46), kRailX,
                  static_cast<int16_t>(yOffset + 134), ST7305_COLOR_BLACK);
  canvas.drawFilledCircle(kRailX, static_cast<int>(yOffset + kCenterY), 4,
                          ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  for (uint8_t index = 0; index < kOptionCount; ++index) {
    const int8_t relative = relativeIndex(index);
    const int16_t position = static_cast<int16_t>(relative * 100 + slide);
    if (position < -260 || position > 260) {
      continue;
    }

    const int16_t distance = static_cast<int16_t>(std::abs(position));
    const int16_t x = static_cast<int16_t>(kCenterX - 68 + (distance * 22) / 100);
    const int16_t baseline = static_cast<int16_t>(yOffset + kCenterY +
                                                  (position * kOptionStepY) / 100);
    const bool selected = index == selectedIndex_ && position > -35 && position < 35;
    const int16_t dotX = clampInt16(static_cast<int16_t>(kRailX + 8 + distance / 5), 0,
                                    static_cast<int16_t>(width - 1));
    if (baseline >= yOffset + kHeaderHeight && baseline < height - 20) {
      canvas.drawFilledCircle(dotX, baseline - 5, selected ? 3 : 2,
                              ST7305_COLOR_BLACK);
    }
    drawOption(display, kOptionNamesZh[index], x, baseline, selected, yOffset);
  }

  canvas.drawRectangle(kPanelLeft, static_cast<int16_t>(yOffset + kPanelTop), kPanelRight,
                       static_cast<int16_t>(yOffset + kPanelBottom), ST7305_COLOR_BLACK);
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(kPanelLeft + 12), static_cast<int16_t>(yOffset + 68),
                "当前预览");
  canvas.drawFilledRectangle(static_cast<int16_t>(kPanelLeft + 10),
                             static_cast<int16_t>(yOffset + 76),
                             static_cast<int16_t>(kPanelRight - 10),
                             static_cast<int16_t>(yOffset + 108), ST7305_COLOR_BLACK);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  const char* value = kOptionValues[selectedIndex_];
  const int16_t valueWidth = text.getUTF8Width(value);
  text.drawUTF8(static_cast<int16_t>(kPanelLeft + (kPanelRight - kPanelLeft + 1 - valueWidth) / 2),
                static_cast<int16_t>(yOffset + 103), value);
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(static_cast<int16_t>(kPanelLeft + 12), static_cast<int16_t>(yOffset + 123),
                "功能预览");

  text.drawUTF8(54, static_cast<int16_t>(yOffset + kFooterBaseline),
                zh ? "UP/DOWN 选择   OK 预留   LEFT 返回" : "UP/DOWN Select   LEFT Back");
  return true;
}
