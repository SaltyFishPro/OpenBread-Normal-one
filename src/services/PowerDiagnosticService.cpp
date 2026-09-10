#include "PowerDiagnosticService.h"

#include <Arduino.h>

#include "../bsp/PeripheralPower.h"

#ifndef OB_POWER_DIAGNOSTIC_LOG_ENABLED
#define OB_POWER_DIAGNOSTIC_LOG_ENABLED 1
#endif

#if OB_POWER_DIAGNOSTIC_LOG_ENABLED
#define POWER_LOG(format, ...) \
  Serial.printf("[SELFTEST][POWER] " format "\r\n", ##__VA_ARGS__)
#define POWER_ERROR(format, ...) \
  Serial.printf("[ERR][SELFTEST][POWER] " format "\r\n", ##__VA_ARGS__)
#else
#define POWER_LOG(format, ...) ((void)0)
#define POWER_ERROR(format, ...) ((void)0)
#endif

bool PowerDiagnosticService::run(PeripheralPower& peripheralPower) {
  audioLdoEnabled_ = peripheralPower.isAudioEnabled();
  sensorLdoEnabled_ = peripheralPower.isSensorEnabled();
  reading_ = Max17048Driver::Reading{};
  if (!driver_.read(reading_)) {
    state_ = State::Failed;
    changed_ = true;
    POWER_ERROR("MAX17048 read failed address=0x36");
    return false;
  }

  const bool versionValid = reading_.version != 0U && reading_.version != 0xFFFFU;
  if (reading_.voltageMv < kMinimumVoltageMv) {
    voltageState_ = VoltageState::Low;
  } else if (reading_.voltageMv > kMaximumVoltageMv) {
    voltageState_ = VoltageState::High;
  } else {
    voltageState_ = VoltageState::Normal;
  }
  const bool voltagePlausible = voltageState_ == VoltageState::Normal;
  const bool socPlausible = reading_.stateOfChargeX100 <= 10500U;
  state_ = versionValid && voltagePlausible && socPlausible ? State::Passed : State::Warning;
  changed_ = true;
  POWER_LOG("battery=3800mV/4350mV/800mAh version=0x%04X voltage_mv=%u soc_x100=%u remaining_mah=%u rate_x100=%ld estimated_current_x10_ma=%ld status=0x%04X audio_ldo=%u sensor_ldo=%u result=%s",
            reading_.version, reading_.voltageMv, reading_.stateOfChargeX100,
            estimatedRemainingMah(), static_cast<long>(reading_.chargeRateX100),
            static_cast<long>(estimatedCurrentMaX10()), reading_.status,
            audioLdoEnabled_ ? 1U : 0U,
            sensorLdoEnabled_ ? 1U : 0U, state_ == State::Passed ? "passed" : "warning");
  return true;
}

PowerDiagnosticService::State PowerDiagnosticService::state() const { return state_; }
PowerDiagnosticService::VoltageState PowerDiagnosticService::voltageState() const {
  return voltageState_;
}
const Max17048Driver::Reading& PowerDiagnosticService::reading() const { return reading_; }
uint16_t PowerDiagnosticService::estimatedRemainingMah() const {
  const uint32_t capacity = static_cast<uint32_t>(reading_.stateOfChargeX100) *
                            kBatteryCapacityMah;
  const uint16_t estimated = static_cast<uint16_t>((capacity + 5000U) / 10000U);
  return estimated > kBatteryCapacityMah ? kBatteryCapacityMah : estimated;
}
int32_t PowerDiagnosticService::estimatedCurrentMaX10() const {
  return (reading_.chargeRateX100 * static_cast<int32_t>(kBatteryCapacityMah)) / 1000L;
}
bool PowerDiagnosticService::audioLdoEnabled() const { return audioLdoEnabled_; }
bool PowerDiagnosticService::sensorLdoEnabled() const { return sensorLdoEnabled_; }
bool PowerDiagnosticService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}
