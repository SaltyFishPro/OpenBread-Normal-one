#pragma once

#include <stdint.h>

// Shared easing/interpolation helpers. Pages previously reimplemented the same
// cubic curves (float and fixed-point) in four different files.
namespace AnimMath {

// Fixed-point animations use this scale so progress stays integer-only.
constexpr uint16_t kFixedScale = 1000;

inline float clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

inline float easeInCubic(float t) {
  const float value = clamp01(t);
  return value * value * value;
}

inline float easeOutCubic(float t) {
  const float value = clamp01(t);
  const float inverse = 1.0f - value;
  return 1.0f - inverse * inverse * inverse;
}

// Truncating interpolation preserves the historical UiManager/HomePage results.
inline int16_t lerpInt16Trunc(int16_t from, int16_t to, float t) {
  return static_cast<int16_t>(from + (to - from) * t);
}

// Rounded interpolation preserves the historical MusicPage list animation.
inline int16_t lerpInt16Rounded(int16_t from, int16_t to, float t) {
  return static_cast<int16_t>(from + (to - from) * t + ((to >= from) ? 0.5f : -0.5f));
}

inline int16_t easeInCubicInt16(int16_t from, int16_t to, float t) {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  return lerpInt16Trunc(from, to, easeInCubic(t));
}

inline int16_t easeOutCubicInt16(int16_t from, int16_t to, float t) {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  return lerpInt16Trunc(from, to, easeOutCubic(t));
}

inline int16_t easeOutCubicRoundedInt16(int16_t from, int16_t to, float t) {
  return lerpInt16Rounded(from, to, easeOutCubic(t));
}

inline uint16_t fixedClampProgress(uint32_t elapsedMs, uint32_t durationMs) {
  if (durationMs == 0U || elapsedMs >= durationMs) {
    return kFixedScale;
  }
  return static_cast<uint16_t>((elapsedMs * kFixedScale) / durationMs);
}

inline uint16_t fixedEaseInOut(uint16_t progress) {
  const uint32_t value = progress > kFixedScale ? kFixedScale : progress;
  const uint32_t eased = (value * value * (3U * kFixedScale - 2U * value)) /
                         (kFixedScale * kFixedScale);
  return static_cast<uint16_t>(eased > kFixedScale ? kFixedScale : eased);
}

inline uint16_t fixedEaseOutCubic(uint16_t progress) {
  const uint16_t value = progress > kFixedScale ? kFixedScale : progress;
  const int32_t inverse = static_cast<int32_t>(kFixedScale - value);
  const int64_t eased = static_cast<int64_t>(kFixedScale) -
                        (static_cast<int64_t>(inverse) * inverse * inverse) /
                            (static_cast<int64_t>(kFixedScale) * kFixedScale);
  if (eased < 0) {
    return 0;
  }
  if (eased > kFixedScale) {
    return kFixedScale;
  }
  return static_cast<uint16_t>(eased);
}

}  // namespace AnimMath
