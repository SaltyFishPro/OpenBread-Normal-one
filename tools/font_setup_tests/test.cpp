#include <WiFi.h>
#include <SD_MMC.h>
#include <fstream>
#include <stdexcept>
#include "src/services/FontSetupService.h"
#include "src/services/WifiProvisionService.h"
#include "src/services/TimeService.h"
#include "src/services/SdCardService.h"
#include "src/ui/pages/FirstBootPage.h"
#include "src/bsp/DisplayMonoTft.h"
DisplayMonoTft::DisplayMonoTft():display_(SPI){}
bool DisplayMonoTft::begin(){display_.setRotation(1);display_.clearDisplay();text_.begin(display_);return true;}
int DisplayMonoTft::width()const{return display_.getDisplayWidth();}
int DisplayMonoTft::height()const{return display_.getDisplayHeight();}
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas(){return display_;}
U8G2_FOR_ST73XX& DisplayMonoTft::text(){return text_;}
void DisplayMonoTft::restoreFrame(const uint8_t* frame){display_.copyFrameBufferFrom(frame);}
void save(DisplayMonoTft& d,const char* name){
  uint8_t frame[8064];d.canvas().copyFrameBufferTo(frame);
  std::ofstream out(std::string("tmp/")+name+".pgm",std::ios::binary);out<<"P5\n384 168\n255\n";
  for(int y=0;y<168;++y)for(int x=0;x<384;++x){int16_t px,py;d.canvas().logicalToPhysical(x,y,px,py);
    out.put(static_cast<char>((frame[(py/2)*42+px/4]&(1U<<(7-((px%4)*2+py%2))))?0:255));}
}
const char kWifiPortalPage[]="test";
TimeService::Snapshot clockData;
const TimeService::Snapshot& TimeService::snapshot()const{return clockData;}
bool TimeService::requestNtpSync(uint32_t,const char*,const char*,bool){throw std::runtime_error("font retry requested time sync");}
void TimeService::cancelNtpSync(){}
SdCardService::Status SdCardService::refresh(){return Status::Ready;}
SdCardService::Status SdCardService::status()const{return Status::Ready;}
bool SdCardService::rootReadable()const{return true;}
uint64_t SdCardService::freeBytes()const{return 32U*1024U*1024U;}
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
  using State=FontSetupService::State;
  SdCardService sd;WifiProvisionService wifi;TimeService time;FontSetupService setup;
  wifi.begin();setup.begin(0);TestClock::nowMs=300;setup.tick(300,sd,wifi);
  check(setup.state()==State::NeedFonts,"missing fonts not detected");
  // Seed an already installed first file. Retries must skip it, without deleting it.
  std::ifstream source("tools/font_builder/output/font-14.bin",std::ios::binary);
  SetupFs::files["/Font/font-14.bin"]={std::istreambuf_iterator<char>(source),{}};
  check(!SetupFs::files["/Font/font-14.bin"].empty(),"missing full-font fixture");
  setup.confirm(310,sd,wifi);
  check(setup.state()==State::Portal,"initial portal absent");
  WebServer::instance->submit("MyWiFi","secret");wifi.tick(320,time);
  WiFi.connected("MyWiFi");wifi.tick(330,time);setup.tick(330,sd,wifi);
  check(setup.state()==State::Error && setup.error()==FontSetupService::Error::Http,"HTTP failure not surfaced");
  check(!wifi.isRadioActive() && !wifi.isPortalActive(),"failure leaked radio");
  check(!SetupTls::active && SetupHttp::ends==1,"404 leaked TLS/HTTP transport");
  check(SetupHttp::urls.back().find("font-14.ui.bin")!=std::string::npos,"installed file not skipped");
  FirstBootPage page;DisplayMonoTft display;display.begin();
  page.render(display,setup,wifi,time,80,true);save(display,"font-retry-default");
  const int connects=WiFi.beginCalls;
  page.handleInput(false,false,false,true,340,setup,sd,wifi,time);
  check(setup.state()==State::Connecting && !wifi.isPortalActive(),"default retry reopened AP");
  wifi.tick(350,time);check(WiFi.beginCalls==connects+1 && WiFi.name=="MyWiFi","retry lost network");
  WiFi.connected("MyWiFi");wifi.tick(360,time);setup.tick(360,sd,wifi);
  check(setup.state()==State::Error && !wifi.isRadioActive(),"repeated failure did not return dialog/off");
  check(SetupHttp::urls.size()==2,"unexpected automatic retry");
  setup.tick(370,sd,wifi);check(SetupHttp::urls.size()==2,"failure auto-retried without user");
  page.handleInput(false,true,false,false,375,setup,sd,wifi,time);
  page.render(display,setup,wifi,time,80,true);save(display,"font-retry-reconfigure");
  page.handleInput(false,false,true,false,376,setup,sd,wifi,time);
  page.render(display,setup,wifi,time,80,true);save(display,"font-retry-left");
  page.handleInput(true,false,false,false,377,setup,sd,wifi,time);
  page.handleInput(false,false,false,true,380,setup,sd,wifi,time);
  check(setup.state()==State::Portal && wifi.isPortalActive(),"explicit reconfigure did not open AP");
  check(SetupFs::files.count("/Font/font-14.bin"),"retry deleted completed file");
  setup.cancel(wifi,time);check(!wifi.isRadioActive() && setup.state()==State::NeedFonts,"cancel leaked radio");
  auto retry=[&](uint32_t now){
    setup.confirm(now,sd,wifi);wifi.tick(now+1,time);
    WiFi.connected("MyWiFi");wifi.tick(now+2,time);setup.tick(now+2,sd,wifi);
  };
  // Starting after cancel opens the portal once; subsequent errors reuse WiFi.
  setup.confirm(400,sd,wifi);WebServer::instance->submit("MyWiFi","secret");
  wifi.tick(401,time);WiFi.connected("MyWiFi");wifi.tick(402,time);setup.tick(402,sd,wifi);
  SetupHttp::beginOk=false;retry(410);
  check(setup.state()==State::Error && !SetupTls::active && !wifi.isRadioActive(),"failed begin leaked TLS/radio");
  SetupHttp::beginOk=true;SetupHttp::status=200;SetupHttp::length=1;retry(420);
  check(setup.state()==State::Error && !SetupTls::active,"bad content length accepted/leaked TLS");
  SetupHttp::length=FontBundle::kFiles[1].bytes;retry(430);
  check(setup.state()==State::Downloading && SetupTls::active,"valid response not started");
  setup.tick(433,sd,wifi);
  check(setup.state()==State::Error && !SetupTls::active && !wifi.isRadioActive(),"body disconnect leaked TLS/radio");
  check(!SetupFs::files.count(FontBundle::kPartPath),"failed body left partial file");
  SetupHttp::connected=true;retry(440);
  setup.tick(16000,sd,wifi);
  check(setup.state()==State::Error && !SetupTls::active && !wifi.isRadioActive(),"body timeout leaked TLS/radio");
  retry(16010);WiFi.statusValue=WL_DISCONNECTED;setup.tick(16013,sd,wifi);
  check(setup.error()==FontSetupService::Error::Wifi && !SetupTls::active && !wifi.isRadioActive(),"WiFi loss leaked TLS/radio");
  puts("PASS: font setup 404/retry, begin failure, invalid length, body disconnect/timeout, WiFi loss, reconfigure, installed-file skip, cancel/radio off");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
