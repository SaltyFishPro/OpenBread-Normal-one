#include "IicBusScanner.h"

#include <Wire.h>

#include "BoardConfig.h"
#include "IicBus.h"

namespace {
constexpr uint32_t kScanClockHz = 100000;
}

void IicBusScanner::powerOn() {
  powerWasEnabled_ = digitalRead(BoardConfig::kPinSensorLdoEn) == HIGH;
  pinMode(BoardConfig::kPinSensorLdoEn, OUTPUT);
  digitalWrite(BoardConfig::kPinSensorLdoEn, HIGH);
}

bool IicBusScanner::beginBus() {
  return IicBus::begin(kScanClockHz, true);
}

void IicBusScanner::end() {
  // Wire is shared by RTC, MAX17048 and sensor tests. Keep it initialized.
  digitalWrite(BoardConfig::kPinSensorLdoEn, powerWasEnabled_ ? HIGH : LOW);
}

uint8_t IicBusScanner::probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission();
}
