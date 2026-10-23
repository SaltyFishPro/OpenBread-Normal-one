// Host verification only: count encoded byte reads without instrumenting firmware.
#pragma once
#include <Arduino.h>

inline bool countIconReads = false;
inline uint64_t iconByteReads = 0;
inline uint8_t readIconByte(const void* address) {
  if (countIconReads) ++iconByteReads;
  return *static_cast<const uint8_t*>(address);
}

#undef pgm_read_byte
#define pgm_read_byte(address) readIconByte(address)
