#pragma once

#include <stdint.h>

#include "HomePage.h"

class DisplayMonoTft;

class FocusClockPage {
public:
  static constexpr uint8_t kHomeIndex = 3;
  static constexpr uint8_t kMenuItemIndex = 0;
  static constexpr uint8_t kOptionCount = 6;

  bool isSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool upEdge, bool downEdge,
                         bool okEdge, uint32_t nowMs);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, HomePage::Language language,
                    uint32_t nowMs) const;
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool isAnimating(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;

private:
  int8_t relativeIndex(uint8_t optionIndex) const;
  void moveSelection(int8_t direction, uint32_t nowMs);

  uint8_t selectedIndex_ = 0;
  int8_t animationDirection_ = 0;
  uint32_t animationStartMs_ = 0;

  static constexpr uint32_t kSelectionSlideMs = 170;
};
