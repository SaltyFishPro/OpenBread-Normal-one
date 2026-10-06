#pragma once
#include "Arduino.h"
class Print { public: virtual size_t write(uint8_t) = 0; };
