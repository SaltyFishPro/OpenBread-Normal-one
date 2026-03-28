#include "HomePage.h"

#include "../ThemeMono.h"
#include "../../bsp/BoardConfig.h"
#include "../assets/UI_background.h"
#include "../assets/icons8-book.h"
#include "../assets/icons8-clock.h"
#include "../assets/icons8-itunes.h"
#include "../assets/icons8-settings.h"
#include "../assets/icons8-wifi.h"

namespace {
constexpr int16_t kWheelFrameWidth = 190;
constexpr int16_t kWheelFrameRightMargin = 8;
constexpr int16_t kWheelItemSpacing = 56;
constexpr int16_t kWheelItemInsetX = 10;
constexpr int16_t kWheelTextGap = 14;
constexpr int16_t kWheelFramePaddingY = 8;
constexpr int16_t kMenuClipTop = 10;
constexpr int16_t kMenuClipBottom = 160;
constexpr int16_t kMenuLabelHeight = 13;

struct IconAnim {
  const uint8_t* frames;
  uint16_t frameBytes;
  uint16_t frameWidth;
  uint16_t frameHeight;
  uint16_t frameDelayMs;
  uint16_t frameCount;
};

const IconAnim kMenuIcons[5] = {
    {reinterpret_cast<const uint8_t*>(&itunes_frames[0][0]), ITUNES_FRAME_BYTES,
     ITUNES_FRAME_WIDTH, ITUNES_FRAME_HEIGHT, ITUNES_FRAME_DELAY, ITUNES_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&book_frames[0][0]), BOOK_FRAME_BYTES,
     BOOK_FRAME_WIDTH, BOOK_FRAME_HEIGHT, BOOK_FRAME_DELAY, BOOK_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&clock_frames[0][0]), CLOCK_FRAME_BYTES,
     CLOCK_FRAME_WIDTH, CLOCK_FRAME_HEIGHT, CLOCK_FRAME_DELAY, CLOCK_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&wifi_frames[0][0]), WIFI_FRAME_BYTES,
     WIFI_FRAME_WIDTH, WIFI_FRAME_HEIGHT, WIFI_FRAME_DELAY, WIFI_FRAME_COUNT},
    {reinterpret_cast<const uint8_t*>(&setting_frames[0][0]), SETTING_FRAME_BYTES,
     SETTING_FRAME_WIDTH, SETTING_FRAME_HEIGHT, SETTING_FRAME_DELAY, SETTING_FRAME_COUNT},
};

const IconAnim kHomeBackground = {
    reinterpret_cast<const uint8_t*>(&ui_background_frames[0][0]),
    UI_BACKGROUND_FRAME_BYTES,
    UI_BACKGROUND_FRAME_WIDTH,
    UI_BACKGROUND_FRAME_HEIGHT,
    UI_BACKGROUND_FRAME_DELAY,
    UI_BACKGROUND_FRAME_COUNT};

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

void drawIconFrame(ST7305_2p9_BW_DisplayDriver& canvas, const IconAnim& anim,
                   uint16_t frameIndex, int16_t dstX, int16_t dstY, int16_t dstW,
                   int16_t dstH, bool invert, int16_t clipTop, int16_t clipBottom) {
  if (anim.frameCount == 0 || dstW <= 0 || dstH <= 0) {
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

    const uint16_t sy = static_cast<uint16_t>((static_cast<uint32_t>(dy) * anim.frameHeight) / dstH);
    for (int16_t dx = 0; dx < dstW; ++dx) {
      const uint16_t sx = static_cast<uint16_t>((static_cast<uint32_t>(dx) * anim.frameWidth) / dstW);
      const uint16_t byteIndex = static_cast<uint16_t>(sy * srcStride + (sx >> 3));
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x7U));
      const bool on = (pgm_read_byte(frame + byteIndex) & bitMask) != 0;
      writeLogicalPixel(canvas, dstX + dx, py, invert ? !on : on);
    }
  }
}
}  // namespace

