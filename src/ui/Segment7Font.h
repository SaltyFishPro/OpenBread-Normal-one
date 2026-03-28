#pragma once

#include <Arduino.h>
#include <ST7305_2p9_BW_DisplayDriver.h>

namespace Segment7Font {

struct Style {
  int16_t digitWidth = 24;
  int16_t digitHeight = 40;
  int16_t thickness = 4;
  int16_t spacing = 3;
  int16_t colonWidth = 6;
  uint16_t onColor = ST7305_COLOR_WHITE;
};

int16_t measureText(const char* text, const Style& style);
void drawText(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y,
              const char* text, const Style& style);

}  // namespace Segment7Font

