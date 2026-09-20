#pragma once

#include <stdint.h>

#include "../PopupView.h"

class DisplayMonoTft;

class FocusClockPage {
public:
  struct Card {
    const char* titleZh;
    const char* const* options;
    const uint8_t* minutes;
    uint8_t optionCount;
    uint8_t defaultOption;
  };

  static constexpr uint8_t kHomeIndex = 3;
  static constexpr uint8_t kMenuItemIndex = 0;
  static constexpr uint8_t kCardCount = 3;
  static constexpr uint8_t kContinuousCardIndex = 0;
  static constexpr uint8_t kContinuousRounds = 3;
  static constexpr uint8_t kBreakMinutes = 5;

  bool isSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  // 从主界面进入页面时复位到卡片选择界面。
  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus);
  bool update(uint32_t nowMs);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         uint32_t nowMs);
  // Left 键：选择界面返回上级；计时中弹出"放弃专注"确认；完成页返回上级。
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, uint32_t nowMs) const;
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool isAnimating(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;

private:
  // 计时全部按"已用时间正计时"累加：持续任务 = (专注 + 休息) × 3，其余为一次性专注。
  enum class View : uint8_t {
    Selection,
    Focus,
    Break,
    Finished,
  };

  struct Animation {
    bool active = false;
    int8_t direction = 0;
    uint8_t fromIndex = 0;
    uint8_t toIndex = 0;
    uint32_t startMs = 0;
  };

  // 视图过渡：旧视图向上滑出、新视图从下方滑入，结束时才真正切换 view_。
  struct ViewTransition {
    bool active = false;
    View from = View::Selection;
    View to = View::Selection;
    bool resetSessionAfter = false;
    uint32_t startMs = 0;
  };

  void moveCard(int8_t direction, uint32_t nowMs);
  void moveOption(int8_t direction, uint32_t nowMs);
  int16_t optionPosition(uint32_t nowMs) const;
  uint8_t optionIndexForCard(uint8_t cardIndex) const;
  void resetOptionForCard(uint8_t cardIndex);
  uint8_t currentOptionIndex() const;
  uint32_t phaseTargetMs() const;
  uint32_t phaseElapsedMs(uint32_t nowMs) const;
  uint32_t sessionElapsedMs(uint32_t nowMs) const;
  uint16_t phaseProgressFixed(uint32_t nowMs) const;
  void startSession(uint32_t nowMs);
  void startPhase(View view, uint32_t nowMs);
  void advancePhase(uint32_t nowMs);
  void finishSession(uint32_t nowMs);
  void stopSession(uint32_t nowMs);
  void beginViewTransition(View to, bool resetSessionAfter, uint32_t nowMs);
  void applyViewTransition();
  void drawView(View view, DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const;

  const char* sessionTitle() const;
  void drawSelection(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const;
  void drawTimer(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const;
  void drawFinished(DisplayMonoTft& display, int16_t yOffset) const;

  uint8_t cardIndex_ = 0;
  uint8_t optionIndex_[kCardCount] = {1, 1, 0};
  Animation cardAnimation_;
  int16_t optionAnimationFromPosition_ = 0;
  uint32_t optionAnimationStartMs_ = 0;
  bool optionAnimationActive_ = false;

  View view_ = View::Selection;
  bool timerPaused_ = false;
  PopupView::ConfirmState abandonConfirm_;
  uint8_t roundIndex_ = 0;
  uint32_t phaseAccumMs_ = 0;
  uint32_t phaseStartMs_ = 0;
  uint32_t sessionAccumMs_ = 0;
  uint32_t finishedTotalMs_ = 0;
  uint32_t lastShownSecond_ = 0xFFFFFFFFU;
  ViewTransition viewTransition_;

  static constexpr uint32_t kCardAnimationMs = 360;
  static constexpr uint32_t kOptionAnimationMs = 100;
  static constexpr uint32_t kViewTransitionMs = 240;
};