bool HomePage::begin() {
  focusIndex_ = 0;
  targetIndex_ = 0;
  slideState_ = SlideState::Idle;
  animFromOffsetY_ = 0;
  animToOffsetY_ = 0;
  animStartMs_ = millis();
  lastFocusFrame_ = 0;
  return true;
}

bool HomePage::handleInput(bool upEdge, bool downEdge, bool okEdge, uint32_t nowMs) {
  if (okEdge && slideState_ == SlideState::Idle) {
    return true;
  }

  int8_t direction = 0;
  if (upEdge) {
    direction = -1;
  } else if (downEdge) {
    direction = 1;
  }

  if (direction == 0) {
    return false;
  }

  if (slideState_ == SlideState::Sliding) {
    focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetY_ = 0;
    animToOffsetY_ = 0;
  }

  beginSlide(direction, nowMs);
  return false;
}

void HomePage::beginSlide(int8_t direction, uint32_t nowMs) {
  targetIndex_ = static_cast<uint8_t>((focusIndex_ + kMenuCount + direction) % kMenuCount);
  animFromOffsetY_ = 0;
  animToOffsetY_ = static_cast<int16_t>(-direction * kWheelItemSpacing);
  animStartMs_ = nowMs;
  slideState_ = SlideState::Sliding;
}

void HomePage::update(uint32_t nowMs) {
  if (slideState_ != SlideState::Sliding) {
    return;
  }

  if (nowMs - animStartMs_ >= ThemeMono::kSlideDurationMs) {
    focusIndex_ = targetIndex_;
    slideState_ = SlideState::Idle;
    animFromOffsetY_ = 0;
    animToOffsetY_ = 0;
  }
}

bool HomePage::isSliding() const { return slideState_ == SlideState::Sliding; }

int16_t HomePage::currentMenuOffset(uint32_t nowMs) const {
  if (slideState_ != SlideState::Sliding) {
    return 0;
  }

  const float t = static_cast<float>(nowMs - animStartMs_) /
                  static_cast<float>(ThemeMono::kSlideDurationMs);
  const float clamped = t > 1.0f ? 1.0f : t;
  return easeOutCubic(animFromOffsetY_, animToOffsetY_, clamped);
}

bool HomePage::hasAnimationTick(uint32_t nowMs) const {
  const IconAnim& anim = kMenuIcons[focusIndex_];
  if (anim.frameCount == 0 || anim.frameDelayMs == 0) {
    return false;
  }
  const uint16_t frame = static_cast<uint16_t>((nowMs / anim.frameDelayMs) % anim.frameCount);
  return frame != lastFocusFrame_;
}

uint8_t HomePage::focusIndex() const { return focusIndex_; }

const char* HomePage::focusName() const { return menuNames_[focusIndex_]; }

void HomePage::render(DisplayMonoTft& display, int16_t pageOffsetX, bool confirmPulse,
                      uint32_t nowMs) {
  renderTransition(display, pageOffsetX, pageOffsetX, 0, 0, 0, confirmPulse, nowMs);
}

