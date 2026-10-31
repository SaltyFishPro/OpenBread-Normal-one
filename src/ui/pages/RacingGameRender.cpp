#include "RacingGame.h"

#include <cmath>
#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "RacingCarMesh.h"

namespace {
constexpr int16_t kScreenWidth = 384;
constexpr int16_t kScreenHeight = 168;
constexpr int16_t kHorizon = 67;
constexpr uint8_t kDrawDistance = 40;
constexpr float kSegmentLength = 200.0f;
constexpr float kTrackLength = 40000.0f;
constexpr float kCameraDepth = 0.84f;
constexpr float kRoadHalfWidth = 2000.0f;

struct Projected {
  int16_t x = 0;
  int16_t y = 0;
  int16_t half = 0;
  float scale = 0.0f;
};

int16_t bounded(int32_t value, int16_t low, int16_t high) {
  return value < low ? low : (value > high ? high : static_cast<int16_t>(value));
}

void rectangle(ST7305_2p9_BW_DisplayDriver& canvas, int32_t x0, int32_t y0,
               int32_t x1, int32_t y1, uint16_t color) {
  if (x1 < 0 || x0 >= kScreenWidth || y1 < 0 || y0 >= kScreenHeight) return;
  x0 = bounded(x0, 0, kScreenWidth - 1);
  x1 = bounded(x1, 0, kScreenWidth - 1);
  y0 = bounded(y0, 0, kScreenHeight - 1);
  y1 = bounded(y1, 0, kScreenHeight - 1);
  if (x0 > x1 || y0 > y1) return;
  for (int32_t y = y0; y <= y1; ++y) {
    canvas.drawFastHLine(x0, y, x1 - x0 + 1, color);
  }
}

void quad(ST7305_2p9_BW_DisplayDriver& canvas, int x0, int y0, int x1, int y1,
          int x2, int y2, int x3, int y3, uint16_t color) {
  if (y0 != y1 || y2 != y3) return;
  if (y0 == y2) {
    canvas.drawFastHLine(bounded(x0, -3000, 3000), y0,
                         bounded(x1 - x0 + 1, 0, 6000), color);
    return;
  }
  const int firstY = bounded(y0 < y2 ? y0 : y2, 0, kScreenHeight - 1);
  const int lastY = bounded(y0 > y2 ? y0 : y2, 0, kScreenHeight - 1);
  if (y0 < 0 && y2 < 0) return;
  if (y0 >= kScreenHeight && y2 >= kScreenHeight) return;
  for (int y = firstY; y <= lastY; ++y) {
    const int left = x0 + (x3 - x0) * (y - y0) / (y2 - y0);
    const int right = x1 + (x2 - x1) * (y - y0) / (y2 - y0);
    const int start = bounded(left < right ? left : right, 0, kScreenWidth - 1);
    const int end = bounded(left > right ? left : right, 0, kScreenWidth - 1);
    if (left >= kScreenWidth && right >= kScreenWidth) continue;
    if (left < 0 && right < 0) continue;
    canvas.drawFastHLine(start, y, end - start + 1, color);
  }
}

void patternedSpan(ST7305_2p9_BW_DisplayDriver& canvas, int start, int end,
                   int y, uint8_t darkQuarters) {
  canvas.drawFastHLine(start, y, end - start + 1, ST7305_COLOR_WHITE);
  if (darkQuarters == 0U) return;
  if (darkQuarters >= 4U) {
    canvas.drawFastHLine(start, y, end - start + 1, ST7305_COLOR_BLACK);
    return;
  }
  const int period = 8;
  const int run = darkQuarters * 2;
  const int phase = (y & 2) ? period / 2 : 0;
  for (int x = start - ((start - phase + period) % period); x <= end; x += period) {
    const int x0 = x < start ? start : x;
    const int x1 = x + run - 1 > end ? end : x + run - 1;
    if (x1 >= x0) canvas.drawFastHLine(x0, y, x1 - x0 + 1, ST7305_COLOR_BLACK);
  }
}

void stripedQuad(ST7305_2p9_BW_DisplayDriver& canvas, int x0, int y0, int x1,
                 int x2, int y2, int x3, uint8_t darkRows) {
  if (y0 == y2) return;
  if ((y0 < 0 && y2 < 0) || (y0 >= kScreenHeight && y2 >= kScreenHeight)) return;
  const int firstY = bounded(y0 < y2 ? y0 : y2, 0, kScreenHeight - 1);
  const int lastY = bounded(y0 > y2 ? y0 : y2, 0, kScreenHeight - 1);
  for (int y = firstY; y <= lastY; ++y) {
    const int left = x0 + (x3 - x0) * (y - y0) / (y2 - y0);
    const int right = x1 + (x2 - x1) * (y - y0) / (y2 - y0);
    if ((left < 0 && right < 0) || (left >= kScreenWidth && right >= kScreenWidth)) continue;
    const int start = bounded(left < right ? left : right, 0, kScreenWidth - 1);
    const int end = bounded(left > right ? left : right, 0, kScreenWidth - 1);
    patternedSpan(canvas, start, end, y, darkRows);
  }
}

void poly4(ST7305_2p9_BW_DisplayDriver& canvas, const int x[4], const int y[4],
           uint8_t darkQuarters) {
  int firstY = y[0];
  int lastY = y[0];
  for (int i = 1; i < 4; ++i) {
    if (y[i] < firstY) firstY = y[i];
    if (y[i] > lastY) lastY = y[i];
  }
  if (lastY < 0 || firstY >= kScreenHeight) return;
  firstY = bounded(firstY, 0, kScreenHeight - 1);
  lastY = bounded(lastY, 0, kScreenHeight - 1);
  for (int row = firstY; row <= lastY; ++row) {
    int intersections[4];
    uint8_t count = 0;
    for (uint8_t edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
      const uint8_t next = (edgeIndex + 1U) & 3U;
      if ((y[edgeIndex] <= row && y[next] > row) ||
          (y[next] <= row && y[edgeIndex] > row)) {
        intersections[count++] = x[edgeIndex] +
            (row - y[edgeIndex]) * (x[next] - x[edgeIndex]) /
                (y[next] - y[edgeIndex]);
      }
    }
    if (count < 2U) continue;
    int left = intersections[0];
    int right = intersections[1];
    if (left > right) { const int temp = left; left = right; right = temp; }
    if (right < 0 || left >= kScreenWidth) continue;
    const int start = bounded(left, 0, kScreenWidth - 1);
    const int end = bounded(right, 0, kScreenWidth - 1);
    patternedSpan(canvas, start, end, row, darkQuarters);
  }
}

void car(ST7305_2p9_BW_DisplayDriver& canvas, int x, int bottom, int width, int height,
         bool player) {
  if (bottom < 26 || bottom - height >= kScreenHeight || x + width < 0 || x - width >= kScreenWidth) return;
  const int half = width / 2;
  rectangle(canvas, x - half - 2, bottom - height / 2, x + half + 2, bottom, ST7305_COLOR_WHITE);
  rectangle(canvas, x - half, bottom - height / 2 + 2, x + half, bottom - 2, ST7305_COLOR_BLACK);
  quad(canvas, x - half / 2, bottom - height, x + half / 2, bottom - height,
       x + half, bottom - height / 2, x - half, bottom - height / 2, ST7305_COLOR_WHITE);
  rectangle(canvas, x - half / 3, bottom - height + 2, x + half / 3,
            bottom - height / 2 - 2, ST7305_COLOR_BLACK);
  if (player || width > 9) {
    rectangle(canvas, x - half + 2, bottom - 5, x - half + 5, bottom - 3, ST7305_COLOR_WHITE);
    rectangle(canvas, x + half - 5, bottom - 5, x + half - 2, bottom - 3, ST7305_COLOR_WHITE);
  }
}

void scenery(ST7305_2p9_BW_DisplayDriver& canvas, uint8_t kind, int x, int bottom, int size) {
  if (size < 3 || x < -size || x >= kScreenWidth + size || bottom < 26) return;
  size = bounded(size, 3, 80);
  if (kind == 1U) {
    rectangle(canvas, x - 1, bottom - size / 3, x + 1, bottom, ST7305_COLOR_BLACK);
    const int crownTop = bottom - size;
    const int crownBottom = bottom - size / 4;
    for (int y = crownTop; y <= crownBottom; ++y) {
      const int half = (y - crownTop) * size / (3 * (crownBottom - crownTop));
      canvas.drawFastHLine(x - half, y, half * 2 + 1, ST7305_COLOR_BLACK);
    }
  } else if (kind == 2U) {
    rectangle(canvas, x - 1, bottom - size / 2, x + 1, bottom, ST7305_COLOR_BLACK);
    canvas.drawFilledCircle(x, bottom - size * 2 / 3, size / 3, ST7305_COLOR_BLACK);
  } else if (kind == 3U) {
    canvas.drawFilledCircle(x, bottom - size / 4, size / 4, ST7305_COLOR_BLACK);
  } else {
    rectangle(canvas, x - size / 3, bottom - size / 3,
              x + size / 3, bottom, ST7305_COLOR_BLACK);
  }
}

void building(ST7305_2p9_BW_DisplayDriver& canvas, int roadEdge, int bottom,
              int width, int height, bool left, uint8_t style) {
  if (height < 4 || width < 3) return;
  const int nearEdge = left ? roadEdge - 4 : roadEdge + 4;
  const int farEdge = left ? nearEdge - width : nearEdge + width;
  const int x0 = left ? farEdge : nearEdge;
  const int x1 = left ? nearEdge : farEdge;
  rectangle(canvas, x0, bottom - height, x1, bottom, ST7305_COLOR_BLACK);
  if (height > 10 && width > 8) {
    const int spacing = style & 1U ? 8 : 12;
    for (int y = bottom - height + 5; y < bottom - 3; y += spacing) {
      rectangle(canvas, x0 + 3, y, x1 - 3, y + 1, ST7305_COLOR_WHITE);
    }
  }
  rectangle(canvas, x0 - 1, bottom - height - 2, x1 + 1, bottom - height,
            ST7305_COLOR_BLACK);
}
}  // namespace

