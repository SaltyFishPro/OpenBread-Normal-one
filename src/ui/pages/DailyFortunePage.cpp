#include "DailyFortunePage.h"
#include "../../data/DailyFortuneData.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DrawUtils.h"
#include "../SdUiText.h"
#include "../../services/SdBitmapFont.h"
#include <cstring>
#include <esp_system.h>
#include <cstdio>
namespace { uint32_t day(const TimeService::Snapshot& t){return t.valid?static_cast<uint32_t>(t.now.year)*10000U+t.now.month*100U+t.now.day:0;} }
void DailyFortunePage::enter(const TimeService::Snapshot& time){date_=day(time);auto r=service_.load(time);view_=r==DailyFortuneService::Result::InvalidTime?View::Invalid:service_.hasToday()?View::Result:View::Intro;}
void DailyFortunePage::input(bool up,bool down,bool ok,uint32_t now,const TimeService::Snapshot& time){
 if(view_==View::Intro && ok){auto r=service_.draw(time,esp_random());if(r==DailyFortuneService::Result::InvalidTime)view_=View::Invalid;else if(r==DailyFortuneService::Result::SaveFailed)view_=View::SaveError;else{view_=View::Animation;started_=now;}return;}
 if(view_==View::SaveError&&ok){view_=View::Intro;return;}
 if((up||down||ok)&&view_==View::Result)view_=View::Detail;
 else if((up||down||ok)&&view_==View::Detail)view_=View::Result;
}
bool DailyFortunePage::back(){if(view_==View::Detail){view_=View::Result;return true;}if(view_==View::Animation){view_=View::Result;return true;}return false;}
bool DailyFortunePage::update(uint32_t now,const TimeService::Snapshot& time){if(day(time)!=date_){enter(time);return true;}if(view_==View::Animation&&now-started_>=1150U){view_=View::Result;return true;}return false;}
void DailyFortunePage::render(DisplayMonoTft& d,int16_t y,uint32_t now,const TimeService::Snapshot& time,SdBitmapFont* full14,SdBitmapFont* full20,SdBitmapFont* ui24)const{
 if(y!=0)return;auto& c=d.canvas();auto& t=d.text();const auto black=ST7305_COLOR_BLACK,white=ST7305_COLOR_WHITE;
 c.drawFilledRectangle(0,0,383,167,black);t.setFont(u8g2_font_7x14_tf);t.setFontMode(1);t.setForegroundColor(white);t.setBackgroundColor(black);
 SdUiText::Scope fontScope(d,full14);
 auto text=[&](const char* s,int x,int baseline){t.drawUTF8(x,baseline,s);};
 auto right=[&](const char* s,int baseline){text(s,368-t.getUTF8Width(s),baseline);};
 auto big=[&](const char* s,int x,int baseline,int end,SdBitmapFont* font){SdUiText::draw(d,font,x,baseline,s,white,{static_cast<int16_t>(x),32,static_cast<int16_t>(end),140});};
 auto wrapped=[&](const char* value,int x,int first,int maxWidth){
   const char* cursor=value;
   for(uint8_t row=0;row<2 && *cursor;++row){
     char buffer[80]={};size_t used=0;int width=0;
     while(*cursor){const char* next=cursor;uint32_t cp;SdBitmapText::nextCodepoint(next,cp);
       const size_t n=static_cast<size_t>(next-cursor);if(used+n>=sizeof(buffer))break;
       char glyph[5]={};memcpy(glyph,cursor,n);const int advance=t.getUTF8Width(glyph);
       if(used && width+advance>maxWidth)break;
       memcpy(buffer+used,cursor,n);used+=n;buffer[used]=0;width+=advance;cursor=next;}
     text(buffer,x,first+row*22);
   }
 };
 auto rule=[&](int x,int yy,int w){c.drawFastHLine(x,yy,w,white);};
 text("今日抽签",16,22);char date[16];if(time.valid)snprintf(date,sizeof(date),"%04u.%02u.%02u",time.now.year,time.now.month,time.now.day);else snprintf(date,sizeof(date),"--.--.--");right(date,22);rule(16,31,352);
 if(view_==View::Invalid||view_==View::SaveError){const char* msg=view_==View::Invalid?"时间还没有校准":"保存失败，请重试";big(msg,102,72,368,ui24);text(view_==View::Invalid?"请先到设置中校准设备时间":"请按 OK 返回并重新抽签",90,102);}
 else if(view_==View::Intro||view_==View::Animation){int dx=0,rise=0;if(view_==View::Animation){uint32_t elapsed=now-started_;if(elapsed<770)dx=((elapsed/100)%2?2:-2);else rise=(elapsed-770)*24/380;if(rise>24)rise=24;}
  for(int i=0;i<5;++i){int x=64+i*8+dx;int sy=51-(i==2?rise:0);c.drawLine(x,sy,x-5+i*2,80,white);c.drawFilledCircle(x,sy,1,white);}c.drawLine(56+dx,81,60+dx,119,white);c.drawLine(106+dx,81,102+dx,119,white);rule(60+dx,119,43);rule(56+dx,81,51);text("签",74+dx,108);c.drawFastVLine(143,48,77,white);
  big(view_==View::Animation?"心里默念，等待回应":"今天，会遇见什么？",163,71,367,full20);text(view_==View::Animation?"让这支签慢慢来到你面前":"给自己一点小小的期待",163,100);
 } else {const auto& f=DailyFortuneData::kEntries[service_.index()];DrawUtils::drawRoundRect(c,20,48,92,76,38,white,white);t.setForegroundColor(black);t.setBackgroundColor(white);text("今日",52,68);SdUiText::draw(d,ui24,54,99,"签",black,{20,48,111,123});text(f.theme,52,116);t.setForegroundColor(white);t.setBackgroundColor(black);c.drawFastVLine(133,47,80,white);big(f.name,153,67,367,full20);if(view_==View::Result){wrapped(f.line,153,98,215);}else{text(f.detail1,153,96);text(f.detail2,153,120);}}
 rule(16,143,352);text("Left 返回",16,161);if(view_==View::Intro)right("OK 抽一支签",161);else if(view_==View::Animation)right("正在抽签",161);else if(view_==View::Result)right("OK 查看签文",161);else if(view_==View::Detail)right("OK 返回签面",161);else if(view_==View::SaveError)right("OK 重试",161);
}
