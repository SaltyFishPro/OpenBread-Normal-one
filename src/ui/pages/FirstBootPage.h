#pragma once

#include <stdint.h>

class DisplayMonoTft;
class FontSetupService;
class WifiProvisionService;
class TimeService;
class SdCardService;

class FirstBootPage {
public:
  void handleInput(bool up, bool down, bool left, bool ok, uint32_t nowMs,
                   FontSetupService& setup, SdCardService& sd,
                   WifiProvisionService& wifi, TimeService& time);
  void render(DisplayMonoTft& display, const FontSetupService& setup,
              const WifiProvisionService& wifi, const TimeService& time,
              uint8_t batteryPercent, bool batteryValid) const;
private:
  uint32_t errorSinceMs_ = 0;
  bool reconfigure_ = false;
};
