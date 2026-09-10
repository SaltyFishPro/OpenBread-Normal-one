#include "Max17048Driver.h"

#include <Wire.h>

#include "IicBus.h"

namespace {
constexpr uint8_t kAddress = 0x36;
constexpr uint8_t kRegVcell = 0x02;
constexpr uint8_t kRegSoc = 0x04;
constexpr uint8_t kRegVersion = 0x08;
constexpr uint8_t kRegCrate = 0x16;
constexpr uint8_t kRegStatus = 0x1A;
}

bool Max17048Driver::read(Reading& reading) {
  if (!IicBus::begin(100000U, true)) {
    return false;
  }

  uint16_t rawVoltage = 0;
  uint16_t rawSoc = 0;
  uint16_t rawRate = 0;
  if (!readRegister16(kRegVersion, reading.version) ||
      !readRegister16(kRegVcell, rawVoltage) || !readRegister16(kRegSoc, rawSoc) ||
      !readRegister16(kRegCrate, rawRate) || !readRegister16(kRegStatus, reading.status)) {
    return false;
  }

  reading.voltageMv = static_cast<uint16_t>(
      (static_cast<uint32_t>(rawVoltage) * 78125UL + 500000UL) / 1000000UL);
  reading.stateOfChargeX100 = static_cast<uint16_t>(
      (static_cast<uint32_t>(rawSoc) * 100UL + 128UL) / 256UL);
  reading.chargeRateX100 =
      (static_cast<int32_t>(static_cast<int16_t>(rawRate)) * 208L) / 10L;
  return true;
}

bool Max17048Driver::readRegister16(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0U) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(kAddress), 2) != 2) {
    return false;
  }
  value = static_cast<uint16_t>((static_cast<uint16_t>(Wire.read()) << 8U) |
                                static_cast<uint8_t>(Wire.read()));
  return true;
}
