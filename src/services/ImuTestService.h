#pragma once
#include <Arduino.h>
#include "../bsp/ImuDriver.h"
class ImuTestService {
public:
  enum class State : uint8_t { Idle, Powering, Sampling, Passed, Failed };
  bool begin();
  bool start(uint32_t nowMs); void tick(uint32_t nowMs); void cancel();
  State state() const; bool isBusy() const; bool consumeChanged(); bool hasPassed() const;
  uint8_t address() const; uint8_t whoAmI() const; uint32_t sampleCount() const;
  const ImuDriver::Sample& latest() const;
private:
  void finish(bool passed);
  ImuDriver driver_; State state_ = State::Idle; uint32_t nextMs_ = 0; uint32_t count_ = 0;
  bool changed_ = false; bool passed_ = false; ImuDriver::Sample first_{}, latest_{};
};
