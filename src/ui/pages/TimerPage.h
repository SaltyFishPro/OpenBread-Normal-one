#pragma once

#include <stdint.h>

#include "../PopupView.h"

class DisplayMonoTft;
class SdBitmapFont;

// 定时器（倒计时）详情页。由主菜单「定时器」直达，自持状态、输入、弹窗与绘制，
// 与 FocusClockPage 保持同一套 UiManager 路由约定。
// 预设 1/5/10/15/30/60 分钟，自定义 1–99 分钟；运行中 OK 暂停/继续，Left 放弃，
// 到点进入完成页，任意按键返回预设。
class TimerPage {
public:
  static constexpr uint8_t kHomeIndex = 6;
  static constexpr uint8_t kMenuItemIndex = 0;
  static constexpr uint8_t kPresetCount = 7;
  static constexpr uint8_t kCustomPresetIndex = 6;

  bool isSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  // 从主菜单进入时复位到预设页。
  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus);
  bool update(uint32_t nowMs);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         uint32_t nowMs);
  // Left 键：预设页返回上级；自定义页回预设；计时中弹出"放弃"确认；完成页回预设。
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, uint32_t nowMs,
                    SdBitmapFont* titleFont) const;
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool isAnimating(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;

private:
  enum class View : uint8_t { Preset, Custom, Running, Done };

  void resetSession();
  void stopToPreset();
  void startTimer(uint32_t nowMs, uint32_t totalSeconds);
  uint32_t remainingMs(uint32_t nowMs) const;
  static uint32_t presetSeconds(uint8_t index);
  static const char* presetLabel(uint8_t index);

  void drawPreset(DisplayMonoTft& display, int16_t yOffset) const;
  void drawCustom(DisplayMonoTft& display, int16_t yOffset) const;
  void drawRunning(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const;
  void drawDone(DisplayMonoTft& display, int16_t yOffset, SdBitmapFont* titleFont) const;

  View view_ = View::Preset;
  uint8_t presetIndex_ = 0;
  uint8_t customMinutes_ = 5;
  uint8_t stepMinutes_ = 1;
  uint32_t totalMs_ = 0;
  uint32_t remainMs_ = 0;
  uint32_t deadlineMs_ = 0;
  bool paused_ = false;
  uint32_t lastShownSec_ = 0xFFFFFFFFU;
  PopupView::ConfirmState abandonConfirm_;
};
