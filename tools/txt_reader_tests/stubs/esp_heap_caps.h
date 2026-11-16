#pragma once
#include <cstdint>
#include <cstdlib>
#include <map>
constexpr uint32_t MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
namespace TestPsram { inline bool fail=false, available=true; inline std::map<void*,size_t> blocks; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return TestPsram::fail?0:8U*1024U*1024U; }
inline void* heap_caps_malloc(size_t size,uint32_t) {
  if(TestPsram::fail)return nullptr;
  void* p=std::malloc(size);if(p)TestPsram::blocks[p]=size;return p;
}
inline void heap_caps_free(void* p) { TestPsram::blocks.erase(p);std::free(p); }
