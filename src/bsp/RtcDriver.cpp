#include "RtcDriver.h"

#include <Wire.h>

#include "BoardConfig.h"

namespace {
static_assert(BoardConfig::kPinIicSda == 47, "RTC I2C SDA must be GPIO47");
static_assert(BoardConfig::kPinIicScl == 48, "RTC I2C SCL must be GPIO48");
}  // namespace

bool RtcDriver::begin() {
  Wire.begin(BoardConfig::kPinIicSda, BoardConfig::kPinIicScl);
  Wire.setClock(400000);
  ready_ = rtc_.begin(Wire);
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
  return true;
}

bool RtcDriver::write(const DateTime& in) {
  if (!ready_) {
    return false;
  }
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
