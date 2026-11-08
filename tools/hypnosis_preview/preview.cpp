#include <cstdio>
#include <fstream>
#include <string>

#define OB_PREVIEW_CUSTOM_CLOCK
#include <Arduino.h>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/ui/pages/HypnosisApp.h"

uint32_t millis() { return 0; }

DisplayMonoTft::DisplayMonoTft() : display_(SPI) {}
bool DisplayMonoTft::begin() {
  display_.setRotation(BoardConfig::kDisplayRotation);
  display_.clearDisplay();
  return true;
}
void DisplayMonoTft::clear() { display_.clearDisplay(); }
int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }
int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }
void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }

static void save(DisplayMonoTft& display, const char* name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/hypnosis-render/") + name + ".pgm", std::ios::binary);
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
}

int main() {
  DisplayMonoTft display;
  if (!display.begin()) return 1;
  HypnosisApp app;
  app.enter(1000);
  if (!app.needsAnimationFrame() || !app.consumeChanged()) return 2;
  app.tick(1100);
  if (!app.consumeChanged()) return 3;
  for (uint32_t elapsed : {0U, 1000U, 3000U, 8000U, 16000U}) {
    display.clear();
    app.tick(1000 + elapsed);
    app.render(display, 0, 1000 + elapsed);
    char name[20];
    snprintf(name, sizeof(name), "%04lu", static_cast<unsigned long>(elapsed));
    save(display, name);
  }
  if (!app.needsAnimationFrame()) return 4;
  app.replay(9000);
  if (!app.needsAnimationFrame()) return 5;
  app.exit();
  if (app.needsAnimationFrame()) return 6;
  std::puts("PASS: 384x168 ST7305 frames, continuous animation, replay and exit");
}
