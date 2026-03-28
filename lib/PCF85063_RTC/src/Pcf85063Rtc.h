#pragma once

#include <Arduino.h>
#include <Wire.h>

struct Pcf85063DateTime {
  uint8_t second = 0;
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t day = 1;
  uint8_t weekday = 0;  // 0-6
  uint8_t month = 1;    // 1-12
  uint8_t year = 0;     // 0-99
};

class Pcf85063Rtc {
public:
  static constexpr uint8_t kDefaultAddress = 0x51;

  explicit Pcf85063Rtc(uint8_t i2cAddress = kDefaultAddress);

  bool begin(TwoWire& wire = Wire);
  bool isConnected() const;

  bool readDateTime(Pcf85063DateTime& out) const;
  bool writeDateTime(const Pcf85063DateTime& dt) const;

  bool setTime(uint8_t hour, uint8_t minute, uint8_t second) const;
  bool setDate(uint8_t day, uint8_t weekday, uint8_t month, uint8_t year) const;

  bool startClock() const;
  bool stopClock() const;

  bool readRegister(uint8_t reg, uint8_t& value) const;
  bool writeRegister(uint8_t reg, uint8_t value) const;

private:
  static constexpr uint8_t kRegControl1 = 0x00;
  static constexpr uint8_t kRegSeconds = 0x04;
  static constexpr uint8_t kControl1StopBit = 1u << 5;

  TwoWire* wire_ = nullptr;
  uint8_t address_;

  static uint8_t decToBcd(uint8_t v);
  static uint8_t bcdToDec(uint8_t v);
  static bool isValidDateTime(const Pcf85063DateTime& dt);

  bool readRegisters(uint8_t startReg, uint8_t* out, size_t len) const;
  bool writeRegisters(uint8_t startReg, const uint8_t* data, size_t len) const;
};
