#include <Preferences.h>
#include "src/services/DailyFortuneService.h"
#include "src/data/DailyFortuneData.h"
#include <stdexcept>
#include <cstdio>
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(){try{using R=DailyFortuneService::Result;TimeService::Snapshot t;DailyFortuneService s;check(s.load(t)==R::InvalidTime,"invalid clock");t.valid=true;t.now.year=2026;t.now.month=11;t.now.day=23;check(s.load(t)==R::Ready&&!s.hasToday(),"first day");check(s.draw(t,89)==R::Ready&&s.index()==89,"first draw");DailyFortuneService restart;check(restart.load(t)==R::Ready&&restart.hasToday()&&restart.index()==89,"restart");check(restart.draw(t,1)==R::Ready&&restart.index()==89,"same-day repeat");t.now.day=24;check(restart.load(t)==R::Ready&&!restart.hasToday(),"new day");TestPrefs::failWrite=true;check(restart.draw(t,7)==R::SaveFailed&&!restart.hasToday(),"save failure");TestPrefs::failWrite=false;check(restart.draw(t,7)==R::Ready&&restart.index()==7,"retry");auto& bytes=TestPrefs::entries["result"];bytes.back()^=1;DailyFortuneService corrupt;check(corrupt.load(t)==R::Ready&&!corrupt.hasToday(),"corrupt record");check(DailyFortuneData::kCount==90,"library size");puts("PASS: invalid time, 90 entries, one result per day, restart, new day, save failure/retry, corrupted record");}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
