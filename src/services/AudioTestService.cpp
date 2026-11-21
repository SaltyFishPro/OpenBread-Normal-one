#include "AudioTestService.h"

#include <Arduino.h>

#include "../bsp/PeripheralPower.h"
#include "../log/Log.h"

bool AudioTestService::begin() { return dac_.begin(); }

bool AudioTestService::start(uint32_t nowMs, PeripheralPower& power, bool musicBusy) {
  if (isActive()) {
    return false;
  }

  startMs_ = nowMs;
  lastDurationMs_ = 0;
  error_ = Error::None;
  heardSelected_ = true;

  if (musicBusy) {
    fail(Error::MusicBusy, nowMs);
    return false;
  }

  power_ = &power;
  audioWasEnabled_ = power.isAudioEnabled();
  if (!dac_.startPowerOn(nowMs)) {
    fail(Error::DacPower, nowMs);
    return false;
  }

  resourceHeld_ = true;
  state_ = State::Powering;
  changed_ = true;
  LogI("SELFTEST/AUDIO", "start freq=%u duration_ms=%u ldo_before=%d route=%s",
       static_cast<unsigned>(kToneFrequencyHz), static_cast<unsigned>(kToneDurationMs),
       audioWasEnabled_ ? 1 : 0, dac_.headphonesInserted() ? "headphone" : "speaker");
  return true;
}

void AudioTestService::tick(uint32_t nowMs) {
  if (state_ == State::Powering) {
    if (nowMs - startMs_ >= kTimeoutMs) {
      fail(Error::Timeout, nowMs);
      return;
    }
    if (!dac_.powerSettled(nowMs)) {
      return;
    }
    if (!tone_.start(nowMs, kToneFrequencyHz, kToneDurationMs)) {
      fail(Error::ToneInit, nowMs);
      return;
    }
    if (!dac_.finishPowerOn()) {
      fail(Error::DacPower, nowMs);
      return;
    }
    state_ = State::Playing;
    changed_ = true;
    LogI("SELFTEST/AUDIO", "tone playing route=%s",
         dac_.headphonesInserted() ? "headphone" : "speaker");
    return;
  }

  if (state_ != State::Playing) {
    return;
  }

  tone_.tick(nowMs);
  if (tone_.state() == I2sToneDriver::State::Error) {
    fail(Error::ToneWrite, nowMs);
    return;
  }
  if (tone_.state() == I2sToneDriver::State::Finished) {
    releaseAudio();
    state_ = State::WaitingConfirm;
    changed_ = true;
    LogI("SELFTEST/AUDIO", "waiting confirm");
    return;
  }
  if (nowMs - startMs_ >= kTimeoutMs) {
    fail(Error::Timeout, nowMs);
  }
}

void AudioTestService::toggleChoice() {
  if (state_ != State::WaitingConfirm) {
    return;
  }
  heardSelected_ = !heardSelected_;
  changed_ = true;
}

bool AudioTestService::confirm(uint32_t nowMs) {
  if (state_ != State::WaitingConfirm) {
    return false;
  }

  lastDurationMs_ = nowMs - startMs_;
  changed_ = true;
  if (heardSelected_) {
    state_ = State::Passed;
    LogI("SELFTEST", "finish item=AUDIO status=passed error=0 duration_ms=%lu",
         static_cast<unsigned long>(lastDurationMs_));
    return true;
  }

  error_ = Error::NotHeard;
  state_ = State::Failed;
  LogW("SELFTEST/AUDIO", "user not heard");
  LogI("SELFTEST", "finish item=AUDIO status=failed error=%u duration_ms=%lu",
       static_cast<unsigned>(errorCode(Error::NotHeard)),
       static_cast<unsigned long>(lastDurationMs_));
  return true;
}

void AudioTestService::cancel() {
  if (state_ == State::Idle) {
    return;
  }
  releaseAudio();
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
  LogI("SELFTEST/AUDIO", "cancel");
}

void AudioTestService::releaseAudio() {
  tone_.stop();
  if (resourceHeld_) {
    dac_.powerOff();
    if (power_ != nullptr) {
      power_->setAudioEnabled(audioWasEnabled_);
    }
  }
  resourceHeld_ = false;
  power_ = nullptr;
}

void AudioTestService::fail(Error error, uint32_t nowMs) {
  releaseAudio();
  error_ = error;
  lastDurationMs_ = nowMs - startMs_;
  state_ = State::Failed;
  changed_ = true;
  LogE("SELFTEST/AUDIO", "failed error=%u duration_ms=%lu", static_cast<unsigned>(errorCode(error)),
       static_cast<unsigned long>(lastDurationMs_));
  LogI("SELFTEST", "finish item=AUDIO status=failed error=%u duration_ms=%lu",
       static_cast<unsigned>(errorCode(error)), static_cast<unsigned long>(lastDurationMs_));
}

uint16_t AudioTestService::errorCode(Error error) {
  switch (error) {
    case Error::ToneInit:
      return 400;
    case Error::DacPower:
      return 401;
    case Error::ToneWrite:
      return 402;
    case Error::NotHeard:
      return 403;
    case Error::MusicBusy:
      return 404;
    case Error::Timeout:
      return 405;
    default:
      return 0;
  }
}

AudioTestService::State AudioTestService::state() const { return state_; }

AudioTestService::Error AudioTestService::error() const { return error_; }

bool AudioTestService::heardSelected() const { return heardSelected_; }

bool AudioTestService::isActive() const {
  return state_ == State::Powering || state_ == State::Playing;
}

bool AudioTestService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint32_t AudioTestService::lastDurationMs() const { return lastDurationMs_; }

bool AudioTestService::headphonesInserted() const { return dac_.headphonesInserted(); }
