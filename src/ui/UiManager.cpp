#include "UiManager.h"

#include <cstdio>

#include "../bsp/BoardConfig.h"
#include "ThemeMono.h"

namespace {
constexpr uint32_t kSectionTransitionMs = 600;
constexpr uint32_t kForwardBgEndMs = 90;
constexpr uint32_t kForwardMenuStartMs = 70;
constexpr uint32_t kForwardMenuEndMs = 500;
constexpr uint32_t kForwardSectionStartMs = 540;

constexpr uint32_t kBackSectionEndMs = 200;
constexpr uint32_t kBackMenuStartMs = 180;
constexpr uint32_t kBackMenuEndMs = 340;
constexpr uint32_t kBackBgStartMs = 300;
constexpr uint32_t kButtonDebounceMs = 25;

struct SectionContent {
  const char* title;
  const char* const* items;
  uint8_t itemCount;
};

const char* const kSettingsItems[] = {"重启设备", "语言", "恢复默认设置", "关于设备", "关于作者"};
const char* const kMusicItems[] = {"音乐列表", "收藏歌曲"};
const char* const kReaderItems[] = {"单词阅读", "提词器"};
const char* const kClockItems[] = {"时钟桌面", "计时器"};
const char* const kWirelessItems[] = {"WiFi连接", "蓝牙连接", "蓝牙远程拍照"};

const SectionContent kSectionContents[] = {
    {"设置", kSettingsItems, static_cast<uint8_t>(sizeof(kSettingsItems) / sizeof(kSettingsItems[0]))},
    {"音乐", kMusicItems, static_cast<uint8_t>(sizeof(kMusicItems) / sizeof(kMusicItems[0]))},
    {"阅读", kReaderItems, static_cast<uint8_t>(sizeof(kReaderItems) / sizeof(kReaderItems[0]))},
    {"时钟", kClockItems, static_cast<uint8_t>(sizeof(kClockItems) / sizeof(kClockItems[0]))},
    {"无线功能", kWirelessItems, static_cast<uint8_t>(sizeof(kWirelessItems) / sizeof(kWirelessItems[0]))},
};

const SectionContent& sectionContentFor(uint8_t homeFocus) {
  const size_t count = sizeof(kSectionContents) / sizeof(kSectionContents[0]);
  return kSectionContents[homeFocus % count];
}

struct SectionListLayout {
  int16_t listX;
  int16_t listStartBaseline;
  int16_t rowStep;
  int16_t textHeight;
  int16_t focusPadX;
  int16_t focusPadY;
  int16_t focusRadius;
  int16_t boxMaxRight;
};

struct SectionRowLayout {
  int16_t baselineY;
  int16_t boxLeft;
  int16_t boxTop;
  int16_t boxRight;
  int16_t boxBottom;
};

SectionListLayout makeSectionListLayout(int16_t xOffset, int16_t yOffset, int16_t width) {
  SectionListLayout layout;
  layout.listX = static_cast<int16_t>(xOffset + 16);
  layout.listStartBaseline = static_cast<int16_t>(yOffset + 52);
  layout.rowStep = 28;
  layout.textHeight = 14;
  layout.focusPadX = 12;
  layout.focusPadY = 4;
  layout.focusRadius = 8;
  layout.boxMaxRight = static_cast<int16_t>(xOffset + width - 16);
  return layout;
}

SectionRowLayout makeSectionRowLayout(const SectionListLayout& layout, int16_t labelW,
                                      uint8_t rowIndex) {
  SectionRowLayout row;
  row.baselineY = static_cast<int16_t>(layout.listStartBaseline + layout.rowStep * rowIndex);
  row.boxTop = static_cast<int16_t>(row.baselineY - layout.textHeight - layout.focusPadY);
  row.boxBottom = static_cast<int16_t>(row.baselineY + layout.focusPadY);
  row.boxLeft = static_cast<int16_t>(layout.listX - layout.focusPadX);
  row.boxRight = static_cast<int16_t>(layout.listX + labelW + layout.focusPadX - 1);
  if (row.boxRight > layout.boxMaxRight) {
    row.boxRight = layout.boxMaxRight;
  }
  return row;
}

void applySectionItemTextStyle(U8G2_FOR_ST73XX& text, bool selected) {
  if (selected) {
    text.setBackgroundColor(ST7305_COLOR_BLACK);
    text.setFontMode(0);
    text.setForegroundColor(ST7305_COLOR_WHITE);
    return;
  }

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
}

void drawFilledRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, int16_t radius, uint16_t color) {
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

  const int16_t w = static_cast<int16_t>(x2 - x1 + 1);
  const int16_t h = static_cast<int16_t>(y2 - y1 + 1);
  if (w <= 0 || h <= 0) {
    return;
  }

  int16_t r = radius;
  if (r < 0) {
    r = 0;
  }
  if (r > w / 2) {
    r = static_cast<int16_t>(w / 2);
  }
  if (r > h / 2) {
    r = static_cast<int16_t>(h / 2);
  }

  if (r == 0) {
    canvas.drawFilledRectangle(x1, y1, x2, y2, color);
    return;
  }

  canvas.drawFilledRectangle(static_cast<uint>(x1 + r), static_cast<uint>(y1),
                             static_cast<uint>(x2 - r), static_cast<uint>(y2), color);
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x1 + r - 1), static_cast<uint>(y2 - r), color);
  canvas.drawFilledRectangle(static_cast<uint>(x2 - r + 1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x2), static_cast<uint>(y2 - r), color);

  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
}
}  // namespace

