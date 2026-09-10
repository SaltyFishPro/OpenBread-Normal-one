#pragma once

#include <Arduino.h>

class BluetoothService;

class RemoteService {
public:
  enum class State : uint8_t {
    Idle,
    Ready,
    Sent,
    NotConnected,
    Busy,
    Error
  };

  bool begin();
  bool triggerCameraShutter(uint32_t nowMs, BluetoothService& bluetooth);

  State state() const;
  uint32_t triggerCount() const;

private:
  void setState(State state);

  State state_ = State::Idle;
  uint32_t triggerCount_ = 0;
};
