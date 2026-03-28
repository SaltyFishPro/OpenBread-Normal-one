#pragma once

#include <Arduino.h>

#include "../bsp/DisplayMonoTft.h"
#include "Render1bpp.h"
#include "pages/HomePage.h"

class UiManager {
public:
  bool begin();
  void tick();

private:
  enum class UiState : uint8_t {
    Home,
    ToSectionTransition,
    ToHomeTransition,
    Section,
    Detail
  };

  struct ButtonEdge {
    uint8_t pin;
    bool stablePressed;
    bool lastRawPressed;
    uint32_t lastRawChangeMs;
  };

  struct InputEdges {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool ok = false;
  };

  InputEdges pollInputEdges();
  void updateState(const InputEdges& edges, uint32_t nowMs);
  bool shouldRedraw(uint32_t nowMs) const;
  void render(uint32_t nowMs);
  void renderSection(int16_t xOffset, int16_t yOffset = 0);
  void renderDetail();
  int16_t easeInCubic(int16_t from, int16_t to, float t) const;
  bool isPressed(uint8_t pin) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;

  DisplayMonoTft display_;
  Render1bpp renderer_;
  HomePage homePage_;
  UiState state_ = UiState::Home;

  uint32_t transitionStartMs_ = 0;
  uint8_t sectionFocusIndex_ = 0;
  bool needsRedraw_ = true;

  ButtonEdge buttons_[5] = {
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0}};
};
