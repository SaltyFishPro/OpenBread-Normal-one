#include "PeripheralPower.h"

#include <Arduino.h>

#include "BoardConfig.h"

#ifndef OB_PERIPHERAL_POWER_LOG_ENABLED
#define OB_PERIPHERAL_POWER_LOG_ENABLED 1
#endif

bool PeripheralPower::begin() {
  pinMode(BoardConfig::kPinAudioLdoEn, OUTPUT);
  pinMode(BoardConfig::kPinSensorLdoEn, OUTPUT);
  digitalWrite(BoardConfig::kPinAudioLdoEn, LOW);
  digitalWrite(BoardConfig::kPinSensorLdoEn, HIGH);
#if OB_PERIPHERAL_POWER_LOG_ENABLED
  if (Serial) {
    Serial.printf("[POWER] init audio_ldo=0 sensor_ldo=1 iic_pullups=esp32s3\r\n");
  }
#endif
  return true;
}

void PeripheralPower::setAudioEnabled(bool enabled) {
  digitalWrite(BoardConfig::kPinAudioLdoEn, enabled ? HIGH : LOW);
}

void PeripheralPower::setSensorEnabled(bool enabled) {
  digitalWrite(BoardConfig::kPinSensorLdoEn, enabled ? HIGH : LOW);
}

bool PeripheralPower::isAudioEnabled() const {
  return digitalRead(BoardConfig::kPinAudioLdoEn) == HIGH;
}

bool PeripheralPower::isSensorEnabled() const {
  return digitalRead(BoardConfig::kPinSensorLdoEn) == HIGH;
}
