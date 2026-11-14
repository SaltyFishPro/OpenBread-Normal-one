#pragma once
#include "NetworkTypes.h"
#include <functional>
#include <map>
enum { HTTP_GET, HTTP_POST };
class WebServer {
 public:
  explicit WebServer(int) { instance=this; }
  inline static WebServer* instance=nullptr;
  std::map<std::string,std::function<void()>> routes;
  std::map<std::string,std::string> args;
  int response=0;
  void on(const char* route,int,std::function<void()> fn) { routes[route]=fn; }
  void onNotFound(std::function<void()>) {}
  void sendHeader(const char*,const char*) {}
  void send_P(int code,const char*,const char*) { response=code; }
  void send(int code,const char*,const char*) { response=code; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  bool hasArg(const char* key) const { return args.count(key); }
  String arg(const char* key) { return args[key]; }
  void submit(const char* ssid,const char* pass="pw") {
    args={{"ssid",ssid},{"pass",pass}};
    routes.at("/connect")();
  }
};
