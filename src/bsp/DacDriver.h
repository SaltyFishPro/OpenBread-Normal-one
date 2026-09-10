#pragma once

class DacDriver {
public:
  bool begin();
  bool powerOn();
  void powerOff();
  void setMuted(bool muted);
  void setVolume(unsigned char percent);
  void updateOutputRoute(bool audioActive);
  bool headphonesInserted() const;
  bool powered() const;

private:
  bool initialized_ = false;
  bool powered_ = false;
  bool muted_ = true;
  bool speakerEnabled_ = false;
};
