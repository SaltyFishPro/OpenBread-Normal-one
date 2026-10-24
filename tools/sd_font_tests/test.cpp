#include <FS.h>
#include <esp_heap_caps.h>
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
  text_.setFont(u8g2_font_7x14_tf);
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
  ++TestFs::refreshes;
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
void root(const char* name, SdBitmapFont& font) {
  font.end();  // Changing the test card must invalidate the previous SD session.
  TestFs::root = std::string(kRoot) + name;
}
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

void compareGlyphs(SdBitmapFont& font, uint16_t size, const char* prefix = "expected-") {
  std::ifstream input(std::string(kRoot) + prefix + std::to_string(size) + ".dat", std::ios::binary);
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

void waitForCommonLoading(SdBitmapFont& font) {
  for (unsigned i = 0; i < 10000U && TestFs::handles < 2U &&
       font.state() == SdBitmapFont::State::Checking; ++i) font.tick();
  check(TestFs::handles == 2U && font.state() == SdBitmapFont::State::Checking &&
        font.commonGlyphCount() == 0U, "Common loading stage not entered");
}

void commonTests(SdBitmapFont& font, DisplayMonoTft& display, bool usePsram) {
  uint8_t row[128];
  SdBitmapFont::Glyph a;
  root("common-valid", font);
  for (uint16_t size : {14, 20, 24}) {
    check(font.open(size), "Common fixture font did not open");
    ready(font);
    check((font.commonGlyphCount() > 0U) == usePsram && TestFs::handles == 1U,
          "Common index not loaded or its handle not closed");
    const auto before = font.cacheStats();
    const size_t reads = TestFs::reads;
    check(font.glyph('A', a), "Common A missing");
    check(TestFs::reads - reads <= 2U || !usePsram, "Common glyph still searched the full index");
    if (usePsram)
      check(font.cacheStats().commonHits == before.commonHits + 1U, "Common lookup was not used");
    const size_t afterA = TestFs::reads;
    check(font.glyph('A', a), "Cached common A missing");
    check(TestFs::reads == afterA, "Common lookup ran before the glyph cache");
    if (a.height) check(font.readRow(a, 0, row, sizeof(row)), "Common glyph row failed");
    check(font.glyph('Z', a), "Uncommon glyph did not use full-font fallback");
    if (usePsram)
      check(font.cacheStats().commonMisses > 0U, "Uncommon lookup not recorded");
    compareGlyphs(font, size);
    for (uint8_t rotation = 0; rotation < 4U; ++rotation) {
      display.setRotation(rotation); display.clear();
      for (uint8_t i = 0; i < 3U; ++i)
        check(SdBitmapText::drawUTF8(display.canvas(), font, 16, 84 + i * 26,
                                     kSamples[i], ST7305_COLOR_BLACK,
                                     {16, 56, int16_t(display.width() - 17), 140}), "Common text render failed");
      save(display, "common-render-" + std::to_string(size) + "-r" + std::to_string(rotation));
    }
    display.setRotation(1);
    display.canvas().fill(0xFF);
    check(SdBitmapText::drawUTF8(display.canvas(), font, -8, 13, "A\xC3\xA9你好", ST7305_COLOR_WHITE,
                                 {3, 2, 100, 29}), "Common inverted text failed");
    save(display, "common-crop-" + std::to_string(size));
    display.clear();
    check(SdBitmapText::drawUTF8(display.canvas(), font, 4, 60, "\xF4\x8F\xBF\xBF", ST7305_COLOR_BLACK,
                                 {0, 0, 383, 167}), "Common missing text failed");
    save(display, "common-missing-" + std::to_string(size));
    const size_t allocations = TestPsram::allocations;
    const size_t bytes = TestFs::bytes;
    check(font.open(size), "Common same-size reopen failed");
    ready(font);
    check(TestPsram::allocations == allocations &&
          TestFs::bytes - bytes == 64U + (usePsram ? 96U + font.commonGlyphCount() * 8U : 0U),
          "Common reuse repeated NBFT CRC or did not reload sidecar");
  }
  // Exercise the user's exact currently exported index and all its glyph bytes.
  for (uint16_t size : {14, 20, 24}) {
    std::ifstream reference(std::string(kRoot) + "expected-common-" + std::to_string(size) + ".dat");
    if (!reference) continue;
    root("common-exported", font); font.open(size); ready(font);
    compareGlyphs(font, size, "expected-common-");
    if (usePsram) check(font.cacheStats().commonHits > 0U, "Exported sidecar unused");
    std::printf("PASS: exported %u px common index count=%lu; all glyph metrics/rows match Python\n",
                size, static_cast<unsigned long>(font.commonGlyphCount()));
  }
  for (const char* name : {"magic", "version", "header-size", "record-size", "size", "zero",
                          "oversize", "length", "reserved", "binding", "crc", "offset",
                          "before-index", "after-index", "duplicate", "scalar", "unordered-offset",
                          "block-boundary", "truncated", "short", "limit"}) {
    const std::string directory = std::string("common-bad-") + name;
    root(directory.c_str(), font); font.open(14); ready(font);
    check(font.commonGlyphCount() == 0U && TestFs::handles == 1U && font.glyph('A', a),
          "Invalid optional sidecar prevented full-font fallback");
  }
  root("common-bad-target", font); font.open(14); ready(font);
  if (usePsram) check(font.commonGlyphCount() == 1U, "Wrong-target fixture did not load");
  const auto count = font.commonGlyphCount();
  check(font.glyph('A', a) && a.codepoint == 'A' && font.commonGlyphCount() == 0U,
        "Wrong target returned another glyph instead of disabling/falling back");
  if (usePsram) check(count == 1U && font.cacheStats().commonHits == 0U, "Wrong target counted as a hit");
  if (usePsram) {
    root("common-aligned", font); font.open(14);
    const size_t alignedBytes = TestFs::bytes;
    font.tick();
    check(TestFs::bytes - alignedBytes == 32768U && TestFs::handles == 1U &&
          font.state() == SdBitmapFont::State::Checking, "NBFT completion exceeded tick budget");
    const size_t headerBytes = TestFs::bytes;
    font.tick();
    check(TestFs::bytes - headerBytes == 96U && TestFs::handles == 2U,
          "Common header did not use its own tick");
    ready(font);
    check(font.commonGlyphCount() == 2U, "Aligned fixture did not load common index");
    std::ifstream entries(std::string(kRoot) + "common-valid/Font/font-14.common.bin", std::ios::binary);
    uint8_t header[96];
    entries.read(reinterpret_cast<char*>(header), sizeof(header));
    const uint32_t count = le32(header + 12);
    std::vector<uint32_t> points;
    for (uint32_t i = 0; i < 256U; ++i) {
      uint8_t entry[8];
      entries.seekg(96U + (i * count / 256U) * 8U);
      entries.read(reinterpret_cast<char*>(entry), sizeof(entry));
      check(bool(entries), "Common benchmark entry missing");
      points.push_back(le32(entry));
    }
    size_t fullReads = 0, commonReads = 0;
    for (const char* directory : {"valid", "common-valid"}) {
      root(directory, font); font.open(14); ready(font);
      const size_t before = TestFs::reads;
      for (uint32_t cp : points) {
        check(font.glyph(cp, a), "Common benchmark glyph missing");
        if (a.height) check(font.readRow(a, 0, row, sizeof(row)), "Common benchmark row missing");
      }
      const size_t reads = TestFs::reads - before;
      if (font.commonGlyphCount()) {
        commonReads = reads;
        check(font.cacheStats().commonHits == points.size(), "Benchmark missed common index");
      } else fullReads = reads;
    }
    check(commonReads < fullReads && commonReads <= 512U, "Common index did not reduce cold lookup reads");
    std::printf("PASS: 256 distributed common glyphs (cold) SD reads full=%zu common=%zu; CRC/load excluded\n",
                fullReads, commonReads);
    root("common-valid", font); font.open(14); waitForCommonLoading(font);
    const size_t bytes = TestFs::bytes;
    font.tick();
    check(TestFs::bytes - bytes <= 32768U && TestFs::bytes - bytes > 0U &&
          font.commonGlyphCount() == 0U, "Common loader exceeded byte budget or enabled a partial index");
    TestClock::nowMs = UINT32_MAX - 1U;
    TestFs::readLatencyMs = 2;
    const size_t reads = TestFs::reads;
    font.tick();
    check(TestFs::reads - reads == 2U && TestClock::nowMs == 2U, "Common loader time/wrap budget failed");
    TestFs::readLatencyMs = 10;
    const size_t slowReads = TestFs::reads;
    font.tick();
    check(TestFs::reads - slowReads == 1U, "Common loader continued after slow read");
    TestFs::readLatencyMs = 0;
    font.end();
    const size_t cancelled = TestFs::reads;
    font.tick();
    check(TestFs::reads == cancelled && TestFs::handles == 0U && TestPsram::used == 0U,
          "Cancelling common load leaked resources or kept reading");
    // Optional file I/O loss degrades gracefully; font file I/O remains fatal.
    root("common-valid", font); font.open(14); waitForCommonLoading(font);
    TestFs::failedPath = ".common.bin";
    font.tick();
    check(font.state() == SdBitmapFont::State::Ready && font.commonGlyphCount() == 0U &&
          font.glyph('A', a) && TestFs::handles == 1U, "Optional payload read failure did not fall back");
    TestFs::failedPath.clear();
    font.open(14); ready(font);
    TestFs::failedPath = "font-14.bin";
    check(!font.glyph('A', a) && font.error() == SdBitmapFont::Error::Read &&
          font.commonGlyphCount() == 0U && TestFs::handles == 0U && TestPsram::used == 0U,
          "Common target I/O failure retained resources");
    TestFs::failedPath.clear();
    // Failure while opening/reading the optional header also keeps NBFT usable.
    font.end();
    TestFs::failedPath = ".common.bin";
    font.open(14); ready(font);
    check(font.commonGlyphCount() == 0U && TestFs::handles == 1U && font.glyph('A', a),
          "Optional header read failure did not fall back");
    TestFs::failedPath.clear();
    SdFontTestPage retryPage;
    SdCardService sd;
    retryPage.begin(font, sd); waitForCommonLoading(font);
    const size_t refreshes = TestFs::refreshes;
    retryPage.handleInput(false, false, true, font, sd);
    check(font.state() == SdBitmapFont::State::Checking && font.commonGlyphCount() == 0U &&
          TestFs::refreshes == refreshes + 1U && TestFs::handles == 1U,
          "OK during common load did not close old files and restart verification");
    ready(font);
    check(font.commonGlyphCount() > 0U, "Retry did not reload common index");
    root("common-valid", font); font.open(14); waitForCommonLoading(font);
    const size_t allocations = TestPsram::allocations;
    font.open(20); ready(font);
    check(font.metrics().pixelSize == 20U && font.commonGlyphCount() > 0U &&
          TestPsram::allocations == allocations && TestFs::handles == 1U,
          "Switching size during common load leaked/reallocated/reused wrong data");
    // Missing optional sidecar after a previously loaded one cannot retain stale entries.
    TestFs::root = std::string(kRoot) + "valid";
    font.open(14); ready(font);
    check(font.commonGlyphCount() == 0U && font.glyph('A', a), "Old common index survived missing sidecar");
    font.end();
    TestPsram::failAllocation = true;
    root("common-valid", font); font.open(14); ready(font);
    check(font.cacheBytes() == 0U && font.commonGlyphCount() == 0U && font.glyph('A', a),
          "Common fixture did not fall back when allocation failed");
    TestPsram::failAllocation = false;
  }
  font.end();
  check(TestFs::handles == 0U && TestPsram::used == 0U, "Common tests leaked resources");
  std::printf("PASS: common index load/fallback, wrong-target check, reuse/size switch/cancel, byte/time budget; mode=%s\n",
              usePsram ? "PSRAM" : "internal");
}
}

