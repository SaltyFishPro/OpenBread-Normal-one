#pragma once
#include <stdint.h>
#include "TimeService.h"
class DailyFortuneService {
public:
  enum class Result : uint8_t { Ready, InvalidTime, SaveFailed };
  Result load(const TimeService::Snapshot& time);
  Result draw(const TimeService::Snapshot& time, uint32_t randomValue);
  bool hasToday() const { return hasToday_; }
  uint16_t index() const { return record_.index; }
private:
  struct Record { uint32_t magic=0x464F5231, date=0; uint16_t index=0, check=0; };
  static uint32_t dateKey(const TimeService::Snapshot& time);
  Record record_{};
  bool hasToday_=false;
};
