#include <FS.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/services/SdBitmapFont.h"
#include "src/services/SdCardService.h"
#include "src/ui/SdBitmapText.h"
#include "src/ui/pages/SdFontTestPage.h"

DisplayMonoTft::DisplayMonoTft() : display_(SPI) {}
bool DisplayMonoTft::begin() {
  display_.setRotation(BoardConfig::kDisplayRotation);
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
void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

// Only replace physical mounting. Production file access, parser, text and page run unchanged.
SdCardService::Status SdCardService::refresh() {
  return TestFs::unavailable ? Status::NotInserted : Status::Ready;
}

namespace {
const char* kRoot = "tmp/sd-font-test/";
const char* kSamples[] = {"你好，面包！Normal", "0123456789 Aa", "Unicode \xF0\xA0\xAE\xB7 \xC3\xA9"};
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
uint16_t le16(const uint8_t* p) { return p[0] | uint16_t(p[1]) << 8U; }
uint32_t le32(const uint8_t* p) { return le16(p) | uint32_t(le16(p + 2)) << 16U; }
void root(const char* name) { TestFs::root = std::string(kRoot) + name; }
void ready(SdBitmapFont& font) {
  for (unsigned i = 0; i < 10000U && font.state() == SdBitmapFont::State::Checking; ++i)
    font.tick();
  check(font.state() == SdBitmapFont::State::Ready, "Font verification failed");
}
void save(DisplayMonoTft& display, const std::string& name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string(kRoot) + name + ".pgm", std::ios::binary);
  out << "P5\n" << display.width() << " " << display.height() << "\n255\n";
  for (int16_t y = 0; y < display.height(); ++y) {
    for (int16_t x = 0; x < display.width(); ++x) {
      int16_t px = 0, py = 0;
      display.canvas().logicalToPhysical(x, y, px, py);
      const size_t index = size_t(py / 2) * 42U + px / 4;
      const uint8_t mask = 1U << (7 - ((px % 4) * 2 + py % 2));
      out.put(static_cast<char>((raw[index] & mask) ? 0 : 255));
    }
  }
  check(bool(out), "Framebuffer save failed");
}

void compareGlyphs(SdBitmapFont& font, uint16_t size) {
  std::ifstream input(std::string(kRoot) + "expected-" + std::to_string(size) + ".dat", std::ios::binary);
  check(bool(input), "Reference glyph file missing");
  uint8_t header[18], row[128];
  size_t count = 0;
  while (input.read(reinterpret_cast<char*>(header), sizeof(header))) {
    SdBitmapFont::Glyph glyph;
    check(font.glyph(le32(header), glyph), "Reference glyph missing");
    check(glyph.width == le16(header + 4) && glyph.height == le16(header + 6) &&
          glyph.xOffset == int16_t(le16(header + 8)) && glyph.yOffset == int16_t(le16(header + 10)) &&
          glyph.advance == int16_t(le16(header + 12)) && glyph.length == le32(header + 14),
          "Glyph metrics differ from Python reference");
    std::vector<uint8_t> expected(glyph.length);
    input.read(reinterpret_cast<char*>(expected.data()), expected.size());
    const size_t stride = (glyph.width + 7U) / 8U;
    for (uint16_t y = 0; y < glyph.height; ++y) {
      check(font.readRow(glyph, y, row, sizeof(row)), "Glyph row read failed");
      check(std::memcmp(row, expected.data() + y * stride, stride) == 0, "Glyph pixels differ from reference");
    }
    ++count;
  }
  check(count > 25U, "Insufficient glyph reference coverage");
}

void decodeTests() {
  const char* valid = "A\xE4\xB8\xAD\xF0\xA0\x80\x80\xF4\x8F\xBF\xBF";
  uint32_t cp;
  for (uint32_t expected : {65U, 0x4E2DU, 0x20000U, 0x10FFFFU}) {
    check(SdBitmapText::nextCodepoint(valid, cp) && cp == expected, "UTF-8 scalar decoding failed");
  }
  check(!SdBitmapText::nextCodepoint(valid, cp), "UTF-8 NUL not respected");
  for (const char* invalid : {"\xC0\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF0", "\xE4\xB8", "\xFF"}) {
    unsigned count = 0;
    while (SdBitmapText::nextCodepoint(invalid, cp)) {
      check(cp == 0xFFFDU && ++count <= 4U, "Invalid UTF-8 was accepted or failed to advance");
    }
  }
}
}

