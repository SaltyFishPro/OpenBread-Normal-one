#include "MotorDriver.h"

#include <Arduino.h>

#include "BoardConfig.h"
#include "../log/Log.h"

bool MotorDriver::begin() {
  pinMode(BoardConfig::kPinDrvEn, OUTPUT);
  digitalWrite(BoardConfig::kPinDrvEn, LOW);
  LogI("MOTOR", "init en_pin=%d en=0 state=off", BoardConfig::kPinDrvEn);
  return true;
}

void MotorDriver::pulse(unsigned short ms) { (void)ms; }
