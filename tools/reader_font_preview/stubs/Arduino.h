#pragma once
#include "../../sd_font_tests/stubs/Arduino.h"
#include <esp_heap_caps.h>
inline bool psramFound() { return TestPsram::available; }
inline void* ps_malloc(size_t bytes) { return TestPsram::available ? std::malloc(bytes) : nullptr; }
