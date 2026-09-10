#pragma once

#include <stdint.h>

class Max17048Driver {
public:
  struct Reading {
    uint16_t version = 0;
    uint16_t status = 0;
    uint16_t voltageMv = 0;
    uint16_t stateOfChargeX100 = 0;
    int32_t chargeRateX100 = 0;
  };

  bool read(Reading& reading);

private:
  bool readRegister16(uint8_t reg, uint16_t& value);
};
