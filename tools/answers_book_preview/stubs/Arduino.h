#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
using byte = uint8_t;
using uint = unsigned int;
#define ARDUINO 100
#define PROGMEM
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#ifndef OB_PREVIEW_CUSTOM_CLOCK
inline uint32_t millis() { return 0; }
inline void delay(unsigned) {}
#endif
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
using std::min;
using std::max;
