#pragma once
#include "WiFiClientSecure.h"
#include <string>
#include <vector>
constexpr int HTTP_CODE_OK=200;
namespace SetupHttp {
inline std::vector<std::string> urls;
inline int status=404, length=0, ends=0;
inline bool beginOk=true, connected=false;
}
class HTTPClient {
  WiFiClient stream_;
  WiFiClientSecure* client_=nullptr;
  bool reuse_=true;
public:
  void setReuse(bool value){reuse_=value;}
  void setTimeout(uint32_t){}
  bool begin(WiFiClientSecure& client,const char* url){
    if(SetupTls::active)throw std::runtime_error("stale TLS socket reused");
    client_=&client;SetupHttp::urls.emplace_back(url);
    // Also exercise cleanup of a partially opened, failed begin.
    SetupTls::active=true;return SetupHttp::beginOk;
  }
  int GET(){
    if(reuse_)throw std::runtime_error("font HTTP keep-alive enabled");
    return SetupHttp::status;
  }
  int getSize(){return SetupHttp::length;}
  void end(){
    // Firmware must close TLS before HTTPClient can drain response bytes.
    if(SetupTls::active)throw std::runtime_error("HTTP end before TLS stop");
    ++SetupHttp::ends;client_=nullptr;
  }
  bool connected(){return SetupHttp::connected;}
  WiFiClient* getStreamPtr(){return &stream_;}
};
