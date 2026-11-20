#include "DisplayMonoTft.h"

#include <SPI.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>

#include <cstring>

#include "BoardConfig.h"
#include "../log/Log.h"

// 差异刷新窗口：ST7305 的 MX 位会反转列地址窗口，displayByteWindow 已按
// 面板实现做镜像；实机确认后可置 0 退回“只做无变化不推屏、其余整帧推送”。
#ifndef OB_DISPLAY_PARTIAL_WINDOWS
#define OB_DISPLAY_PARTIAL_WINDOWS 1
#endif

namespace {
constexpr uint32_t kTeWaitTimeoutUs = 35000;

const ST73xxPins kDisplayPins = {
    BoardConfig::kPinDc,
    BoardConfig::kPinCs,
    BoardConfig::kPinSclk,
    BoardConfig::kPinSdin,
    BoardConfig::kPinRst,
};

constexpr int kDisplaySleepPins[] = {
    BoardConfig::kPinSdin,
    BoardConfig::kPinSclk,
    BoardConfig::kPinCs,
    BoardConfig::kPinDc,
    BoardConfig::kPinRst,
};

void holdDisplayPins(bool hold) {
  for (int pin : kDisplaySleepPins) {
    if (hold) {
      gpio_hold_en(static_cast<gpio_num_t>(pin));
    } else {
      gpio_hold_dis(static_cast<gpio_num_t>(pin));
    }
  }
}
}  // namespace

DisplayMonoTft::DisplayMonoTft() : display_(kDisplayPins, SPI) {}

bool DisplayMonoTft::begin() {
  SPI.begin(BoardConfig::kPinSclk, -1, BoardConfig::kPinSdin, BoardConfig::kPinCs);
  display_.initialize();
  display_.High_Power_Mode();
  display_.display_on(true);
  display_.display_Inversion(false);
  display_.setRotation(BoardConfig::kDisplayRotation);
  pinMode(BoardConfig::kPinTftTe, INPUT);
  teState_ = TeState::Unknown;
  lastTeWaitUs_ = 0;
  lastPresentUs_ = 0;
  teSyncAnnounced_ = false;

  text_.begin(display_);
  text_.setFontMode(1);
  text_.setForegroundColor(ST7305_COLOR_BLACK);
  text_.setBackgroundColor(ST7305_COLOR_WHITE);
  // Keep a small Latin/number face available for the transition period while
  // Chinese UI text is being moved to the SD bitmap fonts. The display starts
  // without mounting SD and pages may replace this face when an SD font is ready.
  text_.setFont(u8g2_font_7x14_tf);
  return true;
}

void DisplayMonoTft::clear() { display_.clearDisplay(); }

void DisplayMonoTft::present() {
  display_.display();
  rememberFrame();
}

void DisplayMonoTft::presentSynced() {
  lastTeWaitUs_ = 0;
  const uint32_t presentStartUs = micros();
  if (teState_ != TeState::Unavailable) {
    (void)waitForTe();
  }
  display_.display();
  lastPresentUs_ = micros() - presentStartUs;
  rememberFrame();
}

bool DisplayMonoTft::waitForTe() {
  const uint32_t startUs = micros();
  int previousLevel = digitalRead(BoardConfig::kPinTftTe);
  while (micros() - startUs < kTeWaitTimeoutUs) {
    const int level = digitalRead(BoardConfig::kPinTftTe);
    if (previousLevel == LOW && level == HIGH) {
      if (teState_ == TeState::Unknown) {
        teState_ = TeState::Available;
        if (!teSyncAnnounced_) {
          LogI("DISPLAY", "TE sync active gpio=%d", BoardConfig::kPinTftTe);
          teSyncAnnounced_ = true;
        }
      }
      lastTeWaitUs_ = micros() - startUs;
      return true;
    }
    previousLevel = level;
    delayMicroseconds(50);
  }

  LogW("DISPLAY", "TE timeout on GPIO%d; check TE wiring and pulse, using unsynced refresh",
       BoardConfig::kPinTftTe);
  lastTeWaitUs_ = micros() - startUs;
  teState_ = TeState::Unavailable;
  return false;
}

void DisplayMonoTft::presentRegion(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  if (x1 > x2) {
    const int16_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    const int16_t t = y1;
    y1 = y2;
    y2 = t;
  }

  const int16_t maxX = static_cast<int16_t>(display_.getDisplayWidth() - 1);
  const int16_t maxY = static_cast<int16_t>(display_.getDisplayHeight() - 1);
  if (maxX < 0 || maxY < 0 || x2 < 0 || y2 < 0 || x1 > maxX || y1 > maxY) {
    return;
  }
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > maxX) x2 = maxX;
  if (y2 > maxY) y2 = maxY;

  int16_t px1 = 0;
  int16_t py1 = 0;
  int16_t px2 = 0;
  int16_t py2 = 0;
  display_.logicalToPhysical(x1, y1, px1, py1);
  display_.logicalToPhysical(x2, y2, px2, py2);
  display_.displayRegion(static_cast<uint16_t>(px1), static_cast<uint16_t>(py1),
                         static_cast<uint16_t>(px2), static_cast<uint16_t>(py2));
  rememberFrame();
}

