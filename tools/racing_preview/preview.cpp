#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#define private public
#include "src/ui/pages/RacingGame.h"
#undef private

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
void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }
void DisplayMonoTft::restoreFrame(const uint8_t* src) { display_.copyFrameBufferFrom(src); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

void save(DisplayMonoTft& display, const char* name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/racing-render/") + name + ".pgm", std::ios::binary);
  out << "P5\n384 168\n255\n";
  for (int16_t y = 0; y < 168; ++y) {
    for (int16_t x = 0; x < 384; ++x) {
      int16_t px = 0;
      int16_t py = 0;
      display.canvas().logicalToPhysical(x, y, px, py);
      const size_t index = static_cast<size_t>(py / 2) * 42U + px / 4;
      const uint8_t mask = static_cast<uint8_t>(1U << (7 - ((px % 4) * 2 + py % 2)));
      out.put(static_cast<char>((raw[index] & mask) ? 0 : 255));
    }
  }
  if (!out) throw std::runtime_error("Framebuffer export failed");
}

void shot(DisplayMonoTft& display, RacingGame& game, const char* name, float position,
          uint8_t lightPhase) {
  game.position_ = position;
  game.lightPhase_ = lightPhase;
  game.speed_ = 10000.0f;
  game.lap_ = 2;
  game.lapSeconds_ = 83.42f;
  game.bestLapSeconds_ = 70.14f;
  display.clear();
  game.render(display, 0);
  save(display, name);
}

int main() {
  try {
    DisplayMonoTft display;
    display.begin();
    RacingGame game;
    game.enter();
    shot(display, game, "01-city", 24.0f * 200.0f, 1);
    shot(display, game, "02-tunnel", 77.0f * 200.0f, 2);
    std::puts("PASS: captured production racing renderer through rotated ST7305 framebuffer");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
