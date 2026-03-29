#include "IconBitmap.h"

#include "../bsp/BoardConfig.h"

namespace {
void writeLogicalPixel(ST7305_2p9_BW_DisplayDriver& canvas, int16_t lx, int16_t ly,
                       bool colorOn) {
  int16_t rx = lx;
  int16_t ry = ly;
  switch (BoardConfig::kDisplayRotation % 4U) {
    case 0:
      break;
    case 1:
      rx = static_cast<int16_t>(BoardConfig::kDisplayRawWidth - 1 - ly);
      ry = lx;
      break;
    case 2:
      rx = static_cast<int16_t>(BoardConfig::kDisplayRawWidth - 1 - lx);
      ry = static_cast<int16_t>(BoardConfig::kDisplayRawHeight - 1 - ly);
      break;
    case 3:
      rx = ly;
      ry = static_cast<int16_t>(BoardConfig::kDisplayRawHeight - 1 - lx);
      break;
  }

  if (rx < 0 || ry < 0 || rx >= BoardConfig::kDisplayRawWidth ||
      ry >= BoardConfig::kDisplayRawHeight) {
    return;
  }
  canvas.writePoint(static_cast<uint>(rx), static_cast<uint>(ry), colorOn);
}
}  // namespace

namespace IconBitmap {
uint16_t frameAt(const Anim& anim, uint32_t nowMs) {
  if (anim.frameCount == 0 || anim.frameDelayMs == 0) {
    return 0;
  }
  return static_cast<uint16_t>((nowMs / anim.frameDelayMs) % anim.frameCount);
}

void drawFrame(ST7305_2p9_BW_DisplayDriver& canvas, const Anim& anim, uint16_t frameIndex,
               int16_t dstX, int16_t dstY, int16_t dstW, int16_t dstH, bool invert,
               int16_t clipTop, int16_t clipBottom) {
  if (anim.frameCount == 0 || dstW <= 0 || dstH <= 0 || anim.frames == nullptr) {
    return;
  }

  const uint16_t idx = static_cast<uint16_t>(frameIndex % anim.frameCount);
  const uint8_t* frame = anim.frames + (static_cast<size_t>(idx) * anim.frameBytes);
  const uint16_t srcStride = (anim.frameWidth + 7U) / 8U;

  for (int16_t dy = 0; dy < dstH; ++dy) {
    const int16_t py = static_cast<int16_t>(dstY + dy);
    if (py < clipTop || py > clipBottom) {
      continue;
    }

    const uint16_t sy = static_cast<uint16_t>((static_cast<uint32_t>(dy) * anim.frameHeight) /
                                              static_cast<uint16_t>(dstH));
    for (int16_t dx = 0; dx < dstW; ++dx) {
      const uint16_t sx = static_cast<uint16_t>((static_cast<uint32_t>(dx) * anim.frameWidth) /
                                                static_cast<uint16_t>(dstW));
      const uint16_t byteIndex = static_cast<uint16_t>(sy * srcStride + (sx >> 3));
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x7U));
      const bool on = (pgm_read_byte(frame + byteIndex) & bitMask) != 0;
      writeLogicalPixel(canvas, static_cast<int16_t>(dstX + dx), py, invert ? !on : on);
    }
  }
}
}  // namespace IconBitmap

