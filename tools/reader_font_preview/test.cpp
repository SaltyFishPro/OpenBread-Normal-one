#include <FS.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/services/SdBitmapFont.h"
#include "src/services/SdCardService.h"
#include "src/ui/pages/ReaderPage.h"

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

SdCardService::Status SdCardService::refresh() {
  if (TestFs::handles) throw std::runtime_error("SD refreshed with open font handles");
  ++TestFs::refreshes;
  return TestFs::unavailable ? Status::NotInserted : Status::Ready;
}

namespace {
const std::string kRoot = "tmp/reader-sd-font/";
void check(bool condition, const char* why) {
  if (!condition) throw std::runtime_error(why);
}
struct Fixture {
  ReaderPage page;
  ReaderService reader;
  SdCardService sd;
  SdBitmapFont font;
  DisplayMonoTft display;
  Fixture(const char* card) {
    TestFs::root = kRoot + card;
    TestFs::failedPath.clear();
    TestFs::unavailable = false;
    TestFs::readLatencyMs = 0;
    reader.begin();
    display.begin();
    page.handleDetailEnter(2, 0, font);
  }
  ~Fixture() { font.end(); reader.reset(); }
  bool key(char key) {
    return page.handleDetailInput(2, 0, key == 'L', key == 'R', key == 'U', key == 'D',
                                  key == 'O', reader, sd, font);
  }
  bool back() { return page.handleDetailBack(2, 0, reader, font); }
  void settle() {
    for (int i = 0; i < 10000; ++i) {
      font.tick();
      page.updateMeaning(reader, font);
      if (font.state() != SdBitmapFont::State::Checking && !page.isPreparingMeaning()) return;
    }
    throw std::runtime_error("Checking/layout did not finish");
  }
  void draw(const std::string& name) {
    display.clear();
    check(page.renderDetail(2, 0, 0, display, reader, font), "Reader not rendered");
    uint8_t raw[8064];
    display.captureFrame(raw);
    std::ofstream out(kRoot + name + ".pgm", std::ios::binary);
    out << "P5\n384 168\n255\n";
    for (int16_t y = 0; y < 168; ++y) {
      for (int16_t x = 0; x < 384; ++x) {
        int16_t px, py;
        display.canvas().logicalToPhysical(x, y, px, py);
        const size_t index = (py / 2) * 42 + px / 4;
        const uint8_t mask = 1U << (7 - ((px % 4) * 2 + py % 2));
        out.put(static_cast<char>((raw[index] & mask) ? 0 : 255));
      }
    }
    check(bool(out), "Framebuffer write failed");
  }
};
void released() {
  check(TestFs::handles == 0 && TestPsram::used == 0, "Handles/PSRAM leaked after exit");
}
}

