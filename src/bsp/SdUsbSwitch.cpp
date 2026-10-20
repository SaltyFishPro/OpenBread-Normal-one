#include "SdUsbSwitch.h"

#include <Arduino.h>
#include "BoardConfig.h"

#ifndef OB_SD_USB_LOG_ENABLED
#define OB_SD_USB_LOG_ENABLED 1
#endif

void SdUsbSwitch::begin() {
  pinMode(BoardConfig::kPinSdUsbSelect, OUTPUT);
  digitalWrite(BoardConfig::kPinSdUsbSelect, LOW);
  enabled_ = false;
#if OB_SD_USB_LOG_ENABLED
  if (Serial) Serial.printf("[SDUSB] init GPIO%d=LOW normal USB route\n",
                            BoardConfig::kPinSdUsbSelect);
#endif
}

void SdUsbSwitch::setEnabled(bool enabled) {
  if (enabled == enabled_) return;
  // Log before switching: the HIGH route may disconnect the ESP32 USB serial port.
#if OB_SD_USB_LOG_ENABLED
  if (Serial) Serial.printf("[SDUSB] GPIO%d=%s %s\n", BoardConfig::kPinSdUsbSelect,
                            enabled ? "HIGH" : "LOW",
                            enabled ? "SD_MMC released; enable USB-to-SD bridge"
                                    : "restore normal USB; SD mounts on next use");
#endif
  digitalWrite(BoardConfig::kPinSdUsbSelect, enabled ? HIGH : LOW);
  enabled_ = enabled;
}
