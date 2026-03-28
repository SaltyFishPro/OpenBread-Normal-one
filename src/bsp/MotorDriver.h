#pragma once

class MotorDriver {
public:
  bool begin();
  void pulse(unsigned short ms);
};