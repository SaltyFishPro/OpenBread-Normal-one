#pragma once

#include <stdint.h>

class DisplayMonoTft;
namespace IconBitmap { struct Anim; }

class SdUsbPage {
public:
  void render(DisplayMonoTft& display, int16_t yOffset, const IconBitmap::Anim& icon,
              bool enabled) const;
};
