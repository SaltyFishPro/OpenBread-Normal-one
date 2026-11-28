#pragma once

#include <Arduino.h>

#include "../bsp/MotorDriver.h"

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

  enum class Feedback : uint8_t { None, Camera, PagerUp, PagerDown, PagerPause };

  bool begin();
  void tick(uint32_t nowMs);
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
  Feedback feedback() const;
  uint8_t feedbackFrame() const;
  bool consumeChanged();

private:
  void setState(State state);
  void setFeedback(Feedback feedback, uint32_t nowMs);

  MotorDriver motor_;
  State state_ = State::Idle;
  uint32_t triggerCount_ = 0;
  PagerState pagerState_ = PagerState::Idle;
  uint32_t pagerUpCount_ = 0;
  uint32_t pagerDownCount_ = 0;
  uint32_t pagerPauseCount_ = 0;
  Feedback feedback_ = Feedback::None;
  uint32_t feedbackUntilMs_ = 0;
  uint32_t lastFeedbackFrameMs_ = 0;
  uint8_t feedbackFrame_ = 0;
  bool changed_ = false;
  bool hapticActive_ = false;
  bool motorReady_ = false;
};
