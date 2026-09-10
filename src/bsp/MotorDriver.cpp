#include "MotorDriver.h"

#include <Arduino.h>

#include "BoardConfig.h"

#ifndef OB_MOTOR_DRIVER_LOG_ENABLED
#define OB_MOTOR_DRIVER_LOG_ENABLED 1
#endif

bool MotorDriver::begin() {
  pinMode(BoardConfig::kPinDrvEn, OUTPUT);
  digitalWrite(BoardConfig::kPinDrvEn, LOW);
#if OB_MOTOR_DRIVER_LOG_ENABLED
  if (Serial) {
    Serial.printf("[MOTOR] init en_pin=%d en=0 state=off\r\n", BoardConfig::kPinDrvEn);
  }
#endif
  return true;
}

void MotorDriver::pulse(unsigned short ms) { (void)ms; }
