#pragma once

#include <stdint.h>

#include "../bsp/Max17048Driver.h"

class PeripheralPower;

class PowerDiagnosticService {
public:
  enum class State : uint8_t { Idle, Passed, Warning, Failed };
  enum class VoltageState : uint8_t { Normal, Low, High };

  static constexpr uint16_t kBatteryNominalVoltageMv = 3800;
  static constexpr uint16_t kBatteryChargeVoltageMv = 4350;
  static constexpr uint16_t kBatteryCapacityMah = 800;
  static constexpr uint16_t kMinimumVoltageMv = 3000;
  static constexpr uint16_t kMaximumVoltageMv = 4400;

  bool run(PeripheralPower& peripheralPower);
  State state() const;
  VoltageState voltageState() const;
  const Max17048Driver::Reading& reading() const;
  uint16_t estimatedRemainingMah() const;
  int32_t estimatedCurrentMaX10() const;
  bool audioLdoEnabled() const;
  bool sensorLdoEnabled() const;
  bool consumeChanged();

private:
  Max17048Driver driver_;
  Max17048Driver::Reading reading_{};
  State state_ = State::Idle;
  VoltageState voltageState_ = VoltageState::Normal;
  bool audioLdoEnabled_ = false;
  bool sensorLdoEnabled_ = false;
  bool changed_ = false;
};
