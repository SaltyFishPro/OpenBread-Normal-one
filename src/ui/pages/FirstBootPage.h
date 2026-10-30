#pragma once

#include <stdint.h>

class DisplayMonoTft;
class FontSetupService;
class WifiProvisionService;
class TimeService;

class FirstBootPage {
public:
  void render(DisplayMonoTft& display, const FontSetupService& setup,
              const WifiProvisionService& wifi, const TimeService& time,
              uint8_t batteryPercent, bool batteryValid) const;
};
