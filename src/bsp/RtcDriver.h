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
    bool clockIntegrityLost = false;
  };

  struct Diagnostics {
    uint8_t control1 = 0;
    uint8_t control2 = 0;
    uint8_t offset = 0;
    uint8_t timerMode = 0;
    bool oscillatorStopped = false;
  };

  bool begin();
  void sync();
  bool isAvailable() const;
  bool read(DateTime& out);
  bool write(const DateTime& in);
  bool readDiagnostics(Diagnostics& out);
  bool setMinuteInterruptEnabled(bool enabled);
  bool clearTimerFlag();

private:
  Pcf85063Rtc rtc_;
  bool ready_ = false;
};
