#pragma once

class DacDriver {
public:
  bool begin();
  void setVolume(unsigned char percent);
};