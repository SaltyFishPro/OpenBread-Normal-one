#pragma once
#include <stdint.h>
class ImuDriver {
public:
  struct Sample { int16_t ax=0,ay=0,az=0,gx=0,gy=0,gz=0; };
  void powerOn();
  bool beginBus();
  bool beginLowPower();
  bool configure();
  bool readSample(Sample& sample);
  void end();
  uint8_t address()const; uint8_t whoAmI()const;
private:
  bool readRegister(uint8_t reg,uint8_t& value); bool writeRegister(uint8_t reg,uint8_t value); bool readRegisters(uint8_t reg,uint8_t* data,uint8_t length);
  uint8_t address_=0,whoAmI_=0;
};
