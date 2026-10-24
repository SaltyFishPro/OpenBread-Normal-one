#include "SdUsbSwitch.h"

#include <Arduino.h>
#include "BoardConfig.h"
#include "../log/Log.h"

void SdUsbSwitch::begin() {
  pinMode(BoardConfig::kPinSdUsbSelect, OUTPUT);
  digitalWrite(BoardConfig::kPinSdUsbSelect, LOW);
  enabled_ = false;
  LogI("SDUSB", "init GPIO%d=LOW normal USB route", BoardConfig::kPinSdUsbSelect);
}

void SdUsbSwitch::setEnabled(bool enabled) {
  if (enabled == enabled_) return;
  // Log before switching: the HIGH route may disconnect the ESP32 USB serial port.
  LogI("SDUSB", "GPIO%d=%s %s", BoardConfig::kPinSdUsbSelect, enabled ? "HIGH" : "LOW",
       enabled ? "SD_MMC released; enable USB-to-SD bridge"
               : "restore normal USB; SD mounts on next use");
  digitalWrite(BoardConfig::kPinSdUsbSelect, enabled ? HIGH : LOW);
  enabled_ = enabled;
}
