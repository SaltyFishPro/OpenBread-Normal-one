#include "RtcTestService.h"

#ifndef OB_RTC_TEST_LOG_ENABLED
#define OB_RTC_TEST_LOG_ENABLED 1
#endif

#if OB_RTC_TEST_LOG_ENABLED
#define RTC_TEST_LOG(format, ...) Serial.printf("[SELFTEST][RTC] " format "\r\n", ##__VA_ARGS__)
#define RTC_TEST_ERROR(format, ...) \
  Serial.printf("[ERR][SELFTEST][RTC] " format "\r\n", ##__VA_ARGS__)
#else
#define RTC_TEST_LOG(format, ...) ((void)0)
#define RTC_TEST_ERROR(format, ...) ((void)0)
#endif

namespace {
constexpr uint32_t kSecondReadDelayMs = 1100;
constexpr uint8_t kControl1ExtTest = 1U << 7;
constexpr uint8_t kControl1Stop = 1U << 5;
constexpr uint8_t kControl1Cie = 1U << 2;
constexpr uint8_t kControl2Mi = 1U << 5;
constexpr uint8_t kControl2Hmi = 1U << 4;
constexpr uint8_t kControl2CofMask = 0x07U;
constexpr uint8_t kControl2ClockOutputOff = 0x07U;
constexpr uint8_t kOffsetFastMode = 1U << 7;
constexpr uint8_t kTimerClockMask = 0x18U;
constexpr uint8_t kTimerClockLowPower = 0x18U;
constexpr uint8_t kTimerEnable = 1U << 2;
}

bool RtcTestService::begin(RtcDriver& driver) {
  driver_ = &driver;
  return true;
}

bool RtcTestService::start(uint32_t nowMs) {
  if (isBusy()) {
    return false;
  }
  state_ = State::Idle;
  error_ = Error::None;
  warnings_ = None;
  diagnostics_ = RtcDriver::Diagnostics{};
  firstTime_ = RtcDriver::DateTime{};
  latestTime_ = RtcDriver::DateTime{};

  if (driver_ == nullptr || !driver_->isAvailable()) {
    fail(Error::Unavailable);
    return true;
  }
  if (!driver_->readDiagnostics(diagnostics_)) {
    fail(Error::RegisterRead);
    return true;
  }
  warnings_ = evaluateWarnings(diagnostics_);
  if (!driver_->read(firstTime_)) {
    fail(Error::FirstTimeRead);
    return true;
  }
  if (!validDateTime(firstTime_)) {
    fail(Error::InvalidTime);
    return true;
  }

  latestTime_ = firstTime_;
  secondReadAtMs_ = nowMs + kSecondReadDelayMs;
  state_ = State::Waiting;
  changed_ = true;
  RTC_TEST_LOG("start ctrl1=0x%02X ctrl2=0x%02X offset=0x%02X timer=0x%02X warnings=0x%03X",
               diagnostics_.control1, diagnostics_.control2, diagnostics_.offset,
               diagnostics_.timerMode, warnings_);
  return true;
}

void RtcTestService::tick(uint32_t nowMs) {
  if (state_ != State::Waiting || static_cast<int32_t>(nowMs - secondReadAtMs_) < 0) {
    return;
  }
  if (!driver_->read(latestTime_)) {
    fail(Error::SecondTimeRead);
    return;
  }
  if (!validDateTime(latestTime_)) {
    fail(Error::InvalidTime);
    return;
  }
  if (latestTime_.second == firstTime_.second) {
    fail(Error::NotAdvancing);
    return;
  }

  state_ = warnings_ == None ? State::Passed : State::Warning;
  changed_ = true;
  RTC_TEST_LOG("finish state=%s time=%02u:%02u:%02u warnings=0x%03X",
               state_ == State::Passed ? "passed" : "warning", latestTime_.hour,
               latestTime_.minute, latestTime_.second, warnings_);
}

void RtcTestService::cancel() {
  if (state_ == State::Waiting) {
    RTC_TEST_LOG("cancel");
  }
  state_ = State::Idle;
  error_ = Error::None;
  changed_ = true;
}

RtcTestService::State RtcTestService::state() const { return state_; }
RtcTestService::Error RtcTestService::error() const { return error_; }
uint16_t RtcTestService::warnings() const { return warnings_; }
const RtcDriver::Diagnostics& RtcTestService::diagnostics() const { return diagnostics_; }
const RtcDriver::DateTime& RtcTestService::latestTime() const { return latestTime_; }
bool RtcTestService::isBusy() const { return state_ == State::Waiting; }

bool RtcTestService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

bool RtcTestService::validDateTime(const RtcDriver::DateTime& value) const {
  return value.second <= 59U && value.minute <= 59U && value.hour <= 23U &&
         value.day >= 1U && value.day <= 31U && value.weekday <= 6U &&
         value.month >= 1U && value.month <= 12U;
}

uint16_t RtcTestService::evaluateWarnings(const RtcDriver::Diagnostics& value) const {
  uint16_t warnings = None;
  warnings |= (value.control1 & kControl1ExtTest) ? ExternalTestEnabled : None;
  warnings |= (value.control1 & kControl1Stop) ? ClockStopped : None;
  warnings |= value.oscillatorStopped ? OscillatorStopped : None;
  warnings |= (value.control2 & kControl2CofMask) != kControl2ClockOutputOff
                  ? ClockOutputEnabled : None;
  warnings |= (value.control2 & (kControl2Mi | kControl2Hmi)) != 0U
                  ? PeriodicInterruptEnabled : None;
  warnings |= (value.timerMode & kTimerEnable) ? TimerEnabled : None;
  warnings |= (value.timerMode & kTimerClockMask) != kTimerClockLowPower
                  ? TimerClockNotLowPower : None;
  warnings |= (value.offset & kOffsetFastMode) ? FastOffsetMode : None;
  warnings |= (value.control1 & kControl1Cie) ? CorrectionInterruptEnabled : None;
  return warnings;
}

void RtcTestService::fail(Error error) {
  error_ = error;
  state_ = State::Failed;
  changed_ = true;
  RTC_TEST_ERROR("failed stage=%u", static_cast<unsigned>(error));
}
