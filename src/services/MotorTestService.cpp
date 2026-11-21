#include "MotorTestService.h"

#include <Arduino.h>

#include "../log/Log.h"

bool MotorTestService::begin() { return motor_.begin(); }

bool MotorTestService::start(uint32_t nowMs) {
  if (isActive()) {
    return false;
  }

  error_ = Error::None;
  feltSelected_ = true;
  startMs_ = nowMs;
  lastDurationMs_ = 0;
  if (!motor_.startPulse(nowMs, kPulseMs)) {
    fail(Error::StartFailed, nowMs);
    return false;
  }

  state_ = State::Pulsing;
  changed_ = true;
  LogI("SELFTEST/MOTOR", "start duration_ms=%u", static_cast<unsigned>(kPulseMs));
  return true;
}

void MotorTestService::tick(uint32_t nowMs) {
  if (state_ != State::Pulsing) {
    return;
  }

  motor_.tick(nowMs);
  if (motor_.state() == MotorDriver::State::Error) {
    const Error mapped = motor_.error() == MotorDriver::Error::ConfigFailed
                             ? Error::ChipConfig
                             : Error::ChipNoResponse;
    fail(mapped, nowMs);
    return;
  }
  if (motor_.state() == MotorDriver::State::Finished) {
    lastDurationMs_ = nowMs - startMs_;
    state_ = State::WaitingConfirm;
    changed_ = true;
    LogI("SELFTEST/MOTOR", "pulse finished ring_ms=%lu", static_cast<unsigned long>(lastDurationMs_));
    return;
  }
  if (nowMs - startMs_ >= kTimeoutMs) {
    fail(Error::Timeout, nowMs);
  }
}

void MotorTestService::toggleChoice() {
  if (state_ != State::WaitingConfirm) {
    return;
  }
  feltSelected_ = !feltSelected_;
  changed_ = true;
}

bool MotorTestService::confirm(uint32_t nowMs) {
  if (state_ != State::WaitingConfirm) {
    return false;
  }

  lastDurationMs_ = nowMs - startMs_;
  changed_ = true;
  if (feltSelected_) {
    state_ = State::Passed;
    LogI("SELFTEST", "finish item=MOTOR status=passed error=0 duration_ms=%lu",
         static_cast<unsigned long>(lastDurationMs_));
    return true;
  }

  error_ = Error::NotFelt;
  state_ = State::Failed;
  LogW("SELFTEST/MOTOR", "user not felt");
  LogI("SELFTEST", "finish item=MOTOR status=failed error=%u duration_ms=%lu",
       static_cast<unsigned>(errorCode(Error::NotFelt)),
       static_cast<unsigned long>(lastDurationMs_));
  return true;
}

void MotorTestService::cancel() {
  if (state_ == State::Idle) {
    return;
  }
  motor_.stop();
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
  LogI("SELFTEST/MOTOR", "cancel");
}

void MotorTestService::fail(Error error, uint32_t nowMs) {
  motor_.stop();
  error_ = error;
  lastDurationMs_ = nowMs - startMs_;
  state_ = State::Failed;
  changed_ = true;
  LogE("SELFTEST/MOTOR", "failed error=%u duration_ms=%lu", static_cast<unsigned>(errorCode(error)),
       static_cast<unsigned long>(lastDurationMs_));
  LogI("SELFTEST", "finish item=MOTOR status=failed error=%u duration_ms=%lu",
       static_cast<unsigned>(errorCode(error)), static_cast<unsigned long>(lastDurationMs_));
}

uint16_t MotorTestService::errorCode(Error error) {
  switch (error) {
    case Error::StartFailed:
      return 430;
    case Error::NotFelt:
      return 431;
    case Error::Timeout:
      return 432;
    case Error::ChipNoResponse:
      return 433;
    case Error::ChipConfig:
      return 434;
    default:
      return 0;
  }
}

MotorTestService::State MotorTestService::state() const { return state_; }

MotorTestService::Error MotorTestService::error() const { return error_; }

bool MotorTestService::feltSelected() const { return feltSelected_; }

bool MotorTestService::isActive() const { return state_ == State::Pulsing; }

bool MotorTestService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint32_t MotorTestService::lastPulseMs() const { return motor_.lastPulseMs(); }

uint32_t MotorTestService::lastDurationMs() const { return lastDurationMs_; }
