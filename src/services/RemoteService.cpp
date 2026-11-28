#include "RemoteService.h"

#include "BluetoothService.h"

#include "../log/Log.h"

namespace {
constexpr uint32_t kFeedbackDurationMs = 360;
constexpr uint32_t kFeedbackFrameMs = 45;
constexpr uint8_t kCameraHapticEffect = 7;
constexpr uint16_t kCameraHapticMaxMs = 220;
}  // namespace

bool RemoteService::begin() {
  setState(State::Idle);
  triggerCount_ = 0;
  pagerState_ = PagerState::Idle;
  pagerUpCount_ = 0;
  pagerDownCount_ = 0;
  pagerPauseCount_ = 0;
  feedback_ = Feedback::None;
  changed_ = false;
  hapticActive_ = false;
  motorReady_ = false;
  return true;
}

void RemoteService::tick(uint32_t nowMs) {
  if (hapticActive_) {
    motor_.tick(nowMs);
    if (motor_.state() == MotorDriver::State::Finished ||
        motor_.state() == MotorDriver::State::Error) {
      hapticActive_ = false;
      if (motor_.state() == MotorDriver::State::Error) {
        LogW("REMOTE", "camera haptic failed error=%u", static_cast<unsigned>(motor_.error()));
      }
    }
  }

  if (feedback_ == Feedback::None) return;
  if (static_cast<int32_t>(nowMs - feedbackUntilMs_) >= 0) {
    feedback_ = Feedback::None;
    feedbackFrame_ = 0;
    changed_ = true;
    return;
  }
  if (nowMs - lastFeedbackFrameMs_ >= kFeedbackFrameMs) {
    lastFeedbackFrameMs_ = nowMs;
    ++feedbackFrame_;
    changed_ = true;
  }
}

bool RemoteService::triggerCameraShutter(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    setState(State::NotConnected);
    return false;
  }

  if (!bluetooth.triggerCameraShutter(nowMs)) {
    setState(State::Busy);
    return false;
  }

  ++triggerCount_;
  setState(State::Sent);
  setFeedback(Feedback::Camera, nowMs);
  if (!motorReady_) motorReady_ = motor_.begin();
  if (motorReady_ && motor_.playEffect(nowMs, kCameraHapticEffect, kCameraHapticMaxMs)) {
    hapticActive_ = true;
  } else {
    LogW("REMOTE", "camera haptic unavailable motor_state=%u",
         static_cast<unsigned>(motor_.state()));
  }
  LogI("REMOTE", "camera command sent count=%lu haptic=%d",
       static_cast<unsigned long>(triggerCount_), hapticActive_ ? 1 : 0);
  return true;
}

RemoteService::State RemoteService::state() const { return state_; }

uint32_t RemoteService::triggerCount() const { return triggerCount_; }

bool RemoteService::triggerPagerUp(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPageUp(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerUpCount_;
  pagerState_ = PagerState::UpSent;
  setFeedback(Feedback::PagerUp, nowMs);
  return true;
}

bool RemoteService::triggerPagerDown(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPageDown(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerDownCount_;
  pagerState_ = PagerState::DownSent;
  setFeedback(Feedback::PagerDown, nowMs);
  return true;
}

bool RemoteService::triggerPagerPause(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPlayPause(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerPauseCount_;
  pagerState_ = PagerState::Paused;
  setFeedback(Feedback::PagerPause, nowMs);
  return true;
}

RemoteService::PagerState RemoteService::pagerState() const { return pagerState_; }

uint32_t RemoteService::pagerUpCount() const { return pagerUpCount_; }

uint32_t RemoteService::pagerDownCount() const { return pagerDownCount_; }

uint32_t RemoteService::pagerPauseCount() const { return pagerPauseCount_; }

RemoteService::Feedback RemoteService::feedback() const { return feedback_; }

uint8_t RemoteService::feedbackFrame() const { return feedbackFrame_; }

bool RemoteService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void RemoteService::setFeedback(Feedback feedback, uint32_t nowMs) {
  feedback_ = feedback;
  feedbackUntilMs_ = nowMs + kFeedbackDurationMs;
  lastFeedbackFrameMs_ = nowMs;
  feedbackFrame_ = 0;
  changed_ = true;
}

void RemoteService::setState(State state) { state_ = state; }
