#pragma once

#include <stdint.h>

#include "HomePage.h"

class DisplayMonoTft;

class GamesPage {
public:
  static constexpr uint8_t kHomeIndex = 5;
  static constexpr uint8_t kWoodenFishItemIndex = 0;

  bool isWoodenFishSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge, bool okPressed,
                         bool okChanged, uint32_t nowMs);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, HomePage::Language language) const;

private:
  void registerKnock(uint32_t nowMs);

  uint32_t knockCount_ = 0;
  uint32_t messageSeed_ = 0;
  uint8_t lastMessageIndex_ = 0;
  bool hasMessage_ = false;
  bool iconPressed_ = false;
};
