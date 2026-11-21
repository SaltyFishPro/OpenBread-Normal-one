#include "MotorDriver.h"

#include <Arduino.h>
#include <Wire.h>

#include "BoardConfig.h"
#include "IicBus.h"
#include "../log/Log.h"

namespace {
constexpr uint8_t kAddress = 0x5A;  // DRV2605L 固定 7 位地址
constexpr uint8_t kRegMode = 0x01;
constexpr uint8_t kRegRtp = 0x02;
constexpr uint8_t kRegFeedback = 0x1A;
constexpr uint8_t kRegControl3 = 0x1D;
constexpr uint8_t kModeRtp = 0x05;
constexpr uint8_t kFeedbackErmLraMask = 0x80;
constexpr uint8_t kControl3ErmOpenLoop = 0x20;
constexpr uint8_t kRtpAmplitude = 0x50;  // 约 63%（有符号），固定中等强度
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint32_t kStartupMs = 1;  // 数据手册要求 EN 高后等待 >250µs
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

  LogI("MOTOR", "init en_pin=%d en=0 addr=0x%02X actuator=%s max_pulse_ms=%u",
       BoardConfig::kPinDrvEn, kAddress, kActuatorIsLra ? "LRA" : "ERM",
       static_cast<unsigned>(kMaxPulseMs));
  return true;
}

bool MotorDriver::startPulse(uint32_t nowMs, uint16_t durationMs) {
  if (state_ == State::Starting || state_ == State::Driving) {
    return false;
  }
  if (durationMs == 0U) {
    return false;
  }
  if (durationMs > kMaxPulseMs) {
    durationMs = kMaxPulseMs;
  }

  digitalWrite(BoardConfig::kPinDrvEn, HIGH);
  enabled_ = true;
  state_ = State::Starting;
  error_ = Error::None;
  startMs_ = nowMs;
  driveStartMs_ = 0;
  durationMs_ = durationMs;
  lastPulseMs_ = 0;
  changed_ = true;
  timerArmed_ = false;

  if (timer_ != nullptr) {
    (void)esp_timer_stop(timer_);
    timerArmed_ = esp_timer_start_once(timer_, static_cast<uint64_t>(durationMs) * 1000ULL) == ESP_OK;
    if (!timerArmed_) {
      LogE("MOTOR", "hard stop timer start failed; tick fallback enabled");
    }
  }

  LogI("MOTOR", "pulse start en=1 duration_ms=%u timer=%d", static_cast<unsigned>(durationMs),
       timerArmed_ ? 1 : 0);
  return true;
}

void MotorDriver::tick(uint32_t nowMs) {
  if (state_ == State::Starting) {
    if (nowMs - startMs_ < kStartupMs) {
      return;
    }
    if (!configureAndStart()) {
      LogE("MOTOR", "drv2605 config failed error=%u", static_cast<unsigned>(error_));
      disable();
      state_ = State::Error;
      changed_ = true;
      return;
    }
    driveStartMs_ = nowMs;
    state_ = State::Driving;
    changed_ = true;
    LogI("MOTOR", "driving rtp=0x%02X expected_ms=%u", kRtpAmplitude,
         static_cast<unsigned>(durationMs_ - (driveStartMs_ - startMs_)));
    return;
  }

  if (state_ != State::Driving) {
    return;
  }

  if (!enabled_) {
    finish(nowMs, true);
    return;
  }
  if (nowMs - startMs_ >= durationMs_) {
    disable();
    finish(nowMs, false);
  }
}

void MotorDriver::finish(uint32_t nowMs, bool byTimer) {
  (void)nowMs;
  if (driveStartMs_ != 0U) {
    const uint32_t startupMs = driveStartMs_ - startMs_;
    lastPulseMs_ = startupMs < durationMs_ ? durationMs_ - startupMs : 0U;
  }
  state_ = State::Finished;
  timerArmed_ = false;
  changed_ = true;
  LogI("MOTOR", "pulse end en=%d source=%s duration_ms=%lu",
       digitalRead(BoardConfig::kPinDrvEn) == HIGH ? 1 : 0, byTimer ? "timer" : "tick",
       static_cast<unsigned long>(lastPulseMs_));
}

bool MotorDriver::configureAndStart() {
  if (!IicBus::begin(kI2cClockHz, true)) {
    error_ = Error::BusInit;
    return false;
  }

  uint8_t feedback = 0;
  if (!readRegister(kRegFeedback, feedback)) {
    error_ = Error::NoAck;
    return false;
  }
  feedback = kActuatorIsLra ? static_cast<uint8_t>(feedback | kFeedbackErmLraMask)
                            : static_cast<uint8_t>(feedback & ~kFeedbackErmLraMask);
  if (!writeRegister(kRegFeedback, feedback)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  uint8_t control3 = 0;
  if (!readRegister(kRegControl3, control3)) {
    error_ = Error::NoAck;
    return false;
  }
  control3 = static_cast<uint8_t>(control3 | kControl3ErmOpenLoop);
  if (!writeRegister(kRegControl3, control3)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  // 一次写入同时退出 STANDBY 并进入 RTP 模式，随后写幅度即开始驱动。
  if (!writeRegister(kRegMode, kModeRtp) || !writeRegister(kRegRtp, kRtpAmplitude)) {
    error_ = Error::ConfigFailed;
    return false;
  }

  LogI("MOTOR", "drv2605 ready addr=0x%02X feedback=0x%02X control3=0x%02X amp=0x%02X",
       kAddress, feedback, control3, kRtpAmplitude);
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

void MotorDriver::stop() {
  if (state_ == State::Idle) {
    return;
  }

  disable();
  if (state_ == State::Driving && driveStartMs_ != 0U) {
    lastPulseMs_ = millis() - driveStartMs_;
  }
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
  LogI("MOTOR", "stop en=0 duration_ms=%lu", static_cast<unsigned long>(lastPulseMs_));
}

MotorDriver::State MotorDriver::state() const { return state_; }

MotorDriver::Error MotorDriver::error() const { return error_; }

bool MotorDriver::isActive() const {
  return state_ == State::Starting || state_ == State::Driving;
}

bool MotorDriver::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint32_t MotorDriver::lastPulseMs() const { return lastPulseMs_; }
