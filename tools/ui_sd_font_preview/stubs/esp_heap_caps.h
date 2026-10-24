#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
constexpr uint32_t MALLOC_CAP_SPIRAM = 1;
constexpr uint32_t MALLOC_CAP_8BIT = 2;
namespace TestPsram {
inline bool available = true, failAllocation = false;
inline size_t largest = 8U * 1024U * 1024U;
inline size_t used = 0, peak = 0, allocations = 0, frees = 0;
inline std::unordered_map<void*, size_t> blocks;
}
inline size_t heap_caps_get_largest_free_block(uint32_t caps) {
  return TestPsram::available && (caps & MALLOC_CAP_SPIRAM) ? TestPsram::largest : 0;
}
inline void* heap_caps_malloc(size_t bytes, uint32_t caps) {
  if (!TestPsram::available || TestPsram::failAllocation || !(caps & MALLOC_CAP_SPIRAM) ||
      bytes > TestPsram::largest) return nullptr;
  auto* data = std::malloc(bytes);
  if (data) {
    ++TestPsram::allocations; TestPsram::blocks[data] = bytes; TestPsram::used += bytes;
    if (TestPsram::used > TestPsram::peak) TestPsram::peak = TestPsram::used;
  }
  return data;
}
inline void heap_caps_free(void* data) {
  if (data) {
    ++TestPsram::frees; TestPsram::used -= TestPsram::blocks.at(data);
    TestPsram::blocks.erase(data);
  }
  std::free(data);
}
