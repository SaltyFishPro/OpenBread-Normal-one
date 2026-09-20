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

// RLE 图标整帧解码缓存。逐行解码时每一行都要从游程流起点扫描到该行：
// 主界面菜单一帧的 4 个 RLE 图标合计约 2.8 万次游程扫描，而像素写入只有
// 约 6.9 千次，扫描是实际瓶颈。这里改成整帧解码一次（约 1.6 千次扫描），
// 之后各行直接按行偏移索引，直到该图标的动画帧号变化才重新解码。
// 槽位需覆盖 100x100 1bpp 图标：13 字节/行 × 100 行 = 1300 字节（此前写成 1280，
// 结果主界面菜单图标全部落到兜底路径、缓存从未命中）。更大的图（150x150 子菜单
// 图标 2850 字节、全屏背景 8685 字节）仍走逐行解码。
constexpr uint16_t kFrameCacheSlotBytes = 1300;
constexpr uint8_t kFrameCacheSlots = 8;

struct FrameCacheSlot {
  const uint8_t* frames = nullptr;
  uint16_t frameIndex = 0xFFFFU;
  bool valid = false;
  uint8_t data[kFrameCacheSlotBytes];
};

FrameCacheSlot gFrameCache[kFrameCacheSlots];
uint8_t gFrameCacheNext = 0;

const uint8_t* decodeFrameCached(const uint8_t* frames, uint16_t frameIndex, const uint8_t* frame,
                                 uint16_t frameBytes, uint16_t decodedBytes) {
  for (uint8_t i = 0; i < kFrameCacheSlots; ++i) {
    const FrameCacheSlot& slot = gFrameCache[i];
    if (slot.valid && slot.frames == frames && slot.frameIndex == frameIndex) {
      return slot.data;
    }
  }

  FrameCacheSlot& slot = gFrameCache[gFrameCacheNext];
  gFrameCacheNext = static_cast<uint8_t>((gFrameCacheNext + 1U) % kFrameCacheSlots);

  uint16_t decoded = 0;
  uint16_t encoded = 0;
  while ((encoded + 1U) < frameBytes && decoded < decodedBytes) {
    const uint8_t runLen = pgm_read_byte(frame + encoded);
    if (runLen == 0) {
      break;
    }
    const uint8_t runValue = pgm_read_byte(frame + encoded + 1U);
    uint16_t len = runLen;
    if (static_cast<uint16_t>(decoded + len) > decodedBytes) {
      len = static_cast<uint16_t>(decodedBytes - decoded);
    }
    for (uint16_t i = 0; i < len; ++i) {
      slot.data[decoded + i] = runValue;
    }
    decoded = static_cast<uint16_t>(decoded + len);
    encoded = static_cast<uint16_t>(encoded + 2U);
  }
  // 编码提前结束时剩余部分按 0 处理，与逐行解码的零填充行为一致。
  for (uint16_t i = decoded; i < decodedBytes; ++i) {
    slot.data[i] = 0;
  }

  slot.frames = frames;
  slot.frameIndex = frameIndex;
  slot.valid = true;
  return slot.data;
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

  // 逐行解码只作为超出缓存容量的兜底路径，例如全屏背景这类大图。
  const uint32_t decodedFrameBytes = static_cast<uint32_t>(srcStride) * anim.frameHeight;
  const uint8_t* decodedFrame =
      (rleEncoded && decodedFrameBytes <= kFrameCacheSlotBytes)
          ? decodeFrameCached(anim.frames, idx, frame, frameBytes,
                              static_cast<uint16_t>(decodedFrameBytes))
          : nullptr;

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
      const uint8_t* rowSource = nullptr;
      if (decodedFrame != nullptr) {
        rowSource = decodedFrame + rowOffset;
      } else if (rleEncoded) {
        if (srcStride > sizeof(decodedRow)) {
          return;
        }
        if (cachedRow != sy) {
          decodeRleSpan(frame, frameBytes, rowOffset, srcStride, decodedRow);
          cachedRow = sy;
        }
        rowSource = decodedRow;
      } else {
        rowSource = frame + rowOffset;
      }

      uint32_t colAccum = 0;
      uint16_t sx = 0;
      for (int16_t dx = 0; dx < dstW; ++dx) {
        const uint16_t byteIndex = static_cast<uint16_t>(sx >> 3);
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x7U));
        const bool on = (rowSource[byteIndex] & bitMask) != 0;
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
