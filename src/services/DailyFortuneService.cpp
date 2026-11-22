#include "DailyFortuneService.h"
#include "../data/DailyFortuneData.h"
#include "../log/Log.h"
#include <Preferences.h>
uint32_t DailyFortuneService::dateKey(const TimeService::Snapshot& t) {
  if (!t.valid || t.now.year < 2000 || t.now.month < 1 || t.now.month > 12 || t.now.day < 1 || t.now.day > 31) return 0;
  return static_cast<uint32_t>(t.now.year)*10000U + t.now.month*100U+t.now.day;
}
DailyFortuneService::Result DailyFortuneService::load(const TimeService::Snapshot& time) {
  hasToday_=false;const uint32_t date=dateKey(time);if(!date)return Result::InvalidTime;
  Preferences p;Record saved{};
  if(p.begin("daily-fortune",true)) {
    const bool ok=p.getBytesLength("result")==sizeof(saved) && p.getBytes("result",&saved,sizeof(saved))==sizeof(saved);
    p.end();
    if(ok && saved.magic==record_.magic && saved.index<DailyFortuneData::kCount &&
       saved.check==static_cast<uint16_t>((saved.date ^ (saved.date>>16) ^ saved.index ^ 0xA531U)&0xffffU)) {
      record_=saved;hasToday_=saved.date==date;
    }
  }
  return Result::Ready;
}
DailyFortuneService::Result DailyFortuneService::draw(const TimeService::Snapshot& time,uint32_t randomValue) {
  const auto status=load(time);if(status!=Result::Ready || hasToday_)return status;
  Record next;next.date=dateKey(time);next.index=static_cast<uint16_t>(randomValue%DailyFortuneData::kCount);
  next.check=static_cast<uint16_t>((next.date ^ (next.date>>16) ^ next.index ^ 0xA531U)&0xffffU);
  Preferences p;if(!p.begin("daily-fortune",false)){LogE("FORTUNE","save open failed");return Result::SaveFailed;}
  const bool saved=p.putBytes("result",&next,sizeof(next))==sizeof(next);p.end();
  if(!saved){LogE("FORTUNE","save failed");return Result::SaveFailed;}
  record_=next;hasToday_=true;LogI("FORTUNE","daily result saved date=%lu id=%u",static_cast<unsigned long>(next.date),static_cast<unsigned>(next.index+1));return Result::Ready;
}
