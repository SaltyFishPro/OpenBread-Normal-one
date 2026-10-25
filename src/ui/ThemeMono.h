#pragma once

#include <stdint.h>

namespace ThemeMono {
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kCardWidth = 72;
constexpr int16_t kCardHeight = 82;
constexpr int16_t kCardSpacing = 96;
constexpr int16_t kCornerLen = 12;
constexpr int16_t kCornerGap = 3;

constexpr uint16_t kSlideDurationMs = 240;
constexpr uint16_t kTransitionDurationMs = 180;

constexpr uint16_t kIconFocusSize = 42;
// Match the XMB preview's ghost category cells. The focused icon remains 42px;
// side categories use a compact 30px raster so their centers stay on the 70px rail.
constexpr uint16_t kIconSideSize = 30;
}
