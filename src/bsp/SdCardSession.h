#pragma once

#include <stdint.h>

// Changes whenever the mounted card or its contents can have changed.
namespace SdCardSession {
inline uint32_t epoch = 1;
inline uint32_t generation() { return epoch; }
inline void invalidate() { if (++epoch == 0) ++epoch; }
}
