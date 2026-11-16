#pragma once
#include <cstddef>
#include <cstdint>
#include <WiFi.h>
#include <stdexcept>
namespace SetupTls {
inline bool active=false;
inline int stops=0;
}
class WiFiClient {
public:
  int available(){return 0;}
  int readBytes(uint8_t*,size_t){return 0;}
};
class WiFiClientSecure:public WiFiClient {
public:
  void setInsecure(){}
  void stop(){
    if(SetupTls::active && WiFi.currentMode==WIFI_OFF)
      throw std::runtime_error("TLS stopped after radio shutdown");
    SetupTls::active=false;++SetupTls::stops;
  }
};