void HomePage::renderTransition(DisplayMonoTft& display, int16_t backgroundOffsetX,
                                int16_t menuBaseOffsetX, int16_t menuUpExtraOffsetX,
                                int16_t menuFocusExtraOffsetX,
                                int16_t menuDownExtraOffsetX, bool confirmPulse,
                                uint32_t nowMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t menuOffsetY = currentMenuOffset(nowMs);
  const int16_t frameBaseX = static_cast<int16_t>(
      menuBaseOffsetX + width - kWheelFrameWidth - kWheelFrameRightMargin);
  const int16_t centerY = static_cast<int16_t>(height / 2);

  const uint16_t bgFrame =
      static_cast<uint16_t>((nowMs / kHomeBackground.frameDelayMs) %
                            kHomeBackground.frameCount);
  drawIconFrame(canvas, kHomeBackground, bgFrame, backgroundOffsetX, 0, width, height,
                false, 0, static_cast<int16_t>(height - 1));

  text.setFont(u8g2_font_8x13B_mf);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  for (uint8_t i = 0; i < kMenuCount; ++i) {
    int8_t delta = static_cast<int8_t>(i) - static_cast<int8_t>(focusIndex_);
    if (delta > 2) {
      delta -= kMenuCount;
    } else if (delta < -2) {
      delta += kMenuCount;
    }

    const bool isFocus = (delta == 0);
    const IconAnim& anim = kMenuIcons[i];

    int16_t iconSize = isFocus ? static_cast<int16_t>(ThemeMono::kIconFocusSize)
                               : static_cast<int16_t>(ThemeMono::kIconSideSize);
    if (confirmPulse && isFocus) {
      iconSize = static_cast<int16_t>(iconSize + 2);
    }

    const int16_t rowCenterY =
        static_cast<int16_t>(centerY + delta * kWheelItemSpacing + menuOffsetY);
    const int16_t rowTop = static_cast<int16_t>(rowCenterY - iconSize / 2);
    const int16_t rowBottom = static_cast<int16_t>(rowTop + iconSize - 1);
    if (rowBottom < kMenuClipTop || rowTop > kMenuClipBottom) {
      continue;
    }

    int16_t rowOffsetX = 0;
    if (delta == -1) {
      rowOffsetX = menuUpExtraOffsetX;
    } else if (delta == 0) {
      rowOffsetX = menuFocusExtraOffsetX;
    } else if (delta == 1) {
      rowOffsetX = menuDownExtraOffsetX;
    }

    const int16_t frameX = static_cast<int16_t>(frameBaseX + rowOffsetX);

    if (isFocus) {
      int16_t focusTop = static_cast<int16_t>(rowCenterY - (iconSize / 2 + kWheelFramePaddingY));
      int16_t focusBottom = static_cast<int16_t>(rowCenterY + (iconSize / 2 + kWheelFramePaddingY));
      if (focusTop < kMenuClipTop) {
        focusTop = kMenuClipTop;
      }
      if (focusBottom > kMenuClipBottom) {
        focusBottom = kMenuClipBottom;
      }
      canvas.drawRectangle(frameX, focusTop, frameX + kWheelFrameWidth, focusBottom,
                           ST7305_COLOR_BLACK);
    }

    const uint16_t frame = static_cast<uint16_t>((nowMs / anim.frameDelayMs) % anim.frameCount);
    if (isFocus) {
      lastFocusFrame_ = frame;
    }

    const int16_t iconX = static_cast<int16_t>(frameX + kWheelItemInsetX);
    const int16_t iconY = rowTop;
    drawIconFrame(canvas, anim, frame, iconX, iconY, iconSize, iconSize, false,
                  kMenuClipTop, kMenuClipBottom);

    const int16_t labelW = text.getUTF8Width(menuNames_[i]);
    const int16_t textBlockX = static_cast<int16_t>(iconX + iconSize + kWheelTextGap);
    const int16_t textBlockW = static_cast<int16_t>(frameX + kWheelFrameWidth - textBlockX - kWheelItemInsetX);
    int16_t labelX = static_cast<int16_t>(textBlockX + (textBlockW - labelW) / 2);
    if (labelX < textBlockX) {
      labelX = textBlockX;
    }
    const int16_t labelY = static_cast<int16_t>(rowCenterY + 6);
    if (labelY >= kMenuClipTop + kMenuLabelHeight && labelY <= kMenuClipBottom) {
      text.drawUTF8(labelX, labelY, menuNames_[i]);
    }
  }
}

int16_t HomePage::easeOutCubic(int16_t from, int16_t to, float t) const {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  const float inv = 1.0f - t;
  const float eased = 1.0f - (inv * inv * inv);
  return static_cast<int16_t>(from + (to - from) * eased);
}
