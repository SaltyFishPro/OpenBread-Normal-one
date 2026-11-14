#include <WiFi.h>
#include <Preferences.h>
#include <stdexcept>
#include "src/services/WifiProvisionService.h"
#include "src/services/TimeService.h"

const char kWifiPortalPage[]="test";
TimeService::Snapshot clockData;
const TimeService::Snapshot& TimeService::snapshot() const { return clockData; }
bool TimeService::requestNtpSync(uint32_t,const char* ssid,const char*,bool reuse) {
  if(!reuse || WiFi.status()!=WL_CONNECTED || !(WiFi.SSID()==ssid))
    throw std::runtime_error("NTP did not reuse connected station");
  clockData.syncState=SyncState::Syncing;return true;
}
void TimeService::cancelNtpSync() { clockData.syncState=SyncState::Idle; }
void check(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }

int main() {
 try {
  using Service=WifiProvisionService;
  Service wifi;TimeService time;
  check(wifi.begin()&&!wifi.isRadioActive()&&WiFi.currentMode==WIFI_OFF,"boot enabled radio");
  auto& store=const_cast<WifiNetworkStore&>(wifi.networks());
  check(store.save("Home","secret",{}),"seed failed");
  check(wifi.connectSaved(0)&&wifi.isRadioActive(),"saved request not marked busy");
  wifi.tick(10,time);
  check(WiFi.currentMode==WIFI_STA&&!WiFi.ap,"saved connect opened hotspot");
  WiFi.connected("Wrong-old-network");wifi.tick(20,time);
  check(wifi.state()==Service::State::Connecting,"stale connection accepted");
  WiFi.connected("Home");wifi.tick(30,time);
  check(wifi.state()==Service::State::SyncingTime&&store.count()==1,"saved success not synced/deduped");
  clockData.syncState=TimeService::SyncState::Success;wifi.tick(40,time);
  check(!wifi.isRadioActive()&&WiFi.currentMode==WIFI_OFF,"finished session retained radio");
  check(wifi.connectSaved(0),"connect again");wifi.tick(100,time);
  WiFi.statusValue=WL_CONNECT_FAILED;wifi.tick(47100,time);
  check(wifi.state()==Service::State::Failed&&!wifi.isRadioActive()&&store.count()==1,"failed saved attempt modified records/radio");
  check(wifi.retryConnect(),"retry rejected");wifi.cancelProvision(time);wifi.tick(48000,time);
  check(WiFi.currentMode==WIFI_OFF&&!wifi.isRadioActive(),"canceled pending attempt started later");
  for(int i=0;i<4;++i) {char name[16];snprintf(name,sizeof(name),"Network-%d",i);check(store.save(name,"pw",{}),"fill");}
  wifi.startPortal(50000,Service::Purpose::TimeSync,4);
  check(wifi.isPortalActive()&&store.find("Home")>=0,"replacement removed before success");
  WebServer::instance->submit("   ");check(WebServer::instance->response==400,"blank SSID accepted");
  WebServer::instance->submit("New");wifi.tick(50010,time);
  WiFi.statusValue=WL_NO_SSID_AVAIL;wifi.tick(97010,time);
  check(wifi.state()==Service::State::Failed&&wifi.isPortalActive()&&store.find("Home")>=0,"portal failure lost AP/old history");
  check(wifi.editPortal()&&wifi.state()==Service::State::PortalReady,"edit did not return form");
  WebServer::instance->submit("New");wifi.tick(97020,time);
  WiFi.connected("New");wifi.tick(97030,time);
  check(wifi.state()==Service::State::Connected&&wifi.isPortalActive()&&store.count()==5&&store.find("Home")==-1&&store.find("New")==0,"replacement not applied on success");
  wifi.tick(103029,time);check(wifi.isPortalActive(),"hold shorter than 6 seconds");
  wifi.tick(103030,time);check(!wifi.isPortalActive()&&wifi.state()==Service::State::SyncingTime,"AP did not close before NTP");
  wifi.cancelProvision(time);check(!wifi.isRadioActive(),"cancel sync retained radio");
  wifi.startPortal(110000);wifi.tick(410000,time);
  check(wifi.state()==Service::State::PortalTimeout&&!wifi.isRadioActive(),"portal timeout retained radio");
  WiFi.apWorks=false;wifi.startPortal(420000);
  check(wifi.state()==Service::State::Failed&&!wifi.isRadioActive()&&!wifi.canRetryConnect(),"AP failure retained radio or offered invalid retry");
  WiFi.apWorks=true;wifi.startPortal(430000,Service::Purpose::FontSetup);
  WebServer::instance->submit("New");wifi.tick(430010,time);WiFi.connected("New");wifi.tick(430020,time);
  check(wifi.state()==Service::State::Connected&&wifi.isRadioActive()&&!wifi.isPortalActive()&&WiFi.currentMode==WIFI_STA,"font setup station not handed over");
  wifi.abortOnlineSession();
  puts("PASS: production service saved/portal lifecycle, stale IP, failures, retry/cancel, replacement, hold, timeout, font setup");
  return 0;
 } catch(const std::exception& e) {fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
