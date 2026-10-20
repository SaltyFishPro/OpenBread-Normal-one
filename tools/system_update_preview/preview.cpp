#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <cstring>

#include "src/bsp/BoardConfig.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/ui/PopupView.h"
#include "src/ui/pages/SystemUpdatePage.h"
#include "src/services/OtaReleaseNotes.h"

// Preview link stubs: the real OtaService remains the only production source
// of these values; the renderer is exercised with explicit View fixtures below.
OtaService::State OtaService::state() const { return State::Idle; }
OtaService::Error OtaService::error() const { return Error::None; }
const OtaService::ManifestInfo& OtaService::manifest() const {
  static ManifestInfo info;
  return info;
}
uint32_t OtaService::downloadedBytes() const { return 0; }
uint32_t OtaService::totalBytes() const { return 0; }
uint8_t OtaService::progressPercent() const { return 0; }
bool OtaService::hasPostApplyResult() const { return false; }
bool OtaService::postApplySucceeded() const { return false; }

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

namespace {
using State = OtaService::State;
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void save(DisplayMonoTft& display, const char* name) {
  uint8_t raw[8064];
  display.captureFrame(raw);
  std::ofstream out(std::string("tmp/system-update-render/") + name + ".pgm", std::ios::binary);
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
  check(static_cast<bool>(out), "framebuffer save failed");
}
void shot(DisplayMonoTft& display, SystemUpdatePage& page, const char* name,
          const SystemUpdatePage::View& view, uint32_t now, int16_t yOffset = 0) {
  display.clear();
  page.render(display, yOffset, view, now);
  save(display, name);
}

void parserChecks() {
  char notes[768];
  check(!OtaReleaseNotes::parse("{\"release_note\":\"old\"}", notes, sizeof(notes)) && !notes[0],
        "legacy manifest should fall back");
  check(OtaReleaseNotes::parse(u8"{\"release_notes\":\"优化界面\\n修复连接\\\"问题\\\"\"}", notes, sizeof(notes)),
        "UTF8 and escaped newline parse failed");
  check(std::strcmp(notes, u8"优化界面\n修复连接\"问题\"") == 0, "decoded notes mismatch");
  check(OtaReleaseNotes::parse("{\"release_notes\":\"\\u4f18\\u5316\\n\\ud83d\\ude00\"}", notes, sizeof(notes)),
        "unicode escapes and surrogate pair failed");
  check(std::strcmp(notes, u8"优化\n😀") == 0, "unicode decode mismatch");
  char small[5];
  check(OtaReleaseNotes::parse(u8"{\"release_notes\":\"一二三\"}", small, sizeof(small)) &&
          std::strcmp(small, u8"一") == 0, "UTF8 truncation split a character");
  check(!OtaReleaseNotes::parse("{\"release_notes\":\"bad\\q\"}", notes, sizeof(notes)) && !notes[0],
        "invalid optional field should fall back");
  check(!OtaReleaseNotes::parse("{\"release_notes\":\"\\uD800\"}", notes, sizeof(notes)), "invalid surrogate accepted");
  check(!OtaReleaseNotes::parse("{\"nested\":{\"release_notes\":\"wrong\"}}", notes, sizeof(notes)),
        "nested notes field accepted");
  check(OtaReleaseNotes::parse("{\"release_note\":\"\\\"release_notes\\\": wrong\",\"release_notes\":\"right\"}",
                              notes, sizeof(notes)) && std::strcmp(notes, "right") == 0,
        "notes key inside a value was mistaken for a field");
  const std::string longJson = "{\"release_notes\":\"" + std::string(900, 'a') + "\"}";
  check(OtaReleaseNotes::parse(longJson.c_str(), notes, sizeof(notes)) && std::strlen(notes) == 767,
        "notes capacity not enforced");
}
}  // namespace

