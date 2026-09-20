#pragma once

#include <stdint.h>

class DisplayMonoTft;
class TimeService;
class WifiProvisionService;

class TimeCalibrationPage {
public:
  bool isTimeCalibrationSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  uint8_t detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool upEdge, bool downEdge,
                         bool okEdge, uint32_t nowMs, TimeService& timeService,
                         const WifiProvisionService& wifi);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, TimeService& timeService);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, const TimeService& timeService) const;
};
