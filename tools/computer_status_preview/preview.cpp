#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/ui/IconBitmap.h"
#include "src/ui/pages/ComputerStatusPage.h"
#include "src/ui/assets/main_menu/computer_status.h"

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

namespace {
const IconBitmap::Anim kIcon = {
    reinterpret_cast<const uint8_t*>(&computer_status_frames[0][0]),
    COMPUTER_STATUS_FRAME_BYTES, COMPUTER_STATUS_FRAME_WIDTH, COMPUTER_STATUS_FRAME_HEIGHT,
    COMPUTER_STATUS_FRAME_DELAY, COMPUTER_STATUS_FRAME_COUNT};

void save(DisplayMonoTft& display, const char* name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/computer-status-render/") + name + ".pgm",
                    std::ios::binary);
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
  if (!out) throw std::runtime_error("Could not save framebuffer");
}

void shot(DisplayMonoTft& display, ComputerStatusPage& page, const char* name,
          uint32_t nowMs, bool connected = false, int16_t yOffset = 0) {
  display.clear();
  page.render(display, yOffset, kIcon, connected, nowMs);
  save(display, name);
}
}

int main() {
  try {
    DisplayMonoTft display;
    display.begin();
    ComputerStatusPage page;
    // Before entry completes, the incoming page uses a still first frame.
    shot(display, page, "01-transition", 0, false, 84);
    uint8_t firstTransition[8064];
    display.captureFrame(firstTransition);
    page.begin(1000);
    shot(display, page, "02-intro-start", 1000);
    shot(display, page, "03-intro-middle", 1500);
    shot(display, page, "04-waiting", 2000);
    if (page.needsAnimationFrame(60000, kIcon) || page.needsRedraw(60000, kIcon, false))
      throw std::runtime_error("Waiting page did not settle");
    if (!page.needsRedraw(60000, kIcon, true))
      throw std::runtime_error("Connection change was not detected");
    shot(display, page, "05-connected", 60000, true);
    if (page.needsRedraw(60000, kIcon, true))
      throw std::runtime_error("Connected page did not settle");
    page.prepareEnter();
    shot(display, page, "06-reenter-transition", 61000, false, 84);
    uint8_t nextTransition[8064];
    display.captureFrame(nextTransition);
    if (std::memcmp(firstTransition, nextTransition, sizeof(firstTransition)) != 0)
      throw std::runtime_error("Re-entry transition retained the previous icon or status");
    page.begin(61000);
    if (!page.needsRedraw(61000, kIcon, false))
      throw std::runtime_error("Re-entry did not reset the intro");
    std::puts("PASS: real rotated framebuffer, intro settles, connection redraw and re-entry");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
