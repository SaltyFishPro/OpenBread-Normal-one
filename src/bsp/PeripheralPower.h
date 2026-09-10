#pragma once

class PeripheralPower {
public:
  bool begin();
  void setAudioEnabled(bool enabled);
  void setSensorEnabled(bool enabled);
  bool isAudioEnabled() const;
  bool isSensorEnabled() const;
};
