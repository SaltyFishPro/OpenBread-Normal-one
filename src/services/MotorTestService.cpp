#include "MotorTestService.h"

#include <Arduino.h>

#include "../log/Log.h"

// DRV2605L LRA 库（库 6）中的常用效果；maxMs 是硬性截止时间。
const MotorTestService::Mode MotorTestService::kModes[MotorTestService::kModeCount] = {
    {"轻触", "Soft Bump", 7, 500},
    {"双击", "Double Click", 10, 700},
    {"强振", "Strong Buzz", 14, 1200},
    {"长振", "Long Buzz", 118, 1500},
    {"渐强", "Ramp Up", 82, 1200},
};

bool MotorTestService::begin() { return motor_.begin(); }

void MotorTestService::selectPrev() {
  if (isActive()) {
    return;
  }
  selectedIndex_ = selectedIndex_ == 0 ? static_cast<uint8_t>(kModeCount - 1U)
                                       : static_cast<uint8_t>(selectedIndex_ - 1U);
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
}

void MotorTestService::selectNext() {
  if (isActive()) {
    return;
  }
  selectedIndex_ = static_cast<uint8_t>((selectedIndex_ + 1U) % kModeCount);
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
}

bool MotorTestService::play(uint32_t nowMs) {
  if (isActive()) {
    return false;
  }

  error_ = Error::None;
  startMs_ = nowMs;
  lastPlayMs_ = 0;
  const Mode& mode = kModes[selectedIndex_];
  if (!motor_.playEffect(nowMs, mode.effectId, mode.maxMs)) {
    fail(Error::StartFailed, nowMs);
    return false;
  }

  state_ = State::Playing;
  changed_ = true;
  LogI("SELFTEST/MOTOR", "play mode=%u effect=%u max_ms=%u", static_cast<unsigned>(selectedIndex_),
       static_cast<unsigned>(mode.effectId), static_cast<unsigned>(mode.maxMs));
  return true;
}

void MotorTestService::tick(uint32_t nowMs) {
  if (state_ != State::Playing) {
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
    lastPlayMs_ = motor_.lastEffectMs();
    state_ = State::Finished;
    changed_ = true;
    LogI("SELFTEST/MOTOR", "play finished mode=%u effect_ms=%lu capped=%d",
         static_cast<unsigned>(selectedIndex_), static_cast<unsigned long>(lastPlayMs_),
         motor_.lastCapped() ? 1 : 0);
    return;
  }
  if (nowMs - startMs_ >= kModes[selectedIndex_].maxMs + kTimeoutSlackMs) {
    fail(Error::Timeout, nowMs);
  }
}

void MotorTestService::cancel() {
  if (state_ == State::Idle && !motor_.isActive()) {
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
  state_ = State::Failed;
  changed_ = true;
  LogE("SELFTEST/MOTOR", "failed error=%u duration_ms=%lu", static_cast<unsigned>(errorCode(error)),
       static_cast<unsigned long>(nowMs - startMs_));
  LogI("SELFTEST", "finish item=MOTOR status=failed error=%u duration_ms=%lu",
       static_cast<unsigned>(errorCode(error)), static_cast<unsigned long>(nowMs - startMs_));
}

uint16_t MotorTestService::errorCode(Error error) {
  switch (error) {
    case Error::StartFailed:
      return 430;
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

uint8_t MotorTestService::selectedIndex() const { return selectedIndex_; }

const char* MotorTestService::modeName(uint8_t index) const {
  return kModes[index < kModeCount ? index : selectedIndex_].name;
}

const char* MotorTestService::modeHint(uint8_t index) const {
  return kModes[index < kModeCount ? index : selectedIndex_].hint;
}

uint16_t MotorTestService::modeMaxMs(uint8_t index) const {
  return kModes[index < kModeCount ? index : selectedIndex_].maxMs;
}

bool MotorTestService::isActive() const { return state_ == State::Playing; }

bool MotorTestService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint32_t MotorTestService::lastPlayMs() const { return lastPlayMs_; }

bool MotorTestService::lastCapped() const { return motor_.lastCapped(); }
