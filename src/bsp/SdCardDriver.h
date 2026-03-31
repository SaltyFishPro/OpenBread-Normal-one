#pragma once

#include <stdint.h>

class SdCardDriver {
public:
  struct Info {
    bool mounted = false;
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
  };

  bool begin();
  bool refresh(Info& info);
};
