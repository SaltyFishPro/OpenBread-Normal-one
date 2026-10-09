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

  enum class PagerState : uint8_t {
    Idle,
    UpSent,
    DownSent,
    Paused,
    NotConnected,
    Busy,
    Error
  };

  bool begin();
  bool triggerCameraShutter(uint32_t nowMs, BluetoothService& bluetooth);
  bool triggerPagerUp(uint32_t nowMs, BluetoothService& bluetooth);
  bool triggerPagerDown(uint32_t nowMs, BluetoothService& bluetooth);
  bool triggerPagerPause(uint32_t nowMs, BluetoothService& bluetooth);

  State state() const;
  uint32_t triggerCount() const;
  PagerState pagerState() const;
  uint32_t pagerUpCount() const;
  uint32_t pagerDownCount() const;
  uint32_t pagerPauseCount() const;

private:
  void setState(State state);

  State state_ = State::Idle;
  uint32_t triggerCount_ = 0;
  PagerState pagerState_ = PagerState::Idle;
  uint32_t pagerUpCount_ = 0;
  uint32_t pagerDownCount_ = 0;
  uint32_t pagerPauseCount_ = 0;
};