bool UiManager::begin() {
  buttons_[0].pin = static_cast<uint8_t>(BoardConfig::kPinBtnLeft);
  buttons_[1].pin = static_cast<uint8_t>(BoardConfig::kPinBtnRight);
  buttons_[2].pin = static_cast<uint8_t>(BoardConfig::kPinBtnUp);
  buttons_[3].pin = static_cast<uint8_t>(BoardConfig::kPinBtnDown);
  buttons_[4].pin = static_cast<uint8_t>(BoardConfig::kPinBtnOk);

  const uint32_t nowMs = millis();
  for (auto& button : buttons_) {
    pinMode(button.pin, INPUT_PULLUP);
    const bool pressed = isPressed(button.pin);
    button.stablePressed = pressed;
    button.lastRawPressed = pressed;
    button.lastRawChangeMs = nowMs;
  }

  if (!display_.begin()) {
    return false;
  }
  if (!renderer_.begin()) {
    return false;
  }
  if (!homePage_.begin()) {
    return false;
  }

  state_ = UiState::Home;
  transitionStartMs_ = millis();
  needsRedraw_ = true;
  return true;
}

void UiManager::tick() {
  const uint32_t nowMs = millis();
  const InputEdges edges = pollInputEdges();
  updateState(edges, nowMs);

  if (!shouldRedraw(nowMs)) {
    return;
  }

  render(nowMs);
  needsRedraw_ = false;
}

UiManager::InputEdges UiManager::pollInputEdges() {
  InputEdges edges;
  const uint32_t nowMs = millis();

  auto risingEdgeDebounced = [&](ButtonEdge& button) -> bool {
    const bool rawPressed = isPressed(button.pin);
    if (rawPressed != button.lastRawPressed) {
      button.lastRawPressed = rawPressed;
      button.lastRawChangeMs = nowMs;
    }

    const bool stableEnough = (nowMs - button.lastRawChangeMs) >= kButtonDebounceMs;
    if (stableEnough && button.stablePressed != button.lastRawPressed) {
      const bool prevStable = button.stablePressed;
      button.stablePressed = button.lastRawPressed;
      return button.stablePressed && !prevStable;
    }

    return false;
  };

  edges.left = risingEdgeDebounced(buttons_[0]);
  edges.right = risingEdgeDebounced(buttons_[1]);
  edges.up = risingEdgeDebounced(buttons_[2]);
  edges.down = risingEdgeDebounced(buttons_[3]);
  edges.ok = risingEdgeDebounced(buttons_[4]);

  return edges;
}

