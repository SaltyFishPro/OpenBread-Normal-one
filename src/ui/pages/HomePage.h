#pragma once

#include <Arduino.h>

#include "../../bsp/DisplayMonoTft.h"

class HomePage {
public:
  bool begin();
  bool handleInput(bool upEdge, bool downEdge, bool okEdge, uint32_t nowMs);
  void update(uint32_t nowMs);
  void render(DisplayMonoTft& display, int16_t pageOffsetX, uint32_t nowMs);
  void renderTransition(DisplayMonoTft& display, int16_t backgroundOffsetX,
                        int16_t menuBaseOffsetX, int16_t menuUpExtraOffsetX,
                        int16_t menuFocusExtraOffsetX, int16_t menuDownExtraOffsetX,
                        uint32_t nowMs);

  bool isSliding() const;
  bool hasAnimationTick(uint32_t nowMs) const;
  uint8_t focusIndex() const;
  const char* focusName() const;

private:
  enum class SlideState : uint8_t { Idle, Sliding };

  int16_t currentMenuOffset(uint32_t nowMs) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;
  void beginSlide(int8_t direction, uint32_t nowMs);

  SlideState slideState_ = SlideState::Idle;
  uint8_t focusIndex_ = 0;
  uint8_t targetIndex_ = 0;

  int16_t animFromOffsetY_ = 0;
  int16_t animToOffsetY_ = 0;
  uint32_t animStartMs_ = 0;

  uint16_t lastFocusFrame_ = 0;

  static constexpr uint8_t kMenuCount = 5;
  const char* menuNames_[kMenuCount] = {"设置", "音乐", "阅读", "时钟", "无线功能"};
};
