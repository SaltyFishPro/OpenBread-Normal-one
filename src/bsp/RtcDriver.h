#pragma once

#include <Arduino.h>

#include <Pcf85063Rtc.h>

class RtcDriver {
public:
  struct DateTime {
    uint8_t second = 0;
    uint8_t minute = 0;
    uint8_t hour = 0;
    uint8_t day = 1;
    uint8_t weekday = 0;
    uint8_t month = 1;
    uint8_t year = 0;
  };

  bool begin();
  void sync();
  bool isAvailable() const;
  bool read(DateTime& out);
  bool write(const DateTime& in);

private:
  Pcf85063Rtc rtc_;
  bool ready_ = false;
};