int main() {
  try {
    decodeTests();
    DisplayMonoTft display;
    display.begin();
    SdBitmapFont font;
    SdFontTestPage page;
    SdCardService sd;
    root("valid");
    page.begin(font, sd);
    check(font.state() == SdBitmapFont::State::Checking && TestFs::handles == 1U, "Page did not open font");
    display.clear(); page.render(display, 0, font); save(display, "checking");
    const size_t beforeTick = TestFs::bytes;
    font.tick();
    check(TestFs::bytes - beforeTick <= 8192U && font.state() == SdBitmapFont::State::Checking,
          "Verification blocks on the whole file");
    font.end();
    const size_t afterEnd = TestFs::reads;
    font.tick();
    check(TestFs::handles == 0U && TestFs::reads == afterEnd, "Cancelled verification still reads SD");
    page.begin(font, sd);
    for (uint16_t size : {14, 20, 24}) {
      ready(font);
      check(font.metrics().pixelSize == size && TestFs::handles == 1U, "Size switch or file ownership failed");
      compareGlyphs(font, size);
      display.clear(); page.render(display, 0, font); save(display, "page-" + std::to_string(size));
      for (uint8_t rotation = 0; rotation < 4U; ++rotation) {
        display.setRotation(rotation);
        display.clear();
        const SdBitmapText::Clip clip = {16, 56, int16_t(display.width() - 17), 140};
        for (uint8_t i = 0; i < 3U; ++i)
          check(SdBitmapText::drawUTF8(display.canvas(), font, 16, 84 + i * 26,
                                       kSamples[i], ST7305_COLOR_BLACK, clip), "Text rendering failed");
        save(display, "render-" + std::to_string(size) + "-r" + std::to_string(rotation));
      }
      display.setRotation(1);
      display.canvas().fill(0xFF);
      check(SdBitmapText::drawUTF8(display.canvas(), font, -8, 13, "A\xC3\xA9你好",
                                   ST7305_COLOR_WHITE, {3, 2, 100, 29}), "Clipped/inverted rendering failed");
      save(display, "crop-" + std::to_string(size));
      display.clear();
      check(SdBitmapText::drawUTF8(display.canvas(), font, 4, 60, "\xF4\x8F\xBF\xBF",
                                   ST7305_COLOR_BLACK, {0, 0, 383, 167}), "Missing glyph rendering failed");
      save(display, "missing-" + std::to_string(size));
      const char* measure = "你好 A\n012";
      std::ifstream widthFile(std::string(kRoot) + "width-" + std::to_string(size) + ".txt");
      int32_t expectedWidth = 0;
      widthFile >> expectedWidth;
      check(expectedWidth > 0 && SdBitmapText::measureUTF8(font, measure) == expectedWidth,
            "Width measurement differs from Python reference");
      SdBitmapFont::Glyph a;
      check(font.glyph('A', a), "A not found");
      uint8_t row[128];
      const size_t cachedReads = TestFs::reads;
      for (unsigned i = 0; i < 10; ++i) {
        check(font.glyph('A', a), "Cached glyph missing");
        if (a.height) check(font.readRow(a, 0, row, sizeof(row)), "Cached row missing");
      }
      check(TestFs::reads == cachedReads, "Repeated glyph still accesses SD");
      check(!font.glyph(0x10FFFFU, a), "Missing glyph unexpectedly found");
      const size_t missingReads = TestFs::reads;
      check(!font.glyph(0x10FFFFU, a) && TestFs::reads == missingReads, "Missing glyph not cached");
      check(page.handleInput(false, true, false, font, sd), "Down size change failed");
    }
    ready(font);
    check(font.metrics().pixelSize == 14U, "Size selector did not wrap");
    check(page.handleInput(true, false, false, font, sd), "Up size change failed");
    ready(font);
    check(font.metrics().pixelSize == 24U, "Up did not wrap to 24");
    SdBitmapFont::Glyph stale;
    check(font.glyph('A', stale), "Stale-session setup failed");
    page.handleInput(false, false, true, font, sd);
    ready(font);
    uint8_t row[128];
    check(!font.readRow(stale, 0, row, sizeof(row)), "Old file/session offset was reused");

    for (const auto& bad : std::vector<std::pair<std::string, SdBitmapFont::Error>>{
        {"missing", SdBitmapFont::Error::OpenFailed}, {"header", SdBitmapFont::Error::Header},
        {"truncated", SdBitmapFont::Error::Header}, {"wrong-size", SdBitmapFont::Error::SizeMismatch},
        {"crc", SdBitmapFont::Error::Crc}}) {
      root(bad.first.c_str());
      font.open(14);
      while (font.state() == SdBitmapFont::State::Checking) font.tick();
      check(font.state() == SdBitmapFont::State::Error && font.error() == bad.second && TestFs::handles == 0U,
            "Damaged file accepted or handle leaked");
      display.clear(); page.render(display, 0, font); save(display, "error-" + bad.first);
    }
    for (const char* bad : {"bad-glyph", "padding"}) {
      root(bad); font.open(14); ready(font);
      SdBitmapFont::Glyph glyph;
      check(!font.glyph('A', glyph) && font.error() == SdBitmapFont::Error::Glyph && TestFs::handles == 0U,
            "Corrupt glyph bounds/padding accepted");
    }
    root("large"); font.open(14); ready(font);
    SdBitmapFont::Glyph large;
    check(font.glyph('A', large) && large.length > 128U && large.xOffset < 0, "Large glyph not accepted");
    for (uint16_t y = 0; y < large.height; ++y) {
      check(font.readRow(large, y, row, sizeof(row)) && row[0] == 255 && row[16] == 128,
            "Streaming large glyph failed");
    }
    root("big-index"); font.open(14); ready(font);
    const size_t reads = TestFs::reads;
    SdBitmapFont::Glyph extended;
    check(font.glyph(0x20000U, extended) && extended.advance == 8 && extended.length == 0U,
          "65537-entry index or supplementary scalar failed");
    check(TestFs::reads - reads <= 18U, "Large lookup scanned the file");
    root("valid"); font.open(14);
    TestFs::unavailable = true;
    font.tick();
    check(font.error() == SdBitmapFont::Error::Read && TestFs::handles == 0U, "Removed card not handled");
    page.begin(font, sd);
    display.clear(); page.render(display, 0, font); save(display, "no-card");
    TestFs::unavailable = false;
    page.handleInput(false, false, true, font, sd); ready(font);
    font.end();
    check(TestFs::handles == 0U && font.state() == SdBitmapFont::State::Closed, "Exit leaked a font handle");
    std::printf("PASS: real 14/20/24 BIN glyphs, CRC/cancel, UTF-8, 65537 index, streaming/cache, errors/retry, sessions; service=%zu bytes\n",
                sizeof(SdBitmapFont));
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
