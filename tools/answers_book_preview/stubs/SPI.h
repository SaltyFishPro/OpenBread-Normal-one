#pragma once
#include "Arduino.h"
class SPIClass {
public:
 void begin(int=-1,int=-1,int=-1,int=-1) {}
 void setFrequency(unsigned) {}
 void write(uint8_t) {}
 void writeBytes(const uint8_t*, size_t) {}
};
inline SPIClass SPI;
