#pragma once

#include <Arduino.h>
#include <ST7305_2p9_BW_DisplayDriver.h>

namespace IconBitmap {
struct Anim {
  const uint8_t* frames = nullptr;
  uint16_t frameBytes = 0;
  uint16_t frameWidth = 0;
  uint16_t frameHeight = 0;
  uint16_t frameDelayMs = 0;
  uint16_t frameCount = 0;
};

uint16_t frameAt(const Anim& anim, uint32_t nowMs);

void drawFrame(ST7305_2p9_BW_DisplayDriver& canvas, const Anim& anim, uint16_t frameIndex,
               int16_t dstX, int16_t dstY, int16_t dstW, int16_t dstH, bool invert,
               int16_t clipTop, int16_t clipBottom);
}  // namespace IconBitmap

