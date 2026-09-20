#include "DisplayMonoTft.h"

#include <SPI.h>
#include <driver/gpio.h>

#include "BoardConfig.h"

namespace {
const ST73xxPins kDisplayPins = {
    BoardConfig::kPinDc,
    BoardConfig::kPinCs,
    BoardConfig::kPinSclk,
    BoardConfig::kPinSdin,
    BoardConfig::kPinRst,
};

constexpr int kDisplaySleepPins[] = {
    BoardConfig::kPinSdin,
    BoardConfig::kPinSclk,
    BoardConfig::kPinCs,
    BoardConfig::kPinDc,
    BoardConfig::kPinRst,
};

void holdDisplayPins(bool hold) {
  for (int pin : kDisplaySleepPins) {
    if (hold) {
      gpio_hold_en(static_cast<gpio_num_t>(pin));
    } else {
      gpio_hold_dis(static_cast<gpio_num_t>(pin));
    }
  }
}
}  // namespace

DisplayMonoTft::DisplayMonoTft() : display_(kDisplayPins, SPI) {}

bool DisplayMonoTft::begin() {
  SPI.begin(BoardConfig::kPinSclk, -1, BoardConfig::kPinSdin, BoardConfig::kPinCs);
  display_.initialize();
  display_.High_Power_Mode();
  display_.display_on(true);
  display_.display_Inversion(false);
  display_.setRotation(BoardConfig::kDisplayRotation);

  text_.begin(display_);
  text_.setFontMode(1);
  text_.setForegroundColor(ST7305_COLOR_BLACK);
  text_.setBackgroundColor(ST7305_COLOR_WHITE);
  text_.setFont(chinese_font_all);
  return true;
}

void DisplayMonoTft::clear() { display_.clearDisplay(); }

void DisplayMonoTft::present() { display_.display(); }

void DisplayMonoTft::presentRegion(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  if (x1 > x2) {
    const int16_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    const int16_t t = y1;
    y1 = y2;
    y2 = t;
  }

  const int16_t maxX = static_cast<int16_t>(display_.getDisplayWidth() - 1);
  const int16_t maxY = static_cast<int16_t>(display_.getDisplayHeight() - 1);
  if (maxX < 0 || maxY < 0 || x2 < 0 || y2 < 0 || x1 > maxX || y1 > maxY) {
    return;
  }
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > maxX) x2 = maxX;
  if (y2 > maxY) y2 = maxY;

  int16_t px1 = 0;
  int16_t py1 = 0;
  int16_t px2 = 0;
  int16_t py2 = 0;
  display_.logicalToPhysical(x1, y1, px1, py1);
  display_.logicalToPhysical(x2, y2, px2, py2);
  display_.displayRegion(static_cast<uint16_t>(px1), static_cast<uint16_t>(py1),
                         static_cast<uint16_t>(px2), static_cast<uint16_t>(py2));
}

void DisplayMonoTft::prepareForSleepKeepDisplay() {
  display_.Low_Power_Mode();
  holdDisplayPins(true);
}

void DisplayMonoTft::prepareForSleep() {
  display_.display_on(false);
  display_.Low_Power_Mode();
}

void DisplayMonoTft::restoreAfterSleep() {
  holdDisplayPins(false);
  SPI.begin(BoardConfig::kPinSclk, -1, BoardConfig::kPinSdin, BoardConfig::kPinCs);
  display_.High_Power_Mode();
  display_.display_on(true);
  display_.display_Inversion(false);
}

int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }

int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }

size_t DisplayMonoTft::frameBufferBytes() const { return display_.frameBufferLength(); }

void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }

void DisplayMonoTft::restoreFrame(const uint8_t* src) { display_.copyFrameBufferFrom(src); }

ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }

U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }
