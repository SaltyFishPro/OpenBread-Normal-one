#pragma once

class ImuDriver {
public:
  bool begin();
  void poll();
};