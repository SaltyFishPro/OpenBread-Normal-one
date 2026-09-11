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

ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }

U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }
