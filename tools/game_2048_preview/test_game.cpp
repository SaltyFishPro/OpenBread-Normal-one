#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

#define OB_PREVIEW_CUSTOM_CLOCK
#include <Arduino.h>

#define private public
#include "src/ui/pages/Game2048.h"
#undef private
#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"

uint32_t millis() { return 0; }

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
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

static void saveFrame(DisplayMonoTft& display) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out("tmp/game-2048-render/frame.pgm", std::ios::binary);
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
  if (!out) throw std::runtime_error("Framebuffer export failed");
}

static void clear(Game2048& game) {
  std::memset(game.grid_, 0, sizeof(game.grid_));
  game.score_ = 0;
  game.state_ = Game2048::State::Running;
  game.animationActive_ = false;
  game.edgesReady_ = true;
}

int main() {
  Game2048 game;
  clear(game);
  game.grid_[0][0] = game.grid_[0][1] = game.grid_[0][2] = game.grid_[0][3] = 2;
  const bool merged = game.move(0);
  if (!merged || game.grid_[0][0] != 4 || game.grid_[0][1] != 4 ||
      game.grid_[0][2] != 0 || game.score_ != 8 || game.movingCount_ != 4) {
    std::fprintf(stderr, "merge result=%d row=%u,%u,%u score=%lu moves=%u\n", merged,
                 game.grid_[0][0], game.grid_[0][1], game.grid_[0][2],
                 static_cast<unsigned long>(game.score_), game.movingCount_);
    return 1;
  }
  if (game.move(2) || game.score_ != 8) return 2;

  clear(game);
  game.grid_[1][2] = 8;
  game.tick(100, true, false, false, false);
  game.tick(800, true, false, false, false);
  game.tick(810, false, false, false, false);
  if (game.grid_[1][2] != 8 || game.animationActive_) return 3;
  game.handleLeft(950);
  if (game.grid_[1][0] != 8 || !game.animationActive_) return 4;

  game.state_ = Game2048::State::GameOver;
  game.tick(1200, false, false, false, false);
  if (game.animationActive_) return 5;

  clear(game);
  game.grid_[0][1] = 2;
  game.grid_[1][3] = 8;
  game.grid_[2][0] = 128;
  game.grid_[2][2] = 2048;
  game.grid_[3][1] = 32768;
  game.score_ = 18320;
  game.best_ = 42140;
  DisplayMonoTft display;
  if (!display.begin()) return 6;
  game.render(display, 0);
  saveFrame(display);
  std::puts("PASS: merge, score, invalid move, routed Left and final animation");
  std::puts("PASS: exported 384x168 production ST7305 framebuffer");
}