int main(int argc, char** argv) {
  try {
    const bool usePsram = argc < 2 || std::string(argv[1]) != "--no-psram";
    TestPsram::available = usePsram;
    decodeTests();
    DisplayMonoTft display;
    display.begin();
    SdBitmapFont font;
    SdFontTestPage page;
    SdCardService sd;
    check(TestPsram::used == 0U, "Idle font allocated PSRAM");
    root("valid", font);
    page.begin(font, sd);
    check((font.cacheBytes() > 0U) == usePsram && TestPsram::used == font.cacheBytes() &&
          font.cacheBytes() <= SdFontCacheBuffer::kBudgetBytes, "Cache memory budget/allocation failed");
    check(font.state() == SdBitmapFont::State::Checking && TestFs::handles == 1U, "Page did not open font");
    display.clear(); page.render(display, 0, font); save(display, "checking");
    const size_t beforeTick = TestFs::bytes;
    font.tick();
    check(TestFs::bytes - beforeTick == 32768U && font.state() == SdBitmapFont::State::Checking,
          "Verification blocks on the whole file");
    TestFs::readLatencyMs = 2;
    TestClock::nowMs = UINT32_MAX - 1U;
    const size_t budgetReads = TestFs::reads;
    font.tick();
    check(TestFs::reads - budgetReads == 2U && TestClock::nowMs == 2U,
          "4ms time budget or timer wrap failed");
    TestFs::readLatencyMs = 10;
    const size_t slowReads = TestFs::reads;
    font.tick();
    check(TestFs::reads - slowReads == 1U, "Slow SD read exceeded budget and started another read");
    TestFs::readLatencyMs = 0;
    font.end();
    const size_t afterEnd = TestFs::reads;
    font.tick();
    check(TestFs::handles == 0U && TestFs::reads == afterEnd && TestPsram::used == 0U,
          "Cancelled verification still reads SD or retains PSRAM");
    page.begin(font, sd);
    for (uint16_t size : {14, 20, 24}) {
      ready(font);
      check(font.metrics().pixelSize == size && TestFs::handles == 1U, "Size switch or file ownership failed");
      compareGlyphs(font, size);
      display.clear(); page.render(display, 0, font); save(display, "page-" + std::to_string(size));
      const size_t redrawReads = TestFs::reads;
      display.clear(); page.render(display, 0, font);
      if (usePsram) check(TestFs::reads == redrawReads, "Redrawing a complete sample still accesses SD");
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
      const size_t refreshes = TestFs::refreshes;
      const size_t allocations = TestPsram::allocations;
      const size_t switchBytes = TestFs::bytes;
      check(page.handleInput(false, true, false, font, sd), "Down size change failed");
      check(TestFs::refreshes == refreshes, "Switching sizes repeated capacity/root checks");
      check(TestPsram::allocations == allocations, "Size switch reallocated PSRAM");
      if (size == 24U)
        check(font.state() == SdBitmapFont::State::Ready && TestFs::bytes - switchBytes == 64U,
              "Returning to a verified size reread payload");
    }
    ready(font);
    check(font.metrics().pixelSize == 14U, "Size selector did not wrap");
    check(page.handleInput(true, false, false, font, sd), "Up size change failed");
    ready(font);
    check(font.metrics().pixelSize == 24U, "Up did not wrap to 24");
    SdBitmapFont::Glyph stale;
    check(font.glyph('A', stale), "Stale-session setup failed");
    const size_t retryRefreshes = TestFs::refreshes;
    page.handleInput(false, false, true, font, sd);
    check(font.state() == SdBitmapFont::State::Checking && TestFs::refreshes == retryRefreshes + 1U,
          "OK retry reused verification or skipped refreshing SD state");
    ready(font);
    uint8_t row[128];
    check(!font.readRow(stale, 0, row, sizeof(row)), "Old file/session offset was reused");
    font.open(14);
    check(font.state() == SdBitmapFont::State::Checking, "OK retry retained other size verification");
    ready(font);
    font.open(14);
    check(font.state() == SdBitmapFont::State::Ready, "Verified size was not retained");
    TestFs::root = std::string(kRoot) + "changed-header";
    font.open(14);
    check(font.state() == SdBitmapFont::State::Checking, "Changed header reused a previous verification");
    ready(font);
    page.begin(font, sd);
    check(font.state() == SdBitmapFont::State::Checking, "Reentering page retained verification");
    ready(font);

    for (const auto& bad : std::vector<std::pair<std::string, SdBitmapFont::Error>>{
        {"missing", SdBitmapFont::Error::OpenFailed}, {"header", SdBitmapFont::Error::Header},
        {"truncated", SdBitmapFont::Error::Header}, {"wrong-size", SdBitmapFont::Error::SizeMismatch},
        {"crc", SdBitmapFont::Error::Crc}}) {
      root(bad.first.c_str(), font);
      font.open(14);
      while (font.state() == SdBitmapFont::State::Checking) font.tick();
      check(font.state() == SdBitmapFont::State::Error && font.error() == bad.second && TestFs::handles == 0U,
            "Damaged file accepted or handle leaked");
      check(TestPsram::used == 0U, "File error retained PSRAM");
      display.clear(); page.render(display, 0, font); save(display, "error-" + bad.first);
    }
    for (const char* bad : {"bad-glyph", "padding"}) {
      root(bad, font); font.open(14); ready(font);
      SdBitmapFont::Glyph glyph;
      check(!font.glyph('A', glyph) && font.error() == SdBitmapFont::Error::Glyph && TestFs::handles == 0U,
            "Corrupt glyph bounds/padding accepted");
      check(TestPsram::used == 0U, "Glyph error retained PSRAM");
    }
    root("large", font); font.open(14); ready(font);
    SdBitmapFont::Glyph large;
    check(font.glyph('A', large) && large.length > 128U && large.xOffset < 0, "Large glyph not accepted");
    for (uint16_t y = 0; y < large.height; ++y) {
      check(font.readRow(large, y, row, sizeof(row)) && row[0] == 255 && row[16] == 128,
            "Streaming large glyph failed");
    }
    root("big-index", font); font.open(14); ready(font);
    const size_t reads = TestFs::reads;
    SdBitmapFont::Glyph extended;
    check(font.glyph(0x20000U, extended) && extended.advance == 8 && extended.length == 0U,
          "65537-entry index or supplementary scalar failed");
    check(TestFs::reads - reads <= 18U, "Large lookup scanned the file");
    root("valid", font); font.open(14);
    TestFs::unavailable = true;
    font.tick();
    check(font.error() == SdBitmapFont::Error::Read && TestFs::handles == 0U, "Removed card not handled");
    page.begin(font, sd);
    display.clear(); page.render(display, 0, font); save(display, "no-card");
    TestFs::unavailable = false;
    page.handleInput(false, false, true, font, sd); ready(font);
    font.open(20); ready(font);
    TestFs::unavailable = true;
    SdBitmapFont::Glyph uncached;
    check(!font.glyph('A', uncached) && font.error() == SdBitmapFont::Error::Read,
          "Ready-state card loss did not invalidate verification");
    TestFs::unavailable = false;
    font.open(14);
    check(font.state() == SdBitmapFont::State::Checking, "I/O error retained another size verification");
    ready(font);
    font.end();
    check(TestFs::handles == 0U && font.state() == SdBitmapFont::State::Closed &&
          TestPsram::used == 0U && TestPsram::allocations == TestPsram::frees,
          "Exit leaked a font handle or PSRAM allocation");
    if (usePsram) {
      size_t externalReads = 0, fallbackReads = 0;
      for (bool fail : {false, true}) {
        TestPsram::failAllocation = fail;
        root("valid", font); font.open(14); ready(font);
        check((font.cacheBytes() == 0U) == fail, "Failed allocation did not use internal fallback");
        const size_t reads = TestFs::reads;
        for (unsigned pass = 0; pass < 2; ++pass) {
          const size_t before = TestFs::reads;
          for (uint32_t cp = 0x4E00U; cp < 0x4F00U; ++cp) {
            SdBitmapFont::Glyph glyph;
            if (font.glyph(cp, glyph) && glyph.height)
              check(font.readRow(glyph, 0, row, sizeof(row)), "Working-set row failed");
            check(font.state() == SdBitmapFont::State::Ready, "Working-set glyph failed");
          }
          if (pass == 1U && !fail)
            check(TestFs::reads == before, "256-character working set did not stay cached");
        }
        if (fail) fallbackReads = TestFs::reads - reads;
        else {
          externalReads = TestFs::reads - reads;
          check(font.cacheStats().indexHits > 0 && font.cacheStats().glyphHits >= 256U,
                "PSRAM index/glyph cache did not register hits");
        }
        font.end();
        check(TestPsram::used == 0U, "Working-set cache leaked");
      }
      check(externalReads < fallbackReads, "Limited PSRAM cache did not reduce file reads");
      std::printf("PASS: 256-codepoint workload SD reads PSRAM=%zu fallback=%zu; second PSRAM pass=0\n",
                  externalReads, fallbackReads);
      TestPsram::failAllocation = false;
      root("valid", font); font.open(14); ready(font);
      SdBitmapFont::Glyph first;
      check(font.glyph(0x4E00U, first) && first.height > 0U, "Eviction reference missing");
      uint8_t expectedRow[128];
      check(font.readRow(first, 0, expectedRow, sizeof(expectedRow)), "Eviction reference read failed");
      const size_t initialAllocations = TestPsram::allocations;
      for (uint32_t i = 1; i <= 20U; ++i) {
        SdBitmapFont::Glyph collision;
        (void)font.glyph(0x4E00U + i * 128U, collision);
        check(font.state() == SdBitmapFont::State::Ready, "Cache collision corrupted font state");
      }
      check(font.readRow(first, 0, row, sizeof(row)) &&
            std::memcmp(row, expectedRow, (first.width + 7U) / 8U) == 0,
            "Evicted glyph reused another character's bitmap");
      check(TestPsram::allocations == initialAllocations, "Cache eviction grew its allocation");
      font.end();
      TestPsram::largest = 64U * 1024U;
      root("valid", font); font.open(14); ready(font);
      check(font.cacheBytes() == 0U && font.glyph('A', uncached), "Small free block did not fall back");
      font.end();
      TestPsram::largest = 8U * 1024U * 1024U;
      {
        SdBitmapFont scoped;
        scoped.open(14);
        check(TestPsram::used > 0U, "Scoped allocation failed");
      }
      check(TestPsram::used == 0U && TestFs::handles == 0U, "Destructor leaked PSRAM/file");
    }
    commonTests(font, display, usePsram);
    check(TestPsram::peak <= SdFontCacheBuffer::kBudgetBytes &&
          TestPsram::allocations == TestPsram::frees, "PSRAM budget/lifetime mismatch");
    std::printf("PASS: real 14/20/24 BIN glyphs, timed CRC/cancel/wrap, UTF-8, 65537 index, streaming/cache, errors/retry, sessions; service=%zu bytes\n",
                sizeof(SdBitmapFont));
    std::printf("PASS: cache=%s peak=%zu bytes; retry/end/header change recheck; allocation failures/cleanup\n",
                usePsram ? "PSRAM" : "internal", TestPsram::peak);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
