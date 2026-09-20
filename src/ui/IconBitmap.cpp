#include "IconBitmap.h"

#include "../bsp/BoardConfig.h"

namespace {
void decodeRleSpan(const uint8_t* frame, uint16_t encodedBytes, uint16_t spanOffset,
                   uint16_t spanLen, uint8_t* out) {
  for (uint16_t i = 0; i < spanLen; ++i) {
    out[i] = 0;
  }

  const uint16_t spanEnd = static_cast<uint16_t>(spanOffset + spanLen);
  uint16_t decodedOffset = 0;
  uint16_t encodedOffset = 0;
  while (encodedOffset + 1U < encodedBytes) {
    const uint8_t runLen = pgm_read_byte(frame + encodedOffset);
    const uint8_t runValue = pgm_read_byte(frame + encodedOffset + 1U);
    if (runLen == 0) {
      break;
    }

    const uint16_t runEnd = static_cast<uint16_t>(decodedOffset + runLen);
    if (runEnd > spanOffset && decodedOffset < spanEnd) {
      uint16_t copyStart = decodedOffset;
      if (copyStart < spanOffset) {
        copyStart = spanOffset;
      }
      uint16_t copyEnd = runEnd;
      if (copyEnd > spanEnd) {
        copyEnd = spanEnd;
      }
      for (uint16_t pos = copyStart; pos < copyEnd; ++pos) {
        out[pos - spanOffset] = runValue;
      }
    }

    if (runEnd >= spanEnd) {
      return;
    }
    decodedOffset = runEnd;
    encodedOffset = static_cast<uint16_t>(encodedOffset + 2U);
  }
}

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
  const bool rleEncoded = (anim.frameBytes & kRleFrameBytesFlag) != 0;
  const uint16_t frameBytes = static_cast<uint16_t>(anim.frameBytes & kFrameBytesMask);
  const uint8_t* frame = anim.frames + (static_cast<size_t>(idx) * frameBytes);
  const uint16_t srcStride = (anim.frameWidth + 7U) / 8U;
  uint8_t decodedRow[64];
  uint16_t cachedRow = 0xFFFFU;

  // 源索引用增量步进推导，避免内层循环每像素做一次 32 位除法。
  // sx(dx) = dx * srcW / dstW、sy(dy) = dy * srcH / dstH 的取值序列保持完全一致。
  const uint32_t dstWidth = static_cast<uint32_t>(dstW);
  const uint32_t dstHeight = static_cast<uint32_t>(dstH);
  uint32_t rowAccum = 0;
  uint16_t sy = 0;

  for (int16_t dy = 0; dy < dstH; ++dy) {
    const int16_t py = static_cast<int16_t>(dstY + dy);
    const bool rowVisible = (py >= clipTop) && (py <= clipBottom);

    if (rowVisible) {
      const uint16_t rowOffset = static_cast<uint16_t>(sy * srcStride);
      if (rleEncoded && cachedRow != sy) {
        if (srcStride > sizeof(decodedRow)) {
          return;
        }
        decodeRleSpan(frame, frameBytes, rowOffset, srcStride, decodedRow);
        cachedRow = sy;
      }

      uint32_t colAccum = 0;
      uint16_t sx = 0;
      for (int16_t dx = 0; dx < dstW; ++dx) {
        const uint16_t byteIndex = static_cast<uint16_t>(sx >> 3);
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x7U));
        uint8_t sourceByte = 0;
        if (rleEncoded) {
          sourceByte = decodedRow[byteIndex];
        } else {
          sourceByte = pgm_read_byte(frame + rowOffset + byteIndex);
        }
        const bool on = (sourceByte & bitMask) != 0;
        writeLogicalPixel(canvas, static_cast<int16_t>(dstX + dx), py, invert ? !on : on);

        colAccum += anim.frameWidth;
        while (colAccum >= dstWidth) {
          colAccum -= dstWidth;
          ++sx;
        }
      }
    }

    rowAccum += anim.frameHeight;
    while (rowAccum >= dstHeight) {
      rowAccum -= dstHeight;
      ++sy;
    }
  }
}
}  // namespace IconBitmap