void UiManager::updateState(const InputEdges& edges, uint32_t nowMs) {
  if (edges.left || edges.right || edges.up || edges.down || edges.ok) {
    needsRedraw_ = true;
  }

  switch (state_) {
    case UiState::Home: {
      const bool enterSection =
          homePage_.handleInput(edges.up, edges.down, edges.ok, nowMs);
      homePage_.update(nowMs);
      if (enterSection) {
        sectionFocusIndex_ = 0;
        state_ = UiState::ToSectionTransition;
        transitionStartMs_ = nowMs;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToSectionTransition: {
      if (nowMs - transitionStartMs_ >= kSectionTransitionMs) {
        state_ = UiState::Section;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToHomeTransition: {
      if (nowMs - transitionStartMs_ >= kSectionTransitionMs) {
        state_ = UiState::Home;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Section: {
      const SectionContent& content = sectionContentFor(homePage_.focusIndex());
      if (content.itemCount > 0 && edges.up) {
        if (sectionFocusIndex_ == 0) {
          sectionFocusIndex_ = static_cast<uint8_t>(content.itemCount - 1);
        } else {
          sectionFocusIndex_ = static_cast<uint8_t>(sectionFocusIndex_ - 1);
        }
        needsRedraw_ = true;
      } else if (content.itemCount > 0 && edges.down) {
        sectionFocusIndex_ = static_cast<uint8_t>((sectionFocusIndex_ + 1) % content.itemCount);
        needsRedraw_ = true;
      } else if (edges.left) {
        state_ = UiState::ToHomeTransition;
        transitionStartMs_ = nowMs;
        needsRedraw_ = true;
      } else if (edges.ok) {
        state_ = UiState::Detail;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Detail: {
      if (edges.left) {
        state_ = UiState::Section;
        needsRedraw_ = true;
      }
      break;
    }
  }
}

bool UiManager::shouldRedraw(uint32_t nowMs) const {
  if (needsRedraw_) {
    return true;
  }

  if (state_ == UiState::Home) {
    if (homePage_.isSliding()) {
      return true;
    }
    if (homePage_.hasAnimationTick(nowMs)) {
      return true;
    }
  }

  if (state_ == UiState::ToSectionTransition) {
    return true;
  }
  if (state_ == UiState::ToHomeTransition) {
    return true;
  }

  return false;
}

void UiManager::render(uint32_t nowMs) {
  renderer_.beginFrame();
  renderer_.markDirty(0, 0, static_cast<int16_t>(display_.width() - 1),
                      static_cast<int16_t>(display_.height() - 1));

  display_.clear();

  if (state_ == UiState::Home) {
    homePage_.render(display_, 0, nowMs);
  } else if (state_ == UiState::ToSectionTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kSectionTransitionMs ? kSectionTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());
    int16_t backgroundOffsetX = static_cast<int16_t>(-width);
    if (elapsed < kForwardBgEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kForwardBgEndMs == 0 ? 1 : kForwardBgEndMs);
      backgroundOffsetX = easeInCubic(0, static_cast<int16_t>(-width), t);
    }

    const uint32_t menuWindow = kForwardMenuEndMs - kForwardMenuStartMs;
    const uint32_t menuStep = menuWindow / 3U;
    const uint32_t menuStartUp = kForwardMenuStartMs;
    const uint32_t menuStartFocus = kForwardMenuStartMs + menuStep;
    const uint32_t menuStartDown = kForwardMenuStartMs + (menuStep * 2U);
    const uint32_t menuDur = menuStep;

    int16_t menuUpOffsetX = static_cast<int16_t>(-width);
    int16_t menuFocusOffsetX = static_cast<int16_t>(-width);
    int16_t menuDownOffsetX = static_cast<int16_t>(-width);

    auto forwardMenuOffset = [&](uint32_t startMs) -> int16_t {
      if (elapsed <= startMs) {
        return 0;
      }
      const uint32_t endMs = startMs + menuDur;
      if (elapsed >= endMs) {
        return static_cast<int16_t>(-width);
      }
      const float t = static_cast<float>(elapsed - startMs) /
                      static_cast<float>(menuDur == 0 ? 1 : menuDur);
      return easeInCubic(0, static_cast<int16_t>(-width), t);
    };

    menuUpOffsetX = forwardMenuOffset(menuStartUp);
    menuFocusOffsetX = forwardMenuOffset(menuStartFocus);
    menuDownOffsetX = forwardMenuOffset(menuStartDown);

    int16_t sectionOffsetY = height;
    if (elapsed >= kForwardSectionStartMs) {
      const uint32_t moveDuration = kSectionTransitionMs - kForwardSectionStartMs;
      const float t = static_cast<float>(elapsed - kForwardSectionStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      sectionOffsetY = easeOutCubic(height, 0, t);
    }

    homePage_.renderTransition(display_, backgroundOffsetX, 0, menuUpOffsetX,
                               menuFocusOffsetX, menuDownOffsetX, nowMs);
    renderSection(0, sectionOffsetY);
  } else if (state_ == UiState::ToHomeTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kSectionTransitionMs ? kSectionTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());
    int16_t sectionOffsetY = height;
    if (elapsed < kBackSectionEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kBackSectionEndMs == 0 ? 1 : kBackSectionEndMs);
      sectionOffsetY = easeInCubic(0, height, t);
    }

    const uint32_t menuWindow = kBackMenuEndMs - kBackMenuStartMs;
    const uint32_t menuStep = menuWindow / 3U;
    const uint32_t menuStartDown = kBackMenuStartMs;
    const uint32_t menuStartFocus = kBackMenuStartMs + menuStep;
    const uint32_t menuStartUp = kBackMenuStartMs + (menuStep * 2U);
    const uint32_t menuDur = menuStep;

    int16_t menuUpOffsetX = static_cast<int16_t>(-width);
    int16_t menuFocusOffsetX = static_cast<int16_t>(-width);
    int16_t menuDownOffsetX = static_cast<int16_t>(-width);

    auto backwardMenuOffset = [&](uint32_t startMs) -> int16_t {
      if (elapsed <= startMs) {
        return static_cast<int16_t>(-width);
      }
      const uint32_t endMs = startMs + menuDur;
      if (elapsed >= endMs) {
        return 0;
      }
      const float t = static_cast<float>(elapsed - startMs) /
                      static_cast<float>(menuDur == 0 ? 1 : menuDur);
      return easeOutCubic(static_cast<int16_t>(-width), 0, t);
    };

    menuDownOffsetX = backwardMenuOffset(menuStartDown);
    menuFocusOffsetX = backwardMenuOffset(menuStartFocus);
    menuUpOffsetX = backwardMenuOffset(menuStartUp);

    int16_t backgroundOffsetX = static_cast<int16_t>(-width);
    if (elapsed >= kBackBgStartMs) {
      const uint32_t moveDuration = kSectionTransitionMs - kBackBgStartMs;
      const float t = static_cast<float>(elapsed - kBackBgStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      backgroundOffsetX = easeOutCubic(static_cast<int16_t>(-width), 0, t);
    }

    homePage_.renderTransition(display_, backgroundOffsetX, 0, menuUpOffsetX,
                               menuFocusOffsetX, menuDownOffsetX, nowMs);
    renderSection(0, sectionOffsetY);
  } else if (state_ == UiState::Section) {
    renderSection(0, 0);
  } else {
    renderDetail();
  }

  if (renderer_.hasDirty()) {
    display_.present();
  }
}

void UiManager::renderSection(int16_t xOffset, int16_t yOffset) {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const uint8_t focus = homePage_.focusIndex();
  const SectionContent& content = sectionContentFor(focus);
  const uint8_t itemCount = content.itemCount;
  const SectionListLayout layout = makeSectionListLayout(xOffset, yOffset, width);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(xOffset + 12, static_cast<int16_t>(yOffset + 20), content.title);

  if (itemCount == 0) {
    return;
  }

  const uint8_t selected = static_cast<uint8_t>(sectionFocusIndex_ % itemCount);

  for (uint8_t i = 0; i < itemCount; ++i) {
    const int16_t labelW = text.getUTF8Width(content.items[i]);
    const SectionRowLayout row = makeSectionRowLayout(layout, labelW, i);
    const bool isSelected = (i == selected);

    if (isSelected) {
      drawFilledRoundRect(canvas, row.boxLeft, row.boxTop, row.boxRight, row.boxBottom,
                          layout.focusRadius,
                          ST7305_COLOR_BLACK);
    }
    applySectionItemTextStyle(text, isSelected);

    text.drawUTF8(layout.listX, row.baselineY, content.items[i]);
  }

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
}

void UiManager::renderDetail() {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());

  canvas.drawFilledRectangle(0, 0, width - 1, ThemeMono::kHeaderHeight,
                             ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(6, 24, "详情页");

  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(8, 62, homePage_.focusName());
  text.drawUTF8(8, 92, "详情内容开发中");
  text.drawUTF8(width - 92, height - 10, "LEFT: 返回");

  canvas.drawRectangle(4, 32, width - 5, height - 5, ST7305_COLOR_BLACK);
}

bool UiManager::isPressed(uint8_t pin) const { return digitalRead(pin) == LOW; }

int16_t UiManager::easeInCubic(int16_t from, int16_t to, float t) const {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  const float eased = t * t * t;
  return static_cast<int16_t>(from + (to - from) * eased);
}

int16_t UiManager::easeOutCubic(int16_t from, int16_t to, float t) const {
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