int main() {
  try {
    parserChecks();
    DisplayMonoTft display;
    display.begin();
    SystemUpdatePage page;
    SystemUpdatePage::View view;
    page.prepareEnter();
    shot(display, page, "01-transition", view, 0, 84);
    page.update(view, true, false, 1000, display.text());
    check(page.logPageCount() == 3 && page.logPage() == 0, "local logs page count incorrect");
    shot(display, page, "02-idle", view, 1000);
    check(!page.update(view, true, false, 4999, display.text()), "auto-scroll started too early");
    check(page.update(view, true, false, 5000, display.text()) && page.needsAnimationFrame(), "4s auto-scroll missing");
    shot(display, page, "03-log-scroll", view, 5275);
    check(page.update(view, true, false, 5550, display.text()) && !page.needsAnimationFrame(), "animation did not settle");
    shot(display, page, "04-log-page-two", view, 5550);
    check(page.handleLogInput(true, false, 6000) && page.logPage() == 0, "manual up failed");
    page.update(view, true, false, 6550, display.text());
    page.update(view, true, false, 13999, display.text());
    check(page.logPage() == 0 && !page.needsAnimationFrame(), "8s manual pause failed");
    page.update(view, true, false, 14000, display.text());
    check(page.logPage() == 1, "auto-scroll did not resume");
    page.update(view, true, true, 14100, display.text());
    check(!page.needsAnimationFrame(), "popup did not stop animation");
    page.update(view, false, false, 20000, display.text());
    check(!page.needsAnimationFrame(), "exit did not stop animation");
    page.prepareEnter();
    page.update(view, true, false, 0xFFFFF800U, display.text());
    page.update(view, true, false, 0xFFFFF800U + 4000U, display.text());
    check(page.logPage() == 1, "millis wrap broke scrolling");

    view.state = State::Checking;
    view.connecting = true;
    page.update(view, true, false, 30000, display.text());
    check(!page.needsAnimationFrame(), "checking kept log animation");
    shot(display, page, "05-checking", view, 30000);
    view.state = State::UpdateAvailable;
    view.version = "0.1.3"; // fixture, production obtains this from the real manifest.
    view.notes = u8"优化系统界面\n改善蓝牙连接体验\n改善更新进度显示\n优化连接等待页面";
    view.size = 3921248;
    page.update(view, true, false, 31000, display.text());
    check(page.logPage() == 0 && page.logPageCount() == 2, "remote logs not reset");
    shot(display, page, "06-available", view, 31000);
    check(page.handleLogInput(false, true, 32000) && page.logPage() == 1, "manual down failed");
    page.update(view, true, false, 32550, display.text());
    check(page.handleLogInput(false, true, 33000) && page.logPage() == 1, "manual paging should be bounded");
    view.state = State::Downloading;
    view.connecting = false;
    view.total = view.size;
    for (const uint8_t percent : {0, 68, 100}) {
      view.percent = percent;
      view.downloaded = static_cast<uint32_t>(static_cast<uint64_t>(view.total) * percent / 100U);
      page.update(view, true, false, 34000, display.text());
      const std::string name = "07-download-" + std::to_string(percent);
      shot(display, page, name.c_str(), view, 34000);
    }
    view.state = State::Verifying;
    page.update(view, true, false, 35000, display.text());
    shot(display, page, "08-verifying", view, 35000);
    view.state = State::ReadyToApply;
    page.update(view, true, false, 36000, display.text());
    shot(display, page, "09-ready", view, 36000);
    PopupView::ConfirmState popup;
    PopupView::open(popup, 36000);
    check(popup.primarySelected, "restart confirmation must default to cancel");
    PopupView::drawConfirm(display, 0, popup, u8"应用更新并重启？", u8"取消", u8"应用", 36100);
    save(display, "10-confirm-cancel");
    check(PopupView::handleInput(popup, false, false, false, false, true, 36100), "popup cancel input failed");
    check(PopupView::update(popup, 36200) == PopupView::Result::Cancelled, "default OK applied update");
    PopupView::open(popup, 36300);
    PopupView::handleInput(popup, false, true, false, false, false, 36400);
    PopupView::handleInput(popup, false, false, false, false, true, 36400);
    check(PopupView::update(popup, 36500) == PopupView::Result::Confirmed, "explicit apply failed");
    view.state = State::Applying;
    shot(display, page, "11-applying", view, 37000);
    view.state = State::UpToDate;
    page.update(view, true, false, 38000, display.text());
    shot(display, page, "12-latest", view, 38000);
    view.state = State::Idle;
    view.postApplyResult = view.postApplySucceeded = true;
    page.update(view, true, false, 39000, display.text());
    shot(display, page, "13-success", view, 39000);
    view.postApplyResult = false;
    view.state = State::Error;
    view.error = OtaService::Error::NoCredentials;
    page.update(view, true, false, 40000, display.text());
    shot(display, page, "14-no-wifi", view, 40000);
    view.error = OtaService::Error::ShaMismatch;
    shot(display, page, "15-checksum-error", view, 40000);
    view.state = State::UpdateAvailable;
    view.version = "123.456.789-preview";
    view.notes = u8"这是很长的一段更新日志，用于检查真实字体自动换行和中文完整性。\nABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    page.update(view, true, false, 41000, display.text());
    shot(display, page, "16-long-notes", view, 41000);
    view.notes = "";
    page.prepareEnter();
    page.update(view, true, false, 42000, display.text());
    shot(display, page, "17-empty-notes", view, 42000);
    std::puts("PASS: real framebuffer states, 4s/8s scrolling, popup, exit/re-entry, millis wrap, UTF8 optional notes");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