int main(int argc, char** argv) {
  try {
    TestPsram::available = !(argc > 1 && std::string(argv[1]) == "--no-psram");
    for (const char* card : {"valid", "plain"}) {
      Fixture f(card);
      f.draw(std::string(card) + "-library");
      check(f.font.state() == SdBitmapFont::State::Closed, "Library opened font eagerly");
      check(f.key('O') && f.reader.state() == ReaderService::State::Ready, "Book did not load");
      f.draw(std::string(card) + "-checking");
      check(f.font.state() == SdBitmapFont::State::Checking, "Expected incremental CRC");
      check(!f.key('D'), "Paging while checking");
      f.settle();
      check(f.font.state() == SdBitmapFont::State::Ready, "Font not ready");
      check(f.font.cacheBytes() == (TestPsram::available ? 125792U : 0U), "Unbounded cache");
      for (unsigned word = 0; word < f.reader.recordCount(); ++word) {
        std::ofstream meaning(kRoot + "meaning-" + std::to_string(word) + ".bin", std::ios::binary);
        meaning << f.reader.entry().meaning;
        meaning.close();
        const std::string prefix = std::string(card) + "-word-" + std::to_string(word);
        unsigned page = 0;
        do {
          f.settle();
          f.draw(prefix + "-page-" + std::to_string(page++));
          const size_t reads = TestFs::reads;
          for (int i = 0; i < 100; ++i)
            check(!f.page.updateMeaning(f.reader, f.font), "Idle layout retriggered");
          check(reads == TestFs::reads, "Idle layout read SD");
          check(page < 100, "Paging never stops");
        } while (f.key('D'));
        std::ofstream(kRoot + prefix + "-pages.txt") << page;
        for (unsigned i = 1; i < page; ++i) { check(f.key('U'), "Up failed"); f.settle(); }
        check(!f.key('U'), "Up before first page");
        f.draw(prefix + "-back-to-first");
        if (word + 1 < f.reader.recordCount()) check(f.key('R'), "Next word failed");
      }
      check(!f.key('R'), "Right beyond last word");
      for (unsigned i = 1; i < f.reader.recordCount(); ++i) {
        check(f.key('L'), "Previous word failed");
        f.settle();
      }
      check(!f.key('L'), "Left at first word must reach existing back route");
      check(f.back(), "Word back must stay in library");
      released();
      check(!f.back(), "Library back must reach section");
      released();
    }
    // Font errors retain word title/controls. OK can recover without leaving the book.
    for (const char* card : {"missing", "crc", "wrong-size", "tall"}) {
      Fixture f(card);
      f.key('O'); f.settle(); f.draw(std::string("error-") + card);
      if (std::string(card) != "tall")
        check(f.font.state() == SdBitmapFont::State::Error, "Invalid font accepted");
      check(!f.key('D'), "Error page can scroll");
      TestFs::root = kRoot + "valid";
      check(f.key('O'), "Retry not consumed");
      f.settle();
      f.draw(std::string("recovered-") + card);
      check(f.font.state() == SdBitmapFont::State::Ready, "Retry failed");
      check(f.back(), "Error/retry back failed");
      released();
    }
    {
      Fixture f("valid");
      f.key('O');
      TestFs::readLatencyMs = 1;
      for (int i = 0; i < 3; ++i) {
        const size_t bytes = TestFs::bytes;
        const uint32_t started = millis();
        f.font.tick();
        check(TestFs::bytes - bytes <= 32768U && millis() - started <= 4U, "CRC step unbounded");
      }
      check(f.back(), "Cancel CRC failed");
      released();
      TestFs::readLatencyMs = 0;
      f.key('O');
      while (f.font.state() == SdBitmapFont::State::Checking) f.font.tick();
      // The long word needs more than one 16-codepoint layout step.
      f.key('R');
      check(!f.page.updateMeaning(f.reader, f.font) && f.page.isPreparingMeaning(), "Layout blocks on full entry");
      f.draw("layout-pending");
      check(f.key('R'), "Switch word during layout failed");
      f.settle(); f.draw("layout-switch");
      check(f.key('L'), "Return to long entry failed");
      f.page.updateMeaning(f.reader, f.font);
      check(f.page.isPreparingMeaning(), "Expected unfinished layout before cancelling");
      check(f.back(), "Cancel layout failed");
      released();
      const size_t reads = TestFs::reads;
      for (int i = 0; i < 100; ++i) { f.font.tick(); f.page.updateMeaning(f.reader, f.font); }
      check(reads == TestFs::reads, "SD used after back");
    }
    {
      Fixture f("valid");
      TestFs::unavailable = true;
      f.key('O');
      check(f.reader.state() == ReaderService::State::SdMissing &&
            f.font.state() == SdBitmapFont::State::Closed, "No-card path failed");
      f.draw("no-card");
      f.back(); released();
      TestFs::unavailable = false;
      f.key('O'); f.settle();
      f.key('R'); f.settle(); f.key('D'); f.settle();
      // Reset glyph cache without changing layout; simulate card failure during render.
      f.font.open(14);
      while (f.font.state() == SdBitmapFont::State::Checking) f.font.tick();
      TestFs::failedPath = "font-14.bin";
      f.draw("read-error");
      check(f.font.state() == SdBitmapFont::State::Error, "Drawing failed to report I/O error");
      f.draw("read-error-clean");
      TestFs::failedPath.clear();
      f.key('O'); f.settle(); f.draw("read-error-recovered");
      check(f.font.state() == SdBitmapFont::State::Ready, "I/O retry failed");
      f.back(); released();
    }
    check(TestPsram::allocations == TestPsram::frees, "Cache allocation/free imbalance");
    std::printf("PASS: Reader navigation, incremental CRC/layout cancellation, idle, errors/retry, release; PSRAM=%d peak=%zu\n",
                TestPsram::available, TestPsram::peak);
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
  }
}
