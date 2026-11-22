#include "MotorDriver.h"

#include <Arduino.h>
#include <Wire.h>

#include "BoardConfig.h"
#include "IicBus.h"
#include "../log/Log.h"

namespace {
constexpr uint8_t kAddress = 0x5A;  // DRV2605L 固定 7 位地址
constexpr uint8_t kRegMode = 0x01;
constexpr uint8_t kRegLibrary = 0x03;
constexpr uint8_t kRegWaveSeq1 = 0x04;
constexpr uint8_t kRegWaveSeq2 = 0x05;
constexpr uint8_t kRegGo = 0x0C;
constexpr uint8_t kRegFeedback = 0x1A;
constexpr uint8_t kRegControl3 = 0x1D;
constexpr uint8_t kModeInternalTrigger = 0x00;
constexpr uint8_t kFeedbackLraMask = 0x80;
constexpr uint8_t kControl3ErmOpenLoop = 0x20;
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint32_t kStartupMs = 1;    // 数据手册要求 EN 高后等待 >250µs
constexpr uint32_t kGoPollMs = 30;    // 轮询 GO 位判断播放是否结束
}  // namespace

bool MotorDriver::begin() {
  pinMode(BoardConfig::kPinDrvEn, OUTPUT);
  digitalWrite(BoardConfig::kPinDrvEn, LOW);
  state_ = State::Idle;
  error_ = Error::None;
  enabled_ = false;
  timerArmed_ = false;

  if (timer_ == nullptr) {
    esp_timer_create_args_t args = {};
    args.callback = &MotorDriver::timerCallback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "motor_stop";
    if (esp_timer_create(&args, &timer_) != ESP_OK) {
      timer_ = nullptr;
      LogE("MOTOR", "hard stop timer create failed; tick fallback enabled");
    }
  }

  LogI("MOTOR", "init en_pin=%d en=0 addr=0x%02X actuator=LRA library=%u",
       BoardConfig::kPinDrvEn, kAddress, kLibraryLra);
  return true;
}

bool MotorDriver::playEffect(uint32_t nowMs, uint8_t effectId, uint16_t maxMs) {
  if (state_ == State::Starting || state_ == State::Playing) {
    return false;
  }
  if (effectId == 0U || maxMs == 0U) {
    return false;
  }
  if (maxMs > kMaxEffectMs) {
    maxMs = kMaxEffectMs;
  }

  digitalWrite(BoardConfig::kPinDrvEn, HIGH);
  enabled_ = true;
  state_ = State::Starting;
  error_ = Error::None;
  startMs_ = nowMs;
  playStartMs_ = 0;
  nextPollMs_ = 0;
  maxMs_ = maxMs;
  effectId_ = effectId;
  lastEffectMs_ = 0;
  lastCapped_ = false;
  changed_ = true;
  timerArmed_ = false;

  if (timer_ != nullptr) {
    (void)esp_timer_stop(timer_);
    timerArmed_ = esp_timer_start_once(timer_, static_cast<uint64_t>(maxMs) * 1000ULL) == ESP_OK;
    if (!timerArmed_) {
      LogE("MOTOR", "hard stop timer start failed; tick fallback enabled");
    }
  }

  LogI("MOTOR", "effect start id=%u max_ms=%u timer=%d", static_cast<unsigned>(effectId),
       static_cast<unsigned>(maxMs), timerArmed_ ? 1 : 0);
  return true;
}

void MotorDriver::tick(uint32_t nowMs) {
  if (state_ == State::Starting) {
    if (nowMs - startMs_ < kStartupMs) {
      return;
    }
    if (!configureAndPlay()) {
      LogE("MOTOR", "drv2605 config failed error=%u", static_cast<unsigned>(error_));
      disable();
      state_ = State::Error;
      changed_ = true;
      return;
    }
    playStartMs_ = nowMs;
    nextPollMs_ = nowMs + kGoPollMs;
    state_ = State::Playing;
    changed_ = true;
    LogI("MOTOR", "playing id=%u library=%u lra=1", static_cast<unsigned>(effectId_),
         kLibraryLra);
    return;
  }

  if (state_ != State::Playing) {
    return;
  }

  if (!enabled_) {
    finish(nowMs, true);
    return;
  }

  if (nowMs - startMs_ >= maxMs_) {
    disable();
    finish(nowMs, true);
    return;
  }

  if (static_cast<int32_t>(nowMs - nextPollMs_) < 0) {
    return;
  }
  nextPollMs_ = nowMs + kGoPollMs;

  uint8_t go = 0;
  if (!readRegister(kRegGo, go)) {
    error_ = Error::NoAck;
    disable();
    state_ = State::Error;
    changed_ = true;
    LogE("MOTOR", "GO read failed id=%u", static_cast<unsigned>(effectId_));
    return;
  }
  if ((go & 0x01U) == 0U) {
    disable();
    finish(nowMs, false);
  }
}

