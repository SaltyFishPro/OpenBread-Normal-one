#pragma once
#include "NetworkTypes.h"
enum wl_status_t { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_CONNECTED, WL_CONNECT_FAILED, WL_DISCONNECTED };
enum { WIFI_OFF, WIFI_STA, WIFI_AP_STA };
struct FakeWifi {
  int currentMode=WIFI_OFF, beginCalls=0, apStops=0;
  bool ap=false, apWorks=true;
  wl_status_t statusValue=WL_DISCONNECTED;
  std::string name;
  IPAddress ip{};
  void mode(int value) { currentMode=value; }
  void setSleep(bool) {}
  void disconnect(bool,bool) { statusValue=WL_DISCONNECTED;ip={}; }
  void softAPdisconnect(bool) { ap=false;++apStops; }
  bool softAP(const char*) { return ap=apWorks; }
  void begin(const char* ssid,const char*) { name=ssid;++beginCalls; }
  wl_status_t status() const { return statusValue; }
  IPAddress localIP() const { return ip; }
  IPAddress softAPIP() const { return {{192,168,4,1}}; }
  String SSID() const { return name; }
  void connected(const char* ssid) { name=ssid;statusValue=WL_CONNECTED;ip={{192,168,1,20}}; }
};
inline FakeWifi WiFi;
struct FakeEsp { uint64_t getEfuseMac() { return 0x1A2B; } };
inline FakeEsp ESP;
