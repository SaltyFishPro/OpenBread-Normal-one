#pragma once
#include <stdint.h>
#include <stddef.h>

// Fixed-size parser for one UTF-8 LRC line. Returns the number of timestamps.
namespace LrcParser {
constexpr size_t kMaxTimestamps = 8;
size_t parse(const char* line, uint32_t* timesMs, size_t capacity, const char*& text);
}
