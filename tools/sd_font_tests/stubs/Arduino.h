#pragma once
#define OB_PREVIEW_CUSTOM_CLOCK
#include "../../answers_book_preview/stubs/Arduino.h"
#undef OB_PREVIEW_CUSTOM_CLOCK
namespace TestClock { inline uint32_t nowMs = 0; }
inline uint32_t millis() { return TestClock::nowMs; }
inline void delay(unsigned ms) { TestClock::nowMs += ms; }