void RacingGame::render(DisplayMonoTft& display, int16_t yOffset) const {
  drawSky(display, yOffset);
  drawRoad(display, yOffset);
  drawPlayer(display, yOffset);
  drawHud(display, yOffset);
  if (paused_) {
    auto& canvas = display.canvas();
    rectangle(canvas, 91, yOffset + 59, 293, yOffset + 105, ST7305_COLOR_WHITE);
    canvas.drawRectangle(91, yOffset + 59, 293, yOffset + 105, ST7305_COLOR_BLACK);
    auto& text = display.text();
    text.setFont(u8g2_font_7x14_tf);
    text.setForegroundColor(ST7305_COLOR_BLACK);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.setFontMode(1);
    text.drawUTF8(166, yOffset + 77, "PAUSED");
    text.drawUTF8(105, yOffset + 97, "OK:PLAY   LEFT:EXIT");
  }
}

void RacingGame::drawSky(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  if (segments_[segmentAt(playerTrackPosition())].tunnel) {
    rectangle(canvas, 0, yOffset, kScreenWidth - 1,
              yOffset + kScreenHeight - 1, ST7305_COLOR_BLACK);
    return;
  }
  const bool night = lightPhase_ == 2U;
  rectangle(canvas, 0, yOffset, kScreenWidth - 1, yOffset + kHorizon,
            night ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  if (night) {
    for (uint8_t i = 0; i < 18; ++i) {
      const int x = (i * 71 + static_cast<int>(skyOffset_)) % kScreenWidth;
      const int y = 27 + (i * 23) % (kHorizon - 28);
      canvas.drawFastHLine(x < 0 ? x + kScreenWidth : x, y + yOffset, 1, ST7305_COLOR_WHITE);
    }
  } else {
    for (int y = yOffset + 26; y < yOffset + kHorizon; ++y) {
      if ((y & 3) == 0 || (lightPhase_ == 1U && (y & 3) == 2)) {
        canvas.drawFastHLine(0, y, kScreenWidth, ST7305_COLOR_BLACK);
      }
    }
    const int sunX = (285 + static_cast<int>(skyOffset_ * 0.25f)) % kScreenWidth;
    canvas.drawFilledCircle(sunX < 0 ? sunX + kScreenWidth : sunX,
                            yOffset + kHorizon - 11, 11, ST7305_COLOR_WHITE);
    canvas.drawCircle(sunX < 0 ? sunX + kScreenWidth : sunX,
                      yOffset + kHorizon - 11, 11, ST7305_COLOR_BLACK);
  }
  const int shift = static_cast<int>(skyOffset_ * 0.35f) % 33;
  for (int x = -40 + shift; x < kScreenWidth + 40; x += 33) {
    const int height = 10 + ((x * x + 13) & 23);
    rectangle(canvas, x, yOffset + kHorizon - height, x + 26,
              yOffset + kHorizon, ST7305_COLOR_BLACK);
    if (height > 15) {
      for (int wy = yOffset + kHorizon - height + 5; wy < yOffset + kHorizon - 2; wy += 7) {
        for (int wx = x + 4; wx < x + 25; wx += 7) {
          if (((wx + wy) & 3) != 0) canvas.drawFastHLine(wx, wy, 2, ST7305_COLOR_WHITE);
        }
      }
    }
  }
  rectangle(canvas, 0, yOffset + kHorizon + 1, kScreenWidth - 1,
            yOffset + kScreenHeight - 1, ST7305_COLOR_WHITE);
}

void RacingGame::drawRoad(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  Projected points[kDrawDistance + 1]{};
  const uint16_t base = segmentAt(position_);
  const float offset = fmodf(position_, kSegmentLength);
  const float playerPosition = playerTrackPosition();
  const uint16_t playerSegment = segmentAt(playerPosition);
  const float cameraY = segments_[playerSegment].height + 1000.0f;
  float curveX = 0.0f;
  float curveDelta = -segments_[base].curve * offset / kSegmentLength;
  for (uint8_t n = 1; n <= kDrawDistance; ++n) {
    const uint16_t index = (base + n) % kSegmentCount;
    const float depth = n * kSegmentLength - offset;
    curveX += curveDelta;
    curveDelta += segments_[index].curve;
    if (depth < 80.0f) continue;
    const float scale = kCameraDepth / depth;
    Projected& point = points[n];
    point.scale = scale;
    point.x = bounded(static_cast<int32_t>(kScreenWidth / 2 +
                      scale * (curveX - playerX_ * kRoadHalfWidth) * (kScreenWidth / 2)), -3000, 3000);
    point.y = bounded(static_cast<int32_t>(yOffset + kHorizon +
                      scale * (cameraY - segments_[index].height) * 84.0f), -1000, 1000);
    point.half = bounded(static_cast<int32_t>(scale * kRoadHalfWidth * (kScreenWidth / 2)), 0, 3000);
  }

  for (uint8_t n = kDrawDistance; n > 1; --n) {
    const Projected& far = points[n];
    const Projected& near = points[n - 1];
    if (far.scale == 0.0f || near.scale == 0.0f) continue;
    if (far.y >= near.y || far.y >= kScreenHeight + yOffset) continue;
    const uint16_t index = (base + n) % kSegmentCount;
    const Segment& segment = segments_[index];
    const int top = far.y;
    const int bottom = near.y < kScreenHeight + yOffset ? near.y : kScreenHeight + yOffset;
    if (bottom <= top) continue;

    if (segment.tunnel) {
      const int ceilingFar = far.y - bounded(far.half * 3 / 2, 8, 115);
      const int ceilingNear = near.y - bounded(near.half * 3 / 2, 12, 160);
      quad(canvas, near.x - near.half, ceilingNear, near.x + near.half, ceilingNear,
           far.x + far.half, ceilingFar, far.x - far.half, ceilingFar, ST7305_COLOR_BLACK);
      const int leftX[4] = {near.x - near.half, far.x - far.half,
                            far.x - far.half, near.x - near.half};
      const int rightX[4] = {near.x + near.half, far.x + far.half,
                             far.x + far.half, near.x + near.half};
      const int wallY[4] = {ceilingNear, ceilingFar, far.y, near.y};
      poly4(canvas, leftX, wallY, 3U);
      poly4(canvas, rightX, wallY, 3U);
      canvas.drawLine(near.x - near.half, near.y, far.x - far.half, far.y, ST7305_COLOR_WHITE);
      canvas.drawLine(near.x + near.half, near.y, far.x + far.half, far.y, ST7305_COLOR_WHITE);
      canvas.drawLine(near.x - near.half, ceilingNear, far.x - far.half,
                      ceilingFar, ST7305_COLOR_WHITE);
      canvas.drawLine(near.x + near.half, ceilingNear, far.x + far.half,
                      ceilingFar, ST7305_COLOR_WHITE);
      if (index % 4U == 0U) {
        quad(canvas, near.x - bounded(near.half / 7, 2, 24), ceilingNear,
             near.x + bounded(near.half / 7, 2, 24), ceilingNear,
             far.x + bounded(far.half / 7, 2, 24), ceilingFar,
             far.x - bounded(far.half / 7, 2, 24), ceilingFar, ST7305_COLOR_WHITE);
      }
    } else {
      if ((segment.building & 1U) && index % 3U == 0U) {
        building(canvas, near.x - near.half * 3 / 2, near.y,
                 bounded(near.half / 2, 3, 90), bounded(near.half, 6, 120), true, index);
      }
      if ((segment.building & 2U) && index % 3U == 0U) {
        building(canvas, near.x + near.half * 3 / 2, near.y,
                 bounded(near.half / 2, 3, 90), bounded(near.half, 6, 120), false, index);
      }
    }

    const uint8_t darkRows = segment.tunnel ? 3U : ((index / 3U) & 1U ? 2U : 1U);
    stripedQuad(canvas, far.x - far.half, top, far.x + far.half,
                near.x + near.half, bottom, near.x - near.half, darkRows);
    const int farCurb = far.half / 14 + 1;
    const int nearCurb = near.half / 14 + 1;
    quad(canvas, far.x - far.half, top, far.x - far.half + farCurb, top,
         near.x - near.half + nearCurb, bottom, near.x - near.half, bottom, ST7305_COLOR_WHITE);
    quad(canvas, far.x + far.half - farCurb, top, far.x + far.half, top,
         near.x + near.half, bottom, near.x + near.half - nearCurb, bottom, ST7305_COLOR_WHITE);
    if ((index / 3U) % 2U == 0U) {
      for (int lane = -1; lane <= 1; lane += 2) {
        const int farX = far.x + lane * far.half / 3;
        const int nearX = near.x + lane * near.half / 3;
        quad(canvas, farX - 1, top, farX + 1, top, nearX + 2, bottom,
             nearX - 2, bottom, ST7305_COLOR_WHITE);
      }
    }
  }

  // Props and traffic are projected from the same segment geometry as the road.
  for (uint8_t n = kDrawDistance; n > 1; --n) {
    const Projected& point = points[n];
    if (point.scale == 0.0f || point.y > kScreenHeight + yOffset) continue;
    const uint16_t index = (base + n) % kSegmentCount;
    const Segment& segment = segments_[index];
    if (segment.scenery) {
      scenery(canvas, segment.scenery, point.x + segment.scenerySide * point.half * 3 / 2,
              point.y, bounded(point.half / 2, 3, 80));
    }
    for (const TrafficCar& traffic : traffic_) {
      if (segmentAt(traffic.position) != index) continue;
      const int carX = point.x + static_cast<int>(point.half * traffic.lane);
      const int width = bounded(point.half / 4, 5, 48);
      car(canvas, carX, point.y, width, width / 2 + 4, false);
    }
  }
}

void RacingGame::drawPlayer(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  const uint16_t index = segmentAt(playerTrackPosition());
  const uint16_t ahead = (index + 6U) % kSegmentCount;
  const float slope = (segments_[ahead].height - segments_[index].height) / (6.0f * kSegmentLength);
  const float pitch = 0.28f + fmaxf(-0.25f, fminf(0.25f, atanf(slope) * 0.2f));
  RacingCarMesh::draw(display, kScreenWidth / 2, yOffset + kScreenHeight - 31,
                      playerX_ * 0.5f, pitch);
  if (impactVisible_) {
    rectangle(canvas, 167, yOffset + 4, 216, yOffset + 22, ST7305_COLOR_WHITE);
    canvas.drawRectangle(167, yOffset + 4, 216, yOffset + 22, ST7305_COLOR_BLACK);
    auto& text = display.text();
    text.setFont(u8g2_font_7x14_tf);
    text.setForegroundColor(ST7305_COLOR_BLACK);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.setFontMode(1);
    text.drawUTF8(180, yOffset + 18, "HIT");
  }
}

void RacingGame::drawHud(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  rectangle(canvas, 0, yOffset, 99, yOffset + 33, ST7305_COLOR_BLACK);
  canvas.drawRectangle(0, yOffset, 99, yOffset + 33, ST7305_COLOR_WHITE);
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  char label[40];
  snprintf(label, sizeof(label), "LAP %u/3", static_cast<unsigned>(lap_));
  text.drawUTF8(5, yOffset + 14, label);
  const uint32_t centiseconds = static_cast<uint32_t>(lapSeconds_ * 100.0f);
  snprintf(label, sizeof(label), "TIME %lu.%02lu", static_cast<unsigned long>(centiseconds / 100U),
           static_cast<unsigned long>(centiseconds % 100U));
  text.drawUTF8(5, yOffset + 29, label);
  if (bestLapSeconds_ > 0.0f) {
    const uint32_t best = static_cast<uint32_t>(bestLapSeconds_ * 100.0f);
    snprintf(label, sizeof(label), "BEST %lu.%02lu", static_cast<unsigned long>(best / 100U),
             static_cast<unsigned long>(best % 100U));
    rectangle(canvas, 289, yOffset, 383, yOffset + 18, ST7305_COLOR_BLACK);
    text.drawUTF8(294, yOffset + 14, label);
  }

  const int cx = kScreenWidth - 43;
  const int cy = yOffset + kScreenHeight - 41;
  canvas.drawFilledCircle(cx, cy, 36, ST7305_COLOR_BLACK);
  canvas.drawCircle(cx, cy, 36, ST7305_COLOR_WHITE);
  const int kmh = bounded(static_cast<int>(speed_ * 300.0f / 13000.0f), 0, 300);
  const float angle = (-3.93f + (kmh / 300.0f) * 4.71f);
  for (int mark = 0; mark <= 6; ++mark) {
    const float markAngle = -3.93f + mark * 4.71f / 6.0f;
    const int x0 = cx + static_cast<int>(27.0f * cosf(markAngle));
    const int y0 = cy + static_cast<int>(27.0f * sinf(markAngle));
    const int x1 = cx + static_cast<int>(33.0f * cosf(markAngle));
    const int y1 = cy + static_cast<int>(33.0f * sinf(markAngle));
    canvas.drawLine(x0, y0, x1, y1, ST7305_COLOR_WHITE);
  }
  canvas.drawLine(cx, cy, cx + static_cast<int>(25.0f * cosf(angle)),
                  cy + static_cast<int>(25.0f * sinf(angle)), ST7305_COLOR_WHITE);
  canvas.drawFilledCircle(cx, cy, 2, ST7305_COLOR_WHITE);
  snprintf(label, sizeof(label), "%d", kmh);
  text.drawUTF8(cx - text.getUTF8Width(label) / 2, cy + 26, label);
}
