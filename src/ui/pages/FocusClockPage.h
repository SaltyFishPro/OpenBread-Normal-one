#pragma once

#include <stdint.h>

class DisplayMonoTft;

class FocusClockPage {
public:
  struct Card {
    const char* titleZh;
    const char* const* options;
    uint8_t optionCount;
    uint8_t defaultOption;
  };

  static constexpr uint8_t kHomeIndex = 3;
  static constexpr uint8_t kMenuItemIndex = 0;
  static constexpr uint8_t kCardCount = 3;

  bool isSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool update(uint32_t nowMs);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         uint32_t nowMs);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, uint32_t nowMs) const;
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool isAnimating(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;

private:
  struct Animation {
    bool active = false;
    int8_t direction = 0;
    uint8_t fromIndex = 0;
    uint8_t toIndex = 0;
    uint32_t startMs = 0;
  };

  void moveCard(int8_t direction, uint32_t nowMs);
  void moveOption(int8_t direction, uint32_t nowMs);
  int16_t optionPosition(uint32_t nowMs) const;
  uint8_t optionIndexForCard(uint8_t cardIndex) const;
  void resetOptionForCard(uint8_t cardIndex);

  uint8_t cardIndex_ = 0;
  uint8_t optionIndex_[kCardCount] = {1, 1, 0};
  Animation cardAnimation_;
  int16_t optionAnimationFromPosition_ = 0;
  uint32_t optionAnimationStartMs_ = 0;
  bool optionAnimationActive_ = false;

  static constexpr uint32_t kCardAnimationMs = 360;
  static constexpr uint32_t kOptionAnimationMs = 100;
};
