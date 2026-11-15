#include "LrcParser.h"
#include <stdint.h>

namespace LrcParser {
size_t parse(const char* line, uint32_t* timesMs, size_t capacity, const char*& text) {
  text = nullptr;
  if (!line || !timesMs || !capacity) return 0;
  const auto* bytes = reinterpret_cast<const uint8_t*>(line);
  if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) line += 3;
  size_t count = 0;
  const char* p = line;
  while (*p == '[' && count < capacity && count < kMaxTimestamps) {
    const char* q = p + 1;
    uint32_t minutes = 0, seconds = 0;
    if (*q < '0' || *q > '9') break;
    while (*q >= '0' && *q <= '9') {
      minutes = minutes * 10 + static_cast<unsigned>(*q++ - '0');
      if (minutes > 999) return 0;
    }
    if (*q++ != ':') break;
    if (*q < '0' || *q > '9') break;
    while (*q >= '0' && *q <= '9') {
      seconds = seconds * 10 + static_cast<unsigned>(*q++ - '0');
      if (seconds >= 60) return 0;
    }
    uint32_t fraction = 0;
    if (*q == '.' || *q == ':') {
      ++q;
      unsigned digits = 0;
      while (*q >= '0' && *q <= '9') {
        if (digits < 3) fraction = fraction * 10 + static_cast<unsigned>(*q - '0');
        ++digits; ++q;
      }
      if (!digits) return 0;
      if (digits == 1) fraction *= 100;
      else if (digits == 2) fraction *= 10;
    }
    if (*q++ != ']') break;
    timesMs[count++] = minutes * 60000U + seconds * 1000U + fraction;
    p = q;
  }
  if (!count || *p == '[' || !*p) return 0;
  text = p;
  return count;
}
}
