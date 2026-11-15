#pragma once

#include <stdint.h>

// Persist the settings shared by the device UI and the management service.
class DeviceSettings {
public:
  struct Values {
    uint32_t magic = 0x4f425331U;
    uint32_t sleepSeconds = 20;
    uint32_t adminTimeoutSeconds = 600;
    char name[33] = "OpenBread Normal One";
  };

  bool begin();
  bool save(const Values& values);
  const Values& values() const { return values_; }
  static bool valid(const Values& values);

private:
  Values values_{};
};
