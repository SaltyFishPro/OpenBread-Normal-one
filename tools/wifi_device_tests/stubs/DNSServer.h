#pragma once
class DNSServer {
 public:
  template<class... Args> void start(Args...) {}
  void stop() {}
  void processNextRequest() {}
};
