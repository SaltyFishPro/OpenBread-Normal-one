#pragma once

#include "../../services/OtaService.h"

class DisplayMonoTft;
class U8G2_FOR_ST73XX;

// 系统更新的布局、日志分页与滚动；联网、下载和应用仍由 OtaService 管理。
class SystemUpdatePage {
public:
  struct View {
    OtaService::State state = OtaService::State::Idle;
    OtaService::Error error = OtaService::Error::None;
    const char* version = "";
    const char* notes = "";
    uint32_t size = 0;
    uint32_t downloaded = 0;
    uint32_t total = 0;
    uint8_t percent = 0;
    bool connecting = false;
    bool postApplyResult = false;
    bool postApplySucceeded = false;
  };

  static View viewOf(const OtaService& ota);
  void prepareEnter();
  bool update(const View& view, bool active, bool popupVisible, uint32_t nowMs,
              U8G2_FOR_ST73XX& text);
  bool handleLogInput(bool up, bool down, uint32_t nowMs);
  bool needsAnimationFrame() const { return active_ && animating_; }
  uint8_t logPage() const { return page_; }
  uint8_t logPageCount() const { return static_cast<uint8_t>((lineCount_ + 1U) / 2U); }
  void render(DisplayMonoTft& display, int16_t yOffset, const View& view, uint32_t nowMs);

private:
  struct Line { uint16_t offset; uint8_t bytes; };
  static constexpr uint8_t kMaxLines = 40;
  static constexpr uint32_t kAutoMs = 4000;
  static constexpr uint32_t kManualPauseMs = 8000;
  static constexpr uint32_t kSlideMs = 550;
  void syncLogs(const View& view, U8G2_FOR_ST73XX& text);
  void startSlide(uint8_t next, int8_t direction, uint32_t nowMs);
  void drawLogPage(DisplayMonoTft& display, uint8_t page, int16_t yOffset);

  Line lines_[kMaxLines]{};
  const char* notes_ = nullptr;
  OtaService::State notesState_ = OtaService::State::Idle;
  uint8_t lineCount_ = 0;
  uint8_t page_ = 0;
  uint8_t previousPage_ = 0;
  int8_t direction_ = 1;
  bool active_ = false;
  bool logsVisible_ = false;
  bool animating_ = false;
  bool manualPause_ = false;
  uint32_t lastAdvanceMs_ = 0;
  uint32_t lastManualMs_ = 0;
  uint32_t slideStartMs_ = 0;
};
