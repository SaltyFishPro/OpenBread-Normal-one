#pragma once

#include <stddef.h>

// Optional, bounded external RAM. Never falls back to allocating internal RAM.
class SdFontCacheBuffer {
public:
  static constexpr size_t kBudgetBytes = 128U * 1024U;
  SdFontCacheBuffer() = default;
  ~SdFontCacheBuffer() { end(); }
  SdFontCacheBuffer(const SdFontCacheBuffer&) = delete;
  SdFontCacheBuffer& operator=(const SdFontCacheBuffer&) = delete;
  bool allocate(size_t bytes);
  void end();
  void* data() const { return data_; }
  size_t size() const { return size_; }

private:
  void* data_ = nullptr;
  size_t size_ = 0;
};
