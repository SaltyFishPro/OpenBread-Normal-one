#include "SdFontCacheBuffer.h"

#include <esp_heap_caps.h>

bool SdFontCacheBuffer::allocate(size_t bytes) {
  if (data_ && size_ == bytes) return true;
  end();
  constexpr uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  if (bytes == 0U || bytes > kBudgetBytes ||
      heap_caps_get_largest_free_block(caps) < bytes) return false;
  data_ = heap_caps_malloc(bytes, caps);
  if (!data_) return false;
  size_ = bytes;
  return true;
}

void SdFontCacheBuffer::end() {
  if (data_) heap_caps_free(data_);
  data_ = nullptr;
  size_ = 0;
}
