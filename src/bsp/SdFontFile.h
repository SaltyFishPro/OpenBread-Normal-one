#pragma once

#include <FS.h>
#include <stdint.h>

// Read-only SD access. Mount/unmount and USB ownership belong to the caller.
class SdFontFile {
public:
  bool open(const char* path);
  void close();
  bool readAt(uint32_t offset, uint8_t* data, size_t length);
  uint32_t size() const { return size_; }

private:
  File file_;
  uint32_t size_ = 0;
};
