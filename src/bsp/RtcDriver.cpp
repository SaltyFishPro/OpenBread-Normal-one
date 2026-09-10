#include "RtcDriver.h"

#include <Wire.h>

#include <cstdarg>

#include "BoardConfig.h"
#include "IicBus.h"

namespace {
static_assert(BoardConfig::kPinIicSda == 47, "RTC I2C SDA must be GPIO47");
static_assert(BoardConfig::kPinIicScl == 48, "RTC I2C SCL must be GPIO48");

constexpr uint32_t kRtcIicClockHz = 100000;
constexpr uint32_t kBusSettleDelayMs = 20;
constexpr uint8_t kRegSeconds = 0x04;
constexpr uint8_t kRegControl1 = 0x00;
constexpr uint8_t kRegControl2 = 0x01;
constexpr uint8_t kRegOffset = 0x02;
constexpr uint8_t kRegTimerMode = 0x11;
constexpr uint8_t kSecondsClockIntegrityLostBit = 0x80;
constexpr uint8_t kControl2ClockOutputMask = 0x07;
constexpr uint8_t kControl2ClockOutputOff = 0x07;

#ifndef OB_RTC_LOG_ENABLED
#define OB_RTC_LOG_ENABLED 1
#endif

void rtcLog(const char* fmt, ...) {
#if OB_RTC_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[RTC] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void rtcErr(const char* fmt, ...) {
#if OB_RTC_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[ERR][RTC] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void prepareRtcIicBus(bool resetBus) {
  (void)IicBus::begin(kRtcIicClockHz, resetBus);
  if (resetBus) {
    delay(kBusSettleDelayMs);
  }
}
}  // namespace

bool RtcDriver::begin() {
  prepareRtcIicBus(true);
  ready_ = rtc_.begin(Wire);
  if (ready_) {
    uint8_t control2 = 0;
    const bool control2Read = rtc_.readRegister(kRegControl2, control2);
    const bool clkoutOff = control2Read &&
                           ((control2 & kControl2ClockOutputMask) == kControl2ClockOutputOff);
    if (control2Read && !clkoutOff) {
      const uint8_t disabledControl2 = static_cast<uint8_t>(
          (control2 & ~kControl2ClockOutputMask) | kControl2ClockOutputOff);
      if (!rtc_.writeRegister(kRegControl2, disabledControl2)) {
        rtcErr("failed to disable CLKOUT control2=0x%02X", control2);
      } else {
        control2 = disabledControl2;
      }
    }
    rtcLog("ready addr=0x51 sda=%d scl=%d battery_direct=1 clkout=%s control2=0x%02X clock=%lu",
           BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
           ((control2 & kControl2ClockOutputMask) == kControl2ClockOutputOff) ? "off" : "unknown",
           control2, static_cast<unsigned long>(kRtcIicClockHz));
  } else {
    rtcErr("begin failed addr=0x51 sda=%d scl=%d battery_direct=1 clock=%lu",
           BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
           static_cast<unsigned long>(kRtcIicClockHz));
  }
  return ready_;
}

void RtcDriver::sync() {
  if (!ready_) {
    return;
  }
  DateTime now;
  (void)read(now);
}

bool RtcDriver::isAvailable() const { return ready_; }

bool RtcDriver::read(DateTime& out) {
  if (!ready_) {
    return false;
  }
  prepareRtcIicBus(false);

  uint8_t secondsReg = 0;
  const bool secondsRegReady = rtc_.readRegister(kRegSeconds, secondsReg);

  Pcf85063DateTime raw;
  if (!rtc_.readDateTime(raw)) {
    return false;
  }
  out.second = raw.second;
  out.minute = raw.minute;
  out.hour = raw.hour;
  out.day = raw.day;
  out.weekday = raw.weekday;
  out.month = raw.month;
  out.year = raw.year;
  out.clockIntegrityLost =
      !secondsRegReady || ((secondsReg & kSecondsClockIntegrityLostBit) != 0U);
  return true;
}

bool RtcDriver::write(const DateTime& in) {
  if (!ready_) {
    return false;
  }
  prepareRtcIicBus(false);
  Pcf85063DateTime raw;
  raw.second = in.second;
  raw.minute = in.minute;
  raw.hour = in.hour;
  raw.day = in.day;
  raw.weekday = in.weekday;
  raw.month = in.month;
  raw.year = in.year;
  return rtc_.writeDateTime(raw);
}

bool RtcDriver::readDiagnostics(Diagnostics& out) {
  if (!ready_) {
    return false;
  }
  prepareRtcIicBus(false);
  uint8_t seconds = 0;
  if (!rtc_.readRegister(kRegControl1, out.control1) ||
      !rtc_.readRegister(kRegControl2, out.control2) ||
      !rtc_.readRegister(kRegOffset, out.offset) ||
      !rtc_.readRegister(kRegTimerMode, out.timerMode) ||
      !rtc_.readRegister(kRegSeconds, seconds)) {
    return false;
  }
  out.oscillatorStopped = (seconds & kSecondsClockIntegrityLostBit) != 0U;
  return true;
}
