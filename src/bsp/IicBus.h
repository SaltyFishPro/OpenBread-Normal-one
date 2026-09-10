#pragma once

#include <stdint.h>

namespace IicBus {

bool begin(uint32_t clockHz, bool reset);

}