void DisplayMonoTft::rememberFrame() {
  if (history_ == nullptr || !historyValid_) return;
  memcpy(history_, display_.frameBuffer(), frameBufferBytes());
}

void DisplayMonoTft::presentChanged() {
  const size_t bytes = frameBufferBytes();
  if (history_ == nullptr) {
    history_ = static_cast<uint8_t*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    historyValid_ = false;
  }
  if (history_ == nullptr) {
    present();
    return;
  }
  const uint8_t* frame = display_.frameBuffer();
  const uint16_t rowBytes = static_cast<uint16_t>(display_.panelWidth() / 4U);
  const uint16_t rows = static_cast<uint16_t>(display_.panelHeight() / 2U);
  if (!historyValid_) {
    display_.display();
    memcpy(history_, frame, bytes);
    historyValid_ = true;
    return;
  }
  uint16_t rowFirst = 0;
  uint16_t rowLast = 0;
  uint16_t byteFirst = rowBytes;
  uint16_t byteLast = 0;
  size_t changed = 0;
  bool any = false;
  for (uint16_t row = 0; row < rows; ++row) {
    const uint8_t* now = frame + static_cast<size_t>(row) * rowBytes;
    const uint8_t* was = history_ + static_cast<size_t>(row) * rowBytes;
    uint16_t first = rowBytes;
    uint16_t last = 0;
    for (uint16_t col = 0; col < rowBytes; ++col) {
      if (now[col] == was[col]) continue;
      if (first == rowBytes) first = col;
      last = col;
      ++changed;
    }
    if (first == rowBytes) continue;
    if (!any) {
      rowFirst = row;
      any = true;
    }
    rowLast = row;
    if (first < byteFirst) byteFirst = first;
    if (last > byteLast) byteLast = last;
  }
  if (!any) return;  // 画面与上一次上屏完全一致，不必刷新面板。
#if OB_DISPLAY_PARTIAL_WINDOWS
  constexpr size_t kMaxPartialBytes = 2400;  // 超过约 30% 直接整帧推送。
  if (changed <= kMaxPartialBytes) {
    const uint16_t byteStart = static_cast<uint16_t>((byteFirst / 3U) * 3U);
    uint16_t byteEnd = static_cast<uint16_t>((byteLast / 3U) * 3U + 2U);
    if (byteEnd >= rowBytes) byteEnd = static_cast<uint16_t>(rowBytes - 1U);
    const uint16_t rowStart = rowFirst > 0U ? static_cast<uint16_t>(rowFirst - 1U) : 0U;
    const uint16_t rowEnd = (rowLast + 1U) < rows ? static_cast<uint16_t>(rowLast + 1U)
                                                  : static_cast<uint16_t>(rows - 1U);
    display_.displayByteWindow(byteStart, byteEnd, rowStart, rowEnd);
    memcpy(history_, frame, bytes);
    LogD("DISPLAY", "partial changed=%u rows=%u..%u bytes=%u..%u",
         static_cast<unsigned>(changed), rowStart, rowEnd, byteStart, byteEnd);
    return;
  }
#else
  (void)changed;
  (void)rowFirst;
  (void)rowLast;
  (void)byteFirst;
  (void)byteLast;
#endif
  display_.display();
  memcpy(history_, frame, bytes);
}

void DisplayMonoTft::prepareForSleepKeepDisplay() {
  display_.Low_Power_Mode();
  holdDisplayPins(true);
}

void DisplayMonoTft::prepareForSleep() {
  display_.display_on(false);
  display_.Low_Power_Mode();
}

void DisplayMonoTft::restoreAfterSleep() {
  holdDisplayPins(false);
  SPI.begin(BoardConfig::kPinSclk, -1, BoardConfig::kPinSdin, BoardConfig::kPinCs);
  display_.High_Power_Mode();
  display_.display_on(true);
  display_.display_Inversion(false);
  pinMode(BoardConfig::kPinTftTe, INPUT);
  teState_ = TeState::Unknown;
  lastTeWaitUs_ = 0;
  lastPresentUs_ = 0;
  teSyncAnnounced_ = false;
  // 休眠/唤醒后无法确定面板内容，下一次差异刷新退回整帧。
  historyValid_ = false;
}

int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }

int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }

size_t DisplayMonoTft::frameBufferBytes() const { return display_.frameBufferLength(); }

void DisplayMonoTft::captureFrame(uint8_t* dst) const { display_.copyFrameBufferTo(dst); }

void DisplayMonoTft::restoreFrame(const uint8_t* src) { display_.copyFrameBufferFrom(src); }

ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }

U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }
