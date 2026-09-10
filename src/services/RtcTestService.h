#pragma once

#include <Arduino.h>

#include "../bsp/RtcDriver.h"

class RtcTestService {
public:
  enum class State : uint8_t { Idle, Waiting, Passed, Warning, Failed };

  enum Warning : uint16_t {
    None = 0,
    ExternalTestEnabled = 1U << 0,
    ClockStopped = 1U << 1,
    OscillatorStopped = 1U << 2,
    ClockOutputEnabled = 1U << 3,
    PeriodicInterruptEnabled = 1U << 4,
    TimerEnabled = 1U << 5,
    TimerClockNotLowPower = 1U << 6,
    FastOffsetMode = 1U << 7,
    CorrectionInterruptEnabled = 1U << 8
  };

  enum class Error : uint8_t { None, Unavailable, RegisterRead, FirstTimeRead, SecondTimeRead, InvalidTime, NotAdvancing };

  bool begin(RtcDriver& driver);
  bool start(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void cancel();
  State state() const;
  Error error() const;
  uint16_t warnings() const;
  const RtcDriver::Diagnostics& diagnostics() const;
  const RtcDriver::DateTime& latestTime() const;
  bool isBusy() const;
  bool consumeChanged();

private:
  bool validDateTime(const RtcDriver::DateTime& value) const;
  uint16_t evaluateWarnings(const RtcDriver::Diagnostics& value) const;
  void fail(Error error);

  RtcDriver* driver_ = nullptr;
  State state_ = State::Idle;
  Error error_ = Error::None;
  RtcDriver::Diagnostics diagnostics_{};
  RtcDriver::DateTime firstTime_{};
  RtcDriver::DateTime latestTime_{};
  uint32_t secondReadAtMs_ = 0;
  uint16_t warnings_ = None;
  bool changed_ = false;
};
