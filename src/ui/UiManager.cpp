#include "UiManager.h"

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
}  // namespace

bool UiManager::begin() {
  buttons_[0].pin = static_cast<uint8_t>(BoardConfig::kPinBtnLeft);
  buttons_[1].pin = static_cast<uint8_t>(BoardConfig::kPinBtnRight);
  buttons_[2].pin = static_cast<uint8_t>(BoardConfig::kPinBtnUp);
  buttons_[3].pin = static_cast<uint8_t>(BoardConfig::kPinBtnDown);
  buttons_[4].pin = static_cast<uint8_t>(BoardConfig::kPinBtnOk);

  for (auto& button : buttons_) {
    pinMode(button.pin, INPUT_PULLUP);
    button.lastPressed = isPressed(button.pin);
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

  const bool leftPressed = isPressed(buttons_[0].pin);
  const bool rightPressed = isPressed(buttons_[1].pin);
  const bool upPressed = isPressed(buttons_[2].pin);
  const bool downPressed = isPressed(buttons_[3].pin);
  const bool okPressed = isPressed(buttons_[4].pin);

  edges.left = leftPressed && !buttons_[0].lastPressed;
  edges.right = rightPressed && !buttons_[1].lastPressed;
  edges.up = upPressed && !buttons_[2].lastPressed;
  edges.down = downPressed && !buttons_[3].lastPressed;
  edges.ok = okPressed && !buttons_[4].lastPressed;

  buttons_[0].lastPressed = leftPressed;
  buttons_[1].lastPressed = rightPressed;
  buttons_[2].lastPressed = upPressed;
  buttons_[3].lastPressed = downPressed;
  buttons_[4].lastPressed = okPressed;

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
      if (edges.left) {
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
    homePage_.render(display_, 0, false, nowMs);
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
                               menuFocusOffsetX, menuDownOffsetX, false, nowMs);
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
                               menuFocusOffsetX, menuDownOffsetX, false, nowMs);
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
  const int16_t height = static_cast<int16_t>(display_.height());

  canvas.drawFilledRectangle(xOffset, yOffset, xOffset + width - 1,
                             static_cast<int16_t>(yOffset + ThemeMono::kHeaderHeight),
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_6x12_mf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(xOffset + 6, static_cast<int16_t>(yOffset + 24), "SECTION");

  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_8x13B_mf);
  text.drawUTF8(xOffset + 8, static_cast<int16_t>(yOffset + 52), homePage_.focusName());

  text.setFont(u8g2_font_6x12_mf);
  text.drawUTF8(xOffset + 8, static_cast<int16_t>(yOffset + 78), "1. Item A");
  text.drawUTF8(xOffset + 8, static_cast<int16_t>(yOffset + 100), "2. Item B");
  text.drawUTF8(xOffset + 8, static_cast<int16_t>(yOffset + 122), "3. Item C");
  text.drawUTF8(xOffset + width - 118, static_cast<int16_t>(yOffset + height - 10),
                "OK: Detail");

  canvas.drawRectangle(xOffset + 4, static_cast<int16_t>(yOffset + 32),
                       xOffset + width - 5, static_cast<int16_t>(yOffset + height - 5),
                       ST7305_COLOR_BLACK);
}

void UiManager::renderDetail() {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());

  canvas.drawFilledRectangle(0, 0, width - 1, ThemeMono::kHeaderHeight,
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_6x12_mf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(6, 24, "DETAIL");

  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_8x13B_mf);
  text.drawUTF8(8, 62, homePage_.focusName());
  text.setFont(u8g2_font_6x12_mf);
  text.drawUTF8(8, 92, "Detail content placeholder");
  text.drawUTF8(width - 102, height - 10, "LEFT: Back");

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
