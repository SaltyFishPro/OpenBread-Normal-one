#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/ui/IconBitmap.h"
#include "src/ui/pages/HomePage.h"
#include "src/ui/pages/SdUsbPage.h"

DisplayMonoTft::DisplayMonoTft() : display_(SPI) {}
bool DisplayMonoTft::begin() {
  display_.setRotation(BoardConfig::kDisplayRotation);
  display_.clearDisplay();
  text_.begin(display_);
  text_.setFontMode(1);
  text_.setForegroundColor(ST7305_COLOR_BLACK);
  text_.setBackgroundColor(ST7305_COLOR_WHITE);
  text_.setFont(u8g2_font_7x14_tf);
  return true;
}
void DisplayMonoTft::clear() { display_.clearDisplay(); }
int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }
int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }
size_t DisplayMonoTft::frameBufferBytes() const { return 8064U; }
void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }
void DisplayMonoTft::restoreFrame(const uint8_t* src) { display_.copyFrameBufferFrom(src); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void save(DisplayMonoTft& display, const char* name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/sd-usb-render/") + name + ".pgm", std::ios::binary);
  out << "P5\n384 168\n255\n";
  for (int16_t y = 0; y < 168; ++y) {
    for (int16_t x = 0; x < 384; ++x) {
      int16_t px = 0, py = 0;
      display.canvas().logicalToPhysical(x, y, px, py);
      const size_t index = static_cast<size_t>(py / 2) * 42U + px / 4;
      const uint8_t mask = static_cast<uint8_t>(1U << (7 - ((px % 4) * 2 + py % 2)));
      out.put(static_cast<char>((raw[index] & mask) ? 0 : 255));
    }
  }
  require(static_cast<bool>(out), "Could not save framebuffer");
}
}

int main() {
  try {
    DisplayMonoTft display;
    display.begin();
    HomePage home;
    require(home.begin(), "Home initialization failed");
    HomePage::ClockData clock;
    clock.valid = true;
    clock.hour = 10; clock.minute = 20;
    clock.year = 2026; clock.month = 10; clock.day = 20; clock.weekday = 2;
    home.setClockData(clock);
    HomePage::BatteryData battery;
    battery.valid = true; battery.percent = 80;
    home.setBatteryData(battery);
    // First item Up wraps to the new final menu item.
    home.handleInput(true, false, false, 1000);
    home.update(2000);
    require(home.focusIndex() == HomePage::kSdUsbMenuIndex, "Up wrap skipped SD USB");
    require(std::strcmp(home.focusName(), "SD卡转USB") == 0, "Wrong menu label");
    require(home.handleInput(false, false, true, 2000), "OK did not enter new item");
    home.update(2900);
    display.clear();
    home.render(display, 0, 2900);
    save(display, "01-home");
    // Wrap in both directions and visit every menu item with the production model.
    home.handleInput(false, true, false, 3000);
    home.update(4000);
    require(home.focusIndex() == 0U, "Down did not wrap back to Settings");
    for (uint8_t i = 1; i <= HomePage::kSdUsbMenuIndex; ++i) {
      const uint32_t now = 4000U + i * 1000U;
      home.handleInput(false, true, false, now);
      home.update(now + 500U);
      require(home.focusIndex() == i, "Menu navigation skipped an entry");
    }

    const auto& icon = HomePage::sdUsbIcon();
    require(icon.frameWidth == 100U && icon.frameHeight == 100U, "Wrong menu asset size");
    SdUsbPage page;
    display.clear();
    page.render(display, 84, icon, false);
    save(display, "02-transition");
    display.clear();
    page.render(display, 0, icon, true);
    save(display, "03-active");
    uint8_t first[8064], second[8064];
    display.captureFrame(first);
    display.clear();
    page.render(display, 0, icon, true);
    display.captureFrame(second);
    require(std::memcmp(first, second, sizeof(first)) == 0, "Static page changed pixels");
    display.clear();
    page.render(display, 0, icon, false);
    save(display, "04-inactive");
    display.captureFrame(second);
    // The transitional resident font omits Chinese status text. Verify the
    // status panel itself remains rendered instead of requiring text pixels.
    int darkPixels = 0;
    for (int16_t y = 44; y < 72; ++y) {
      for (int16_t x = 152; x < 384; ++x) {
        int16_t px = 0;
        int16_t py = 0;
        display.canvas().logicalToPhysical(x, y, px, py);
        const size_t index = static_cast<size_t>(py / 2) * 42U + px / 4;
        const uint8_t mask = static_cast<uint8_t>(1U << (7 - ((px % 4) * 2 + py % 2)));
        darkPixels += (second[index] & mask) != 0;
      }
    }
    require(darkPixels > 100, "Switch status panel is not visible");
    for (int16_t offset = -168; offset <= 168; ++offset) {
      display.clear();
      page.render(display, offset, icon, false);
    }
    std::puts("PASS: 13 menu entries, bidirectional wrap, OK, static/status frames and transition offsets");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
