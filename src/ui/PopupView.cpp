#include "PopupView.h"

#include "../bsp/DisplayMonoTft.h"
#include "AnimMath.h"
#include "DrawUtils.h"
#include "TextUtils.h"

namespace {
constexpr int16_t kConfirmPanelWidth = 280;
constexpr int16_t kConfirmPanelHeight = 100;
constexpr int16_t kPanelRadius = 16;
constexpr int16_t kOptionWidth = 96;
constexpr int16_t kOptionHeight = 32;
constexpr int16_t kOptionGap = 24;
constexpr int16_t kOptionRadius = 10;
constexpr int16_t kInfoPanelWidth = 240;
constexpr int16_t kInfoPanelHeight = 100;
// 进场起始尺寸：85%，配合下滑形成"落下来"的观感，同时保证文字不溢出胶囊。
constexpr uint16_t kStartScaleFixed = 850;

int16_t lerpByShown(int16_t from, int16_t to, uint16_t shown) {
  return static_cast<int16_t>(from + (static_cast<int32_t>(to - from) * shown) /
                                         AnimMath::kFixedScale);
}

void drawPanel(DisplayMonoTft& display, int16_t x, int16_t y, int16_t width, int16_t height) {
  DrawUtils::drawRoundRect(display.canvas(), x, y, width, height, kPanelRadius,
                           ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
}

void drawCenteredText(DisplayMonoTft& display, const char* value, int16_t centerX,
                      int16_t baseline, uint16_t foreground, uint16_t background) {
  auto& text = display.text();
  text.setBackgroundColor(background);
  text.setForegroundColor(foreground);
  text.setFontMode(1);
  text.drawUTF8(TextUtils::centeredTextX(text, value, centerX), baseline, value);
}

// 面板几何：显示程度 0 时整体位于屏幕上沿之外，满显示时居中。
void animatedPanelBounds(int16_t panelWidth, int16_t panelHeight, int16_t displayWidth,
                         int16_t displayHeight, int16_t yOffset, uint16_t shown, int16_t& x,
                         int16_t& y, int16_t& width, int16_t& height) {
  width = lerpByShown(static_cast<int16_t>(panelWidth * kStartScaleFixed / AnimMath::kFixedScale),
                      panelWidth, shown);
  height = lerpByShown(static_cast<int16_t>(panelHeight * kStartScaleFixed / AnimMath::kFixedScale),
                       panelHeight, shown);
  x = static_cast<int16_t>((displayWidth - width) / 2);
  const int16_t finalY = static_cast<int16_t>(yOffset + (displayHeight - height) / 2);
  const int16_t startY = static_cast<int16_t>(yOffset - height - 4);
  y = lerpByShown(startY, finalY, shown);
}
}  // namespace

namespace PopupView {

uint16_t shownFixed(const Anim& anim, uint32_t nowMs) {
  if (!anim.visible) {
    return 0;
  }
  if (!anim.running) {
    return anim.closing ? 0 : AnimMath::kFixedScale;
  }
  const uint16_t raw = AnimMath::fixedClampProgress(nowMs - anim.startMs, kAnimMs);
  const uint16_t eased = AnimMath::fixedEaseOutCubic(raw);
  return anim.closing ? static_cast<uint16_t>(AnimMath::kFixedScale - eased) : eased;
}

bool isAnimating(const Anim& anim) { return anim.running; }

void open(ConfirmState& state, uint32_t nowMs) {
  state.primarySelected = true;
  state.pendingConfirmed = false;
  state.anim.visible = true;
  state.anim.running = true;
  state.anim.closing = false;
  state.anim.startMs = nowMs;
}

void requestClose(ConfirmState& state, bool confirmed, uint32_t nowMs) {
  if (!state.anim.visible || state.anim.closing) {
    return;
  }
  state.pendingConfirmed = confirmed;
  state.anim.running = true;
  state.anim.closing = true;
  state.anim.startMs = nowMs;
}

bool handleInput(ConfirmState& state, bool leftEdge, bool rightEdge, bool upEdge, bool downEdge,
                 bool okEdge, uint32_t nowMs) {
  if (!state.anim.visible) {
    return false;
  }
  if (state.anim.closing) {
    // 退场动画期间吞掉按键，避免误操作落到后面的页面。
    return leftEdge || rightEdge || upEdge || downEdge || okEdge;
  }
  if (leftEdge) {
    requestClose(state, false, nowMs);
    return true;
  }
  if (rightEdge || upEdge || downEdge) {
    state.primarySelected = !state.primarySelected;
    return true;
  }
  if (okEdge) {
    requestClose(state, !state.primarySelected, nowMs);
    return true;
  }
  return false;
}

Result update(ConfirmState& state, uint32_t nowMs) {
  if (!state.anim.running) {
    return Result::None;
  }
  if (nowMs - state.anim.startMs < kAnimMs) {
    return Result::None;
  }
  state.anim.running = false;
  if (!state.anim.closing) {
    return Result::None;
  }
  state.anim.visible = false;
  const bool confirmed = state.pendingConfirmed;
  state.pendingConfirmed = false;
  state.primarySelected = true;
  return confirmed ? Result::Confirmed : Result::Cancelled;
}

void open(InfoState& state, uint32_t nowMs) {
  state.anim.visible = true;
  state.anim.running = true;
  state.anim.closing = false;
  state.anim.startMs = nowMs;
}

void requestClose(InfoState& state, uint32_t nowMs) {
  if (!state.anim.visible || state.anim.closing) {
    return;
  }
  state.anim.running = true;
  state.anim.closing = true;
  state.anim.startMs = nowMs;
}

void update(InfoState& state, uint32_t nowMs) {
  if (!state.anim.running) {
    return;
  }
  if (nowMs - state.anim.startMs < kAnimMs) {
    return;
  }
  state.anim.running = false;
  if (state.anim.closing) {
    state.anim.visible = false;
  }
}

void drawConfirm(DisplayMonoTft& display, int16_t yOffset, const ConfirmState& state,
                 const char* title, const char* primaryLabel, const char* dangerLabel,
                 uint32_t nowMs) {
  const uint16_t shown = shownFixed(state.anim, nowMs);
  if (shown == 0) {
    return;
  }
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t displayWidth = static_cast<int16_t>(display.width());
  const int16_t displayHeight = static_cast<int16_t>(display.height());

  int16_t panelX = 0;
  int16_t panelY = 0;
  int16_t panelWidth = 0;
  int16_t panelHeight = 0;
  animatedPanelBounds(kConfirmPanelWidth, kConfirmPanelHeight, displayWidth, displayHeight, yOffset,
                      shown, panelX, panelY, panelWidth, panelHeight);
  drawPanel(display, panelX, panelY, panelWidth, panelHeight);

  // 内部元素随面板等比缩放、位置同步位移，文字保持最终字号（起始 85% 不会溢出）。
  const int16_t titleBaseline =
      static_cast<int16_t>(panelY + 30 * panelHeight / kConfirmPanelHeight);
  text.setFont(chinese_font_all);
  drawCenteredText(display, title, static_cast<int16_t>(displayWidth / 2), titleBaseline,
                   ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);

  const int16_t optionWidth = static_cast<int16_t>(kOptionWidth * panelWidth / kConfirmPanelWidth);
  const int16_t optionHeight = static_cast<int16_t>(kOptionHeight * panelHeight / kConfirmPanelHeight);
  const int16_t optionGap = static_cast<int16_t>(kOptionGap * panelWidth / kConfirmPanelWidth);
  const int16_t groupWidth = static_cast<int16_t>(optionWidth * 2 + optionGap);
  const int16_t groupX = static_cast<int16_t>(panelX + (panelWidth - groupWidth) / 2);
  const int16_t optionY = static_cast<int16_t>(panelY + 52 * panelHeight / kConfirmPanelHeight);

  for (uint8_t i = 0; i < 2U; ++i) {
    const bool danger = i == 1U;
    const bool selected = danger ? !state.primarySelected : state.primarySelected;
    const int16_t optionX = static_cast<int16_t>(groupX + i * (optionWidth + optionGap));
    DrawUtils::drawRoundRect(canvas, optionX, optionY, optionWidth, optionHeight, kOptionRadius,
                             selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK,
                             selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
    drawCenteredText(display, danger ? dangerLabel : primaryLabel,
                     static_cast<int16_t>(optionX + optionWidth / 2),
                     static_cast<int16_t>(optionY + optionHeight * 22 / kOptionHeight),
                     selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE,
                     selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  }
}

void drawInfo(DisplayMonoTft& display, int16_t yOffset, const InfoState& state, const char* title,
              const char* value, uint32_t nowMs) {
  const uint16_t shown = shownFixed(state.anim, nowMs);
  if (shown == 0) {
    return;
  }
  auto& text = display.text();
  const int16_t displayWidth = static_cast<int16_t>(display.width());
  const int16_t displayHeight = static_cast<int16_t>(display.height());

  int16_t panelX = 0;
  int16_t panelY = 0;
  int16_t panelWidth = 0;
  int16_t panelHeight = 0;
  animatedPanelBounds(kInfoPanelWidth, kInfoPanelHeight, displayWidth, displayHeight, yOffset, shown,
                      panelX, panelY, panelWidth, panelHeight);
  drawPanel(display, panelX, panelY, panelWidth, panelHeight);

  text.setFont(chinese_font_all);
  drawCenteredText(display, title, static_cast<int16_t>(displayWidth / 2),
                   static_cast<int16_t>(panelY + 34 * panelHeight / kInfoPanelHeight),
                   ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);

  text.setFont(u8g2_font_7x14B_tf);
  drawCenteredText(display, value, static_cast<int16_t>(displayWidth / 2),
                   static_cast<int16_t>(panelY + 74 * panelHeight / kInfoPanelHeight),
                   ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
}

}  // namespace PopupView
