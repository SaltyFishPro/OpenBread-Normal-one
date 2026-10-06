#include <fstream>
#include <string>

#include <Arduino.h>
#define private public
#include "src/ui/pages/AnswersBookPage.h"
#undef private
#include "src/bsp/DisplayMonoTft.h"

DisplayMonoTft::DisplayMonoTft() : display_(SPI) {}

bool DisplayMonoTft::begin() {
  display_.setRotation(1);
  display_.clearDisplay();
  text_.begin(display_);
  text_.setFontMode(1);
  text_.setForegroundColor(ST7305_COLOR_BLACK);
  text_.setBackgroundColor(ST7305_COLOR_WHITE);
  text_.setFont(chinese_font_all);
  return true;
}

void DisplayMonoTft::clear() { display_.clearDisplay(); }
int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }
int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }
size_t DisplayMonoTft::frameBufferBytes() const { return display_.frameBufferLength(); }
void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

void save(DisplayMonoTft& display, const char* name) {
  // Capture the same ST7305 framebuffer that present() sends to the display.
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/answers-book-render/") + name + ".pgm",
                    std::ios::binary);
  out << "P5\n384 168\n255\n";
  for (int16_t y = 0; y < 168; ++y) {
    for (int16_t x = 0; x < 384; ++x) {
      int16_t px = 0, py = 0;
      display.canvas().logicalToPhysical(x, y, px, py);
      const size_t index = static_cast<size_t>(py / 2) * 42U +
                           static_cast<size_t>(px / 4);
      const uint8_t mask = static_cast<uint8_t>(1U << (7 - ((px % 4) * 2 + py % 2)));
      out.put(static_cast<char>((raw[index] & mask) ? 0 : 255));
    }
  }
}

void shot(DisplayMonoTft& display, AnswersBookPage& page, const char* name,
          uint32_t nowMs) {
  display.clear();
  page.renderDetail(AnswersBookPage::kHomeIndex, 0, 0, display, nowMs);
  save(display, name);
}

int main() {
  DisplayMonoTft display;
  display.begin();
  AnswersBookPage page;
  page.handleDetailEnter(AnswersBookPage::kHomeIndex, 0);
  shot(display, page, "01-prompt", 0);

  page.view_ = AnswersBookPage::View::Thinking;
  page.phaseStartMs_ = 1000;
  shot(display, page, "02-thinking", 3500);

  page.view_ = AnswersBookPage::View::RevealAnimation;
  shot(display, page, "03-reveal", 6000);

  page.view_ = AnswersBookPage::View::Answer;
  page.hasAnswer_ = true;
  page.answerIndex_ = 0;
  shot(display, page, "04-answer", 7000);

  page.view_ = AnswersBookPage::View::History;
  page.historyCount_ = 3;
  page.historyIndices_[0] = 0;
  page.historyIndices_[1] = 2;
  page.historyIndices_[2] = 4;
  page.historyCursor_ = 0;
  shot(display, page, "05-history-first", 7000);

  page.historyCursor_ = 2;
  shot(display, page, "06-history-third", 7000);
  return 0;
}