bool MotorDriver::configureAndPlay() {
  if (!IicBus::begin(kI2cClockHz, true)) {
    error_ = Error::BusInit;
    return false;
  }

  uint8_t feedback = 0;
  if (!readRegister(kRegFeedback, feedback)) {
    error_ = Error::NoAck;
    return false;
  }
  feedback = static_cast<uint8_t>(feedback | kFeedbackLraMask);
  if (!writeRegister(kRegFeedback, feedback)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  // LRA 走闭环自动谐振；清掉 ERM 开环位，避免复用旧配置。
  uint8_t control3 = 0;
  if (!readRegister(kRegControl3, control3)) {
    error_ = Error::NoAck;
    return false;
  }
  control3 = static_cast<uint8_t>(control3 & ~kControl3ErmOpenLoop);
  if (!writeRegister(kRegControl3, control3)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  // 退出 STANDBY，进入内部触发模式；清除可能残留的 GO，再装载库与波形。
  if (!writeRegister(kRegMode, kModeInternalTrigger)) {
    error_ = Error::ConfigFailed;
    return false;
  }
  if (!writeRegister(kRegGo, 0x00)) {
    error_ = Error::ConfigFailed;
    return false;
  }
  if (!writeRegister(kRegLibrary, kLibraryLra)) {
    error_ = Error::ConfigFailed;
    return false;
  }
  if (!writeRegister(kRegWaveSeq1, effectId_)) {
    error_ = Error::ConfigFailed;
    return false;
  }
  if (!writeRegister(kRegWaveSeq2, 0x00)) {
    error_ = Error::ConfigFailed;
    return false;
  }
  if (!writeRegister(kRegGo, 0x01)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  LogI("MOTOR", "drv2605 ready addr=0x%02X library=%u id=%u feedback=0x%02X control3=0x%02X",
       kAddress, kLibraryLra, static_cast<unsigned>(effectId_), feedback, control3);
  return true;
}

bool MotorDriver::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool MotorDriver::readRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(kAddress), 1) != 1) {
    return false;
  }
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

void MotorDriver::disable() {
  if (timer_ != nullptr) {
    (void)esp_timer_stop(timer_);
  }
  timerArmed_ = false;
  if (enabled_) {
    // 取消仍在播放的序列，并清掉 GO，避免下次触发被旧状态阻塞。
    (void)writeRegister(kRegGo, 0x00);
  }
  digitalWrite(BoardConfig::kPinDrvEn, LOW);
  enabled_ = false;
}

void MotorDriver::timerCallback(void* arg) {
  static_cast<MotorDriver*>(arg)->stopFromTimer();
}

void MotorDriver::stopFromTimer() {
  digitalWrite(BoardConfig::kPinDrvEn, LOW);
  enabled_ = false;
  timerArmed_ = false;
}

void MotorDriver::finish(uint32_t nowMs, bool capped) {
  if (playStartMs_ != 0U) {
    lastEffectMs_ = nowMs - playStartMs_;
  }
  lastCapped_ = capped;
  state_ = State::Finished;
  timerArmed_ = false;
  changed_ = true;
  LogI("MOTOR", "effect end id=%u measured_ms=%lu capped=%d",
       static_cast<unsigned>(effectId_), static_cast<unsigned long>(lastEffectMs_),
       capped ? 1 : 0);
}

void MotorDriver::stop() {
  if (state_ == State::Idle) {
    return;
  }

  disable();
  if (state_ == State::Playing && playStartMs_ != 0U) {
    lastEffectMs_ = millis() - playStartMs_;
  }
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
  LogI("MOTOR", "stop en=0 effect_ms=%lu", static_cast<unsigned long>(lastEffectMs_));
}

MotorDriver::State MotorDriver::state() const { return state_; }

MotorDriver::Error MotorDriver::error() const { return error_; }

bool MotorDriver::isActive() const {
  return state_ == State::Starting || state_ == State::Playing;
}

bool MotorDriver::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint32_t MotorDriver::lastEffectMs() const { return lastEffectMs_; }

bool MotorDriver::lastCapped() const { return lastCapped_; }
