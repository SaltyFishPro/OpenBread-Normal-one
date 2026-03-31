#pragma once

#include <stdint.h>

#include "../bsp/SdCardDriver.h"

class SdCardService {
public:
  enum class Status : uint8_t {
    NotInserted,
    ReinsertNeeded,
    Ready
  };

  bool begin();
  Status refresh();
  Status status() const;
  uint64_t totalBytes() const;
  uint64_t freeBytes() const;

private:
  SdCardDriver driver_;
  Status status_ = Status::ReinsertNeeded;
  uint64_t totalBytes_ = 0;
  uint64_t freeBytes_ = 0;
};
