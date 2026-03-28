#include "Pcf85063Rtc.h"

Pcf85063Rtc::Pcf85063Rtc(uint8_t i2cAddress) : address_(i2cAddress) {}

bool Pcf85063Rtc::begin(TwoWire& wire) {
  wire_ = &wire;
  if (!isConnected()) {
    return false;
  }

  uint8_t ctrl1 = 0;
  if (!readRegister(kRegControl1, ctrl1)) {
    return false;
  }
  if (ctrl1 & kControl1StopBit) {
    ctrl1 = static_cast<uint8_t>(ctrl1 & ~kControl1StopBit);
    if (!writeRegister(kRegControl1, ctrl1)) {
      return false;
    }
  }

  return true;
}

bool Pcf85063Rtc::isConnected() const {
  if (wire_ == nullptr) {
    return false;
  }

  wire_->beginTransmission(address_);
  return wire_->endTransmission() == 0;
}

bool Pcf85063Rtc::readDateTime(Pcf85063DateTime& out) const {
  uint8_t raw[7] = {0};
  if (!readRegisters(kRegSeconds, raw, sizeof(raw))) {
    return false;
  }

  out.second = bcdToDec(static_cast<uint8_t>(raw[0] & 0x7F));
  out.minute = bcdToDec(static_cast<uint8_t>(raw[1] & 0x7F));
  out.hour = bcdToDec(static_cast<uint8_t>(raw[2] & 0x3F));
  out.day = bcdToDec(static_cast<uint8_t>(raw[3] & 0x3F));
  out.weekday = bcdToDec(static_cast<uint8_t>(raw[4] & 0x07));
  out.month = bcdToDec(static_cast<uint8_t>(raw[5] & 0x1F));
  out.year = bcdToDec(raw[6]);
  return true;
}

bool Pcf85063Rtc::writeDateTime(const Pcf85063DateTime& dt) const {
  if (!isValidDateTime(dt)) {
    return false;
  }

  const uint8_t raw[7] = {
      static_cast<uint8_t>(decToBcd(dt.second) & 0x7F),
      static_cast<uint8_t>(decToBcd(dt.minute) & 0x7F),
      static_cast<uint8_t>(decToBcd(dt.hour) & 0x3F),
      static_cast<uint8_t>(decToBcd(dt.day) & 0x3F),
      static_cast<uint8_t>(decToBcd(dt.weekday) & 0x07),
      static_cast<uint8_t>(decToBcd(dt.month) & 0x1F),
      decToBcd(dt.year),
  };

  return writeRegisters(kRegSeconds, raw, sizeof(raw));
}

bool Pcf85063Rtc::setTime(uint8_t hour, uint8_t minute, uint8_t second) const {
  Pcf85063DateTime dt;
  if (!readDateTime(dt)) {
    return false;
  }
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  return writeDateTime(dt);
}

bool Pcf85063Rtc::setDate(uint8_t day, uint8_t weekday, uint8_t month, uint8_t year) const {
  Pcf85063DateTime dt;
  if (!readDateTime(dt)) {
    return false;
  }
  dt.day = day;
  dt.weekday = weekday;
  dt.month = month;
  dt.year = year;
  return writeDateTime(dt);
}

bool Pcf85063Rtc::startClock() const {
  uint8_t ctrl1 = 0;
  if (!readRegister(kRegControl1, ctrl1)) {
    return false;
  }
  ctrl1 = static_cast<uint8_t>(ctrl1 & ~kControl1StopBit);
  return writeRegister(kRegControl1, ctrl1);
}

bool Pcf85063Rtc::stopClock() const {
  uint8_t ctrl1 = 0;
  if (!readRegister(kRegControl1, ctrl1)) {
    return false;
  }
  ctrl1 = static_cast<uint8_t>(ctrl1 | kControl1StopBit);
  return writeRegister(kRegControl1, ctrl1);
}

bool Pcf85063Rtc::readRegister(uint8_t reg, uint8_t& value) const {
  return readRegisters(reg, &value, 1);
}

bool Pcf85063Rtc::writeRegister(uint8_t reg, uint8_t value) const {
  return writeRegisters(reg, &value, 1);
}

uint8_t Pcf85063Rtc::decToBcd(uint8_t v) {
  return static_cast<uint8_t>(((v / 10U) << 4U) | (v % 10U));
}

uint8_t Pcf85063Rtc::bcdToDec(uint8_t v) {
  return static_cast<uint8_t>(((v >> 4U) * 10U) + (v & 0x0FU));
}

bool Pcf85063Rtc::isValidDateTime(const Pcf85063DateTime& dt) {
  if (dt.second > 59 || dt.minute > 59 || dt.hour > 23) {
    return false;
  }
  if (dt.day < 1 || dt.day > 31) {
    return false;
  }
  if (dt.weekday > 6) {
    return false;
  }
  if (dt.month < 1 || dt.month > 12) {
    return false;
  }
  return dt.year <= 99;
}

bool Pcf85063Rtc::readRegisters(uint8_t startReg, uint8_t* out, size_t len) const {
  if (wire_ == nullptr || out == nullptr || len == 0) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(startReg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  const uint8_t requested = static_cast<uint8_t>(len);
  const uint8_t received = wire_->requestFrom(static_cast<int>(address_),
                                              static_cast<int>(requested));
  if (received != requested) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    if (!wire_->available()) {
      return false;
    }
    out[i] = static_cast<uint8_t>(wire_->read());
  }
  return true;
}

bool Pcf85063Rtc::writeRegisters(uint8_t startReg, const uint8_t* data, size_t len) const {
  if (wire_ == nullptr || data == nullptr || len == 0) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(startReg);
  for (size_t i = 0; i < len; ++i) {
    wire_->write(data[i]);
  }
  return wire_->endTransmission() == 0;
}
