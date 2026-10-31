#pragma once

#include <stdint.h>

class DisplayMonoTft;

namespace RacingCarMesh {
void draw(DisplayMonoTft& display, int16_t centerX, int16_t centerY,
          float yaw, float pitch);
}
