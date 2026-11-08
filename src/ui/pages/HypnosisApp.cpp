#include "HypnosisApp.h"

#include <cmath>

#include "../../bsp/DisplayMonoTft.h"

namespace {
constexpr int16_t kWidth = 384;
constexpr int16_t kHeight = 168;
constexpr int16_t kCenterX = kWidth / 2;
constexpr int16_t kCenterY = kHeight / 2;
constexpr int16_t kBandWidth = 24;
constexpr int16_t kBaseRadius = 30;
constexpr int16_t kMaxVisibleRadius = 211;
constexpr uint32_t kFrameIntervalMs = 48;

void fillCircleClipped(ST7305_2p9_BW_DisplayDriver& canvas, int16_t centerY, int16_t yOffset,
                       int16_t radius, uint16_t color) {
  const int32_t radiusSquared = static_cast<int32_t>(radius) * radius;
  const int16_t top = centerY - radius < 0 ? 0 : centerY - radius;
  const int16_t bottom = centerY + radius >= kHeight ? kHeight - 1 : centerY + radius;
  for (int16_t y = top; y <= bottom; ++y) {
    const int32_t dy = y - centerY;
    const int16_t half = static_cast<int16_t>(sqrtf(static_cast<float>(radiusSquared - dy * dy)));
    const int16_t left = kCenterX - half < 0 ? 0 : kCenterX - half;
    const int16_t right = kCenterX + half >= kWidth ? kWidth - 1 : kCenterX + half;
    canvas.drawFastHLine(left, y + yOffset, right - left + 1, color);
  }
}

void drawHeart(ST7305_2p9_BW_DisplayDriver& canvas, int16_t yOffset,
               int16_t halfWidth, int16_t top, uint16_t color) {
  const int16_t lobeRadius = halfWidth / 2 + 2;
  const int16_t lobeCenterY = top + lobeRadius;
  const int16_t lobeOffset = halfWidth - lobeRadius;
  const int16_t pointY = top + halfWidth * 2 - 4;
  for (int16_t y = top; y <= pointY; ++y) {
    if (y < lobeCenterY) {
      const int16_t dy = y - lobeCenterY;
      const int16_t dx = static_cast<int16_t>(sqrtf(static_cast<float>(
          lobeRadius * lobeRadius - dy * dy)));
      for (int8_t side = -1; side <= 1; side += 2) {
        const int16_t center = kCenterX + side * lobeOffset;
        canvas.drawFastHLine(center - dx, y + yOffset, 2 * dx + 1, color);
      }
    } else {
      const int16_t half = static_cast<int16_t>(
          static_cast<int32_t>(halfWidth) * (pointY - y) / (pointY - lobeCenterY));
      canvas.drawFastHLine(kCenterX - half, y + yOffset, 2 * half + 1, color);
    }
  }
}
}  // namespace

void HypnosisApp::enter(uint32_t nowMs) {
  open_ = true;
  replay(nowMs);
}

void HypnosisApp::exit() {
  open_ = false;
  changed_ = true;
}

void HypnosisApp::replay(uint32_t nowMs) {
  if (!open_) return;
  startedMs_ = nowMs;
  lastFrame_ = 0;
  changed_ = true;
}

void HypnosisApp::tick(uint32_t nowMs) {
  if (!open_) return;
  const uint32_t elapsed = nowMs - startedMs_;
  const uint32_t frame = elapsed / kFrameIntervalMs;
  if (frame != lastFrame_) {
    lastFrame_ = frame;
    changed_ = true;
  }
}

bool HypnosisApp::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void HypnosisApp::render(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const {
  if (!open_) return;
  auto& canvas = display.canvas();
  const uint32_t outerRadius = kBaseRadius + (nowMs - startedMs_) / 32U;
  const uint32_t firstBand = outerRadius > kMaxVisibleRadius
                                 ? (outerRadius - kMaxVisibleRadius + kBandWidth - 1U) / kBandWidth
                                 : 0U;
  const uint16_t background = firstBand == 0U || (firstBand & 1U) == 0U
                                  ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;

  for (int16_t y = 0; y < kHeight; ++y) {
    canvas.drawFastHLine(0, y + yOffset, kWidth, background);
  }
  // Fully off-screen bands become the background; visible bands keep expanding.
  for (uint32_t band = firstBand; band * kBandWidth < outerRadius; ++band) {
    const int16_t radius = static_cast<int16_t>(outerRadius - band * kBandWidth);
    const uint16_t color = (band & 1U) ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
    fillCircleClipped(canvas, kCenterY, yOffset, radius, color);
  }
  drawHeart(canvas, yOffset, 28, 57, ST7305_COLOR_BLACK);
  drawHeart(canvas, yOffset, 21, 64, ST7305_COLOR_WHITE);
}
