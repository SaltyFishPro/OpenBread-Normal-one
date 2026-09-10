#pragma once

#include <Arduino.h>

class IicBusScanner {
public:
  struct Result {
    bool busReady = false;
    uint8_t deviceCount = 0;
    uint8_t addresses[16] = {0};
    uint8_t overflowCount = 0;
    uint8_t errorCount = 0;
  };

  void powerOn();
  bool beginBus();
  uint8_t probe(uint8_t address);
  void end();

private:
  bool powerWasEnabled_ = false;
};
