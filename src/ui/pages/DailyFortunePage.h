#pragma once
#include <stdint.h>
#include "../../services/DailyFortuneService.h"
class DisplayMonoTft;class SdBitmapFont;
class DailyFortunePage {
public:
  static bool selected(uint8_t home,uint8_t section){return home==10 && section==0;}
  void enter(const TimeService::Snapshot& time);
  void input(bool up,bool down,bool ok,uint32_t now,const TimeService::Snapshot& time);
  bool back();
  bool update(uint32_t now,const TimeService::Snapshot& time);
  bool animating()const{return view_==View::Animation;}
  void render(DisplayMonoTft&,int16_t y,uint32_t now,const TimeService::Snapshot&,SdBitmapFont* full14,SdBitmapFont* full20,SdBitmapFont* ui24)const;
private:
  enum class View:uint8_t { Intro,Animation,Result,Detail,Invalid,SaveError };
  DailyFortuneService service_;View view_=View::Intro;uint32_t started_=0,date_=0;
};
