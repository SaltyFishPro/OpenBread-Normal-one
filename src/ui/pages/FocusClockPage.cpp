#include "FocusClockPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"
#include "../Segment7Font.h"
#include "../TextUtils.h"

namespace {
constexpr int16_t kCardX = 20;
constexpr int16_t kCardY = 20;
constexpr int16_t kCardWidth = 344;
constexpr int16_t kCardHeight = 128;
constexpr int16_t kCardRadius = 18;
constexpr int16_t kCardTitleX = 18;
constexpr int16_t kCardTitleBaseline = 35;
constexpr int16_t kCardCounterRight = 319;
constexpr int16_t kOptionY = 66;
constexpr int16_t kOptionHeight = 28;
constexpr int16_t kOptionRadius = 12;
constexpr int16_t kOptionGap = 9;
constexpr int16_t kOptionPaddingX = 28;
constexpr uint16_t kEaseScale = AnimMath::kFixedScale;

// 计时界面布局（384×168）。
constexpr int16_t kHeaderHeight = 28;
constexpr int16_t kHeaderBaseline = 19;
constexpr int16_t kTimerDigitTop = 38;
constexpr int16_t kTimerInfoBaseline = 132;
constexpr int16_t kTimerBarX = 20;
constexpr int16_t kTimerBarY = 140;
constexpr int16_t kTimerBarHeight = 12;
constexpr Segment7Font::Style kTimerDigitStyle = {46, 74, 6, 5, 12, ST7305_COLOR_BLACK};

const char* const kContinuousOptions[] = {"10分钟", "15分钟", "20分钟"};
const char* const kShortOptions[] = {"10分钟", "15分钟", "20分钟"};
const char* const kLongOptions[] = {"30分钟", "60分钟"};
const uint8_t kContinuousMinutes[] = {10, 15, 20};
const uint8_t kShortMinutes[] = {10, 15, 20};
const uint8_t kLongMinutes[] = {30, 60};

const FocusClockPage::Card kCards[] = {
    {"持续任务", kContinuousOptions, kContinuousMinutes, 3, 1},
    {"短专注", kShortOptions, kShortMinutes, 3, 1},
    {"长专注", kLongOptions, kLongMinutes, 2, 0},
};

void formatMinutesSeconds(uint32_t ms, char* out, size_t outSize) {
  const uint32_t totalSeconds = ms / 1000U;
  snprintf(out, outSize, "%02u:%02u", static_cast<unsigned>(totalSeconds / 60U),
           static_cast<unsigned>(totalSeconds % 60U));
}

void drawTextCentered(U8G2_FOR_ST73XX& text, const char* value, int16_t centerX,
                      int16_t baseline, uint16_t foreground, uint16_t background) {
  text.setBackgroundColor(background);
  text.setForegroundColor(foreground);
  text.setFontMode(1);
  text.drawUTF8(TextUtils::centeredTextX(text, value, centerX), baseline, value);
}

void drawCardShell(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y) {
  // The previously drawn white shadow was invisible against the white page and
  // doubled the scanline cost of every card on every animation frame.
  DrawUtils::drawRoundRect(canvas, x, y, kCardWidth, kCardHeight, kCardRadius,
                           ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
}

// 计时界面顶部黑条：左侧标题/阶段，右侧目标或状态。
void drawHeaderBand(DisplayMonoTft& display, int16_t yOffset, const char* leftText,
                    const char* rightText) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  canvas.drawFilledRectangle(0, yOffset, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(12, static_cast<int16_t>(yOffset + kHeaderBaseline), leftText);
  if (rightText != nullptr && rightText[0] != '\0') {
    const int16_t rightWidth = text.getUTF8Width(rightText);
    text.drawUTF8(static_cast<int16_t>(width - 12 - rightWidth),
                  static_cast<int16_t>(yOffset + kHeaderBaseline), rightText);
  }
  // 恢复默认文本样式，避免泄漏到后续绘制。
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
}

void drawCardContent(DisplayMonoTft& display, uint8_t cardIndex, int16_t x, int16_t y,
                     int16_t selectionPosition) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  if (y >= display.height() || y + kCardHeight + 2 <= 0) {
    return;
  }
  const FocusClockPage::Card& card = kCards[cardIndex];
  drawCardShell(canvas, x, y);

  text.setFont(chinese_font_all);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(static_cast<int16_t>(x + kCardTitleX),
                static_cast<int16_t>(y + kCardTitleBaseline), card.titleZh);

  char pageNumber[4];
  snprintf(pageNumber, sizeof(pageNumber), "%u", static_cast<unsigned>(cardIndex + 1U));
  const int16_t pageWidth = text.getUTF8Width(pageNumber);
  text.drawUTF8(static_cast<int16_t>(x + kCardCounterRight - pageWidth),
                static_cast<int16_t>(y + 26), pageNumber);

  int16_t widths[3] = {0, 0, 0};
  int16_t positions[3] = {0, 0, 0};
  int16_t totalWidth = 0;
  text.setFont(chinese_font_all);
  for (uint8_t index = 0; index < card.optionCount; ++index) {
    widths[index] = static_cast<int16_t>(text.getUTF8Width(card.options[index]) +
                                         kOptionPaddingX);
    totalWidth = static_cast<int16_t>(totalWidth + widths[index]);
  }
  totalWidth = static_cast<int16_t>(totalWidth + kOptionGap * (card.optionCount - 1U));
  int16_t optionX = static_cast<int16_t>(x + (kCardWidth - totalWidth) / 2);
  for (uint8_t index = 0; index < card.optionCount; ++index) {
    positions[index] = optionX;
    DrawUtils::drawRoundRect(canvas, optionX, static_cast<int16_t>(y + kOptionY),
                             widths[index], kOptionHeight, kOptionRadius, ST7305_COLOR_BLACK,
                             ST7305_COLOR_WHITE);
    optionX = static_cast<int16_t>(optionX + widths[index] + kOptionGap);
  }

  const uint8_t selection = static_cast<uint8_t>(selectionPosition / kEaseScale);
  const uint8_t next = selection + 1U < card.optionCount ? selection + 1U : selection;
  const int16_t fraction = static_cast<int16_t>(selectionPosition % kEaseScale);
  const int16_t selectedX = static_cast<int16_t>(positions[selection] +
      (positions[next] - positions[selection]) * fraction / kEaseScale);
  const int16_t selectedWidth = static_cast<int16_t>(widths[selection] +
      (widths[next] - widths[selection]) * fraction / kEaseScale);
  DrawUtils::drawRoundRect(canvas, selectedX, static_cast<int16_t>(y + kOptionY), selectedWidth,
                           kOptionHeight, kOptionRadius, ST7305_COLOR_WHITE,
                           ST7305_COLOR_BLACK);

  for (uint8_t index = 0; index < card.optionCount; ++index) {
    const int16_t center = static_cast<int16_t>(positions[index] + widths[index] / 2);
    const bool selectedLabel = center >= selectedX && center <= selectedX + selectedWidth;
    // Center the full glyph box inside the pill using the font's real ascent
    // and descent instead of a fixed baseline.
    const int16_t ascent = text.getFontAscent();
    const int16_t descent = text.getFontDescent();
    const int16_t textBoxHeight = static_cast<int16_t>(ascent - descent);
    const int16_t optionBaseline = static_cast<int16_t>(
        y + kOptionY + (kOptionHeight - textBoxHeight) / 2 + ascent);
    drawTextCentered(text, card.options[index], center,
                     optionBaseline,
                     selectedLabel ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE,
                     ST7305_COLOR_BLACK);
  }
}
}  // namespace

bool FocusClockPage::isSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kMenuItemIndex;
}

void FocusClockPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return;
  }
  // 进入页面时直接复位到卡片选择界面，不做过渡。
  abandonConfirm_ = PopupView::ConfirmState{};
  viewTransition_.active = false;
  view_ = View::Selection;
  timerPaused_ = false;
  roundIndex_ = 0;
  phaseAccumMs_ = 0;
  sessionAccumMs_ = 0;
  lastShownSecond_ = 0xFFFFFFFFU;
}

bool FocusClockPage::update(uint32_t nowMs) {
  bool changed = false;
  if (cardAnimation_.active && nowMs - cardAnimation_.startMs >= kCardAnimationMs) {
    cardIndex_ = cardAnimation_.toIndex;
    cardAnimation_.active = false;
    changed = true;
  }
  if (optionAnimationActive_ && nowMs - optionAnimationStartMs_ >= kOptionAnimationMs) {
    optionAnimationActive_ = false;
    changed = true;
  }

  // 弹窗退场动画结束后再执行动作。
  const PopupView::Result popupResult = PopupView::update(abandonConfirm_, nowMs);
  if (popupResult == PopupView::Result::Confirmed) {
    stopSession(nowMs);
    changed = true;
  } else if (popupResult == PopupView::Result::Cancelled) {
    changed = true;
  }

  // 视图过渡期间只推进动画，不再推进计时阶段。
  if (viewTransition_.active) {
    if (nowMs - viewTransition_.startMs >= kViewTransitionMs) {
      applyViewTransition();
      return true;
    }
    const uint32_t second = phaseElapsedMs(nowMs) / 1000U;
    if (second != lastShownSecond_) {
      lastShownSecond_ = second;
      changed = true;
    }
    return changed;
  }

  if (view_ != View::Focus && view_ != View::Break) {
    return changed;
  }
  if (timerPaused_) {
    return changed;
  }

  const uint32_t elapsed = phaseElapsedMs(nowMs);
  if (elapsed >= phaseTargetMs()) {
    advancePhase(nowMs);
    return true;
  }
  // 界面只显示到秒，秒数变化才需要重绘，避免每帧刷新计时界面。
  const uint32_t second = elapsed / 1000U;
  if (second != lastShownSecond_) {
    lastShownSecond_ = second;
    changed = true;
  }
  return changed;
}

uint8_t FocusClockPage::currentOptionIndex() const {
  const uint8_t visibleIndex = cardAnimation_.active ? cardAnimation_.toIndex : cardIndex_;
  return optionIndexForCard(visibleIndex);
}

const char* FocusClockPage::sessionTitle() const { return kCards[cardIndex_].titleZh; }

uint32_t FocusClockPage::phaseTargetMs() const {
  if (view_ == View::Focus) {
    return static_cast<uint32_t>(kCards[cardIndex_].minutes[currentOptionIndex()]) * 60000UL;
  }
  if (view_ == View::Break) {
    return static_cast<uint32_t>(kBreakMinutes) * 60000UL;
  }
  return 0;
}

uint32_t FocusClockPage::phaseElapsedMs(uint32_t nowMs) const {
  if (view_ != View::Focus && view_ != View::Break) {
    return 0;
  }
  if (timerPaused_) {
    return phaseAccumMs_;
  }
  return phaseAccumMs_ + (nowMs - phaseStartMs_);
}

uint32_t FocusClockPage::sessionElapsedMs(uint32_t nowMs) const {
  return sessionAccumMs_ + phaseElapsedMs(nowMs);
}

uint16_t FocusClockPage::phaseProgressFixed(uint32_t nowMs) const {
  const uint32_t target = phaseTargetMs();
  if (target == 0) {
    return 0;
  }
  const uint32_t elapsed = phaseElapsedMs(nowMs);
  if (elapsed >= target) {
    return kEaseScale;
  }
  return static_cast<uint16_t>(elapsed * kEaseScale / target);
}

void FocusClockPage::startPhase(View view, uint32_t nowMs) {
  view_ = view;
  timerPaused_ = false;
  phaseAccumMs_ = 0;
  phaseStartMs_ = nowMs;
  lastShownSecond_ = 0xFFFFFFFFU;
}

void FocusClockPage::startSession(uint32_t nowMs) {
  // 先记录来源视图（卡片选择页），再切到计时状态，过渡期间两个视图同时绘制。
  beginViewTransition(View::Focus, false, nowMs);
  roundIndex_ = 0;
  sessionAccumMs_ = 0;
  finishedTotalMs_ = 0;
  startPhase(View::Focus, nowMs);
}

void FocusClockPage::advancePhase(uint32_t nowMs) {
  sessionAccumMs_ = sessionAccumMs_ + phaseElapsedMs(nowMs);

  if (view_ == View::Focus) {
    if (cardIndex_ == kContinuousCardIndex) {
      startPhase(View::Break, nowMs);
      return;
    }
    finishSession(nowMs);
    return;
  }

  // 休息结束：还有下一轮就继续专注，三轮走完即结束。
  if (static_cast<uint8_t>(roundIndex_ + 1U) < kContinuousRounds) {
    roundIndex_ = static_cast<uint8_t>(roundIndex_ + 1U);
    startPhase(View::Focus, nowMs);
    return;
  }
  finishSession(nowMs);
}

void FocusClockPage::finishSession(uint32_t nowMs) {
  // 当前阶段已经由 advancePhase() 并入总累计，这里直接落盘，避免重复累加。
  finishedTotalMs_ = sessionAccumMs_;
  // 过渡期间继续显示计时界面，结束后再切到完成页。
  viewTransition_.active = true;
  viewTransition_.from = view_;
  viewTransition_.to = View::Finished;
  viewTransition_.resetSessionAfter = false;
  viewTransition_.startMs = nowMs;
  lastShownSecond_ = 0xFFFFFFFFU;
}

void FocusClockPage::stopSession(uint32_t nowMs) {
  // 保留计时状态直到过渡结束，让滑出的旧视图仍显示真实时间。
  beginViewTransition(View::Selection, true, nowMs);
}

void FocusClockPage::beginViewTransition(View to, bool resetSessionAfter, uint32_t nowMs) {
  if (view_ == to) {
    viewTransition_.active = false;
    return;
  }
  viewTransition_.active = true;
  viewTransition_.from = view_;
  viewTransition_.to = to;
  viewTransition_.resetSessionAfter = resetSessionAfter;
  viewTransition_.startMs = nowMs;
}

void FocusClockPage::applyViewTransition() {
  view_ = viewTransition_.to;
  viewTransition_.active = false;
  if (viewTransition_.resetSessionAfter) {
    timerPaused_ = false;
    roundIndex_ = 0;
    phaseAccumMs_ = 0;
    sessionAccumMs_ = 0;
  }
  lastShownSecond_ = 0xFFFFFFFFU;
}

uint8_t FocusClockPage::optionIndexForCard(uint8_t cardIndex) const {
  return cardIndex < kCardCount ? optionIndex_[cardIndex] : 0;
}

void FocusClockPage::resetOptionForCard(uint8_t cardIndex) {
  if (cardIndex < kCardCount) {
    optionIndex_[cardIndex] = kCards[cardIndex].defaultOption;
  }
}

void FocusClockPage::moveOption(int8_t direction, uint32_t nowMs) {
  if (direction == 0) {
    return;
  }
  // Input belongs to the destination as soon as a card switch starts.
  const uint8_t visibleIndex = cardAnimation_.active ? cardAnimation_.toIndex : cardIndex_;
  const Card& card = kCards[visibleIndex];
  const uint8_t current = optionIndexForCard(visibleIndex);
  const uint8_t target = direction < 0
                             ? (current == 0 ? static_cast<uint8_t>(card.optionCount - 1U)
                                             : static_cast<uint8_t>(current - 1U))
                             : static_cast<uint8_t>((current + 1U) % card.optionCount);
  // Retarget from the interpolated position, never jump to the last destination.
  optionAnimationFromPosition_ = optionPosition(nowMs);
  optionAnimationStartMs_ = nowMs;
  optionAnimationActive_ = true;
  optionIndex_[visibleIndex] = target;
}

int16_t FocusClockPage::optionPosition(uint32_t nowMs) const {
  const uint8_t visibleIndex = cardAnimation_.active ? cardAnimation_.toIndex : cardIndex_;
  const int16_t target = static_cast<int16_t>(optionIndexForCard(visibleIndex) * kEaseScale);
  if (!optionAnimationActive_) {
    return target;
  }
  const uint16_t progress = AnimMath::fixedEaseOutCubic(
      AnimMath::fixedClampProgress(nowMs - optionAnimationStartMs_, kOptionAnimationMs));
  return static_cast<int16_t>(optionAnimationFromPosition_ +
      (target - optionAnimationFromPosition_) * progress / kEaseScale);
}

void FocusClockPage::moveCard(int8_t direction, uint32_t nowMs) {
  if (direction == 0) {
    return;
  }
  if (cardAnimation_.active) {
    const uint16_t progress =
        AnimMath::fixedClampProgress(nowMs - cardAnimation_.startMs, kCardAnimationMs);
    const bool completed = progress >= (kEaseScale / 2U);
    cardIndex_ = (direction == cardAnimation_.direction || completed)
                     ? cardAnimation_.toIndex
                     : cardAnimation_.fromIndex;
    cardAnimation_.active = false;
    optionAnimationActive_ = false;
  }
  const uint8_t target = direction < 0
                             ? (cardIndex_ == 0 ? static_cast<uint8_t>(kCardCount - 1U)
                                                : static_cast<uint8_t>(cardIndex_ - 1U))
                             : static_cast<uint8_t>((cardIndex_ + 1U) % kCardCount);
  cardAnimation_ = {true, static_cast<int8_t>(direction < 0 ? -1 : 1), cardIndex_, target,
                    nowMs};
  optionAnimationActive_ = false;
  resetOptionForCard(target);
}

bool FocusClockPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                                       bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                                       uint32_t nowMs) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  // 放弃确认弹窗：Right/Up/Down 切换选项，OK 确认；Left 由 handleDetailBack 关闭。
  if (PopupView::isVisible(abandonConfirm_.anim)) {
    return PopupView::handleInput(abandonConfirm_, false, rightEdge, upEdge, downEdge, okEdge,
                                  nowMs);
  }

  if (view_ == View::Focus || view_ == View::Break) {
    // 视图过渡期间不接收新的页面操作。
    if (viewTransition_.active) {
      return rightEdge || upEdge || downEdge || okEdge;
    }
    if (okEdge) {
      if (timerPaused_) {
        timerPaused_ = false;
        phaseStartMs_ = nowMs;
        lastShownSecond_ = 0xFFFFFFFFU;
      } else {
        phaseAccumMs_ = phaseElapsedMs(nowMs);
        timerPaused_ = true;
      }
      return true;
    }
    // 运行界面的 Right/Up/Down 无功能。
    return rightEdge || upEdge || downEdge;
  }

  if (view_ == View::Finished) {
    if (okEdge) {
      startSession(nowMs);
      return true;
    }
    return rightEdge || upEdge || downEdge;
  }

  if (leftEdge) {
    moveOption(-1, nowMs);
    return true;
  }
  if (rightEdge) {
    moveOption(1, nowMs);
    return true;
  }
  if (upEdge) {
    moveCard(-1, nowMs);
    return true;
  }
  if (downEdge) {
    moveCard(1, nowMs);
    return true;
  }
  if (okEdge) {
    if (cardAnimation_.active || optionAnimationActive_) {
      return true;
    }
    startSession(nowMs);
    return true;
  }
  return false;
}

bool FocusClockPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (PopupView::isVisible(abandonConfirm_.anim)) {
    // Left 在弹窗中等价于"继续专注"。
    PopupView::requestClose(abandonConfirm_, false, nowMs);
    return true;
  }
  if (viewTransition_.active) {
    // 过渡动画进行中，忽略返回，等动画结束。
    return true;
  }
  if (view_ == View::Focus || view_ == View::Break) {
    // 计时中先确认，避免误触丢进度。
    PopupView::open(abandonConfirm_, nowMs);
    return true;
  }
  // 选择界面与完成页交给 UiManager 返回上级。
  return false;
}

bool FocusClockPage::isAnimating(uint8_t homeFocus, uint8_t sectionFocus,
                                 uint32_t nowMs) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (cardAnimation_.active && nowMs - cardAnimation_.startMs < kCardAnimationMs) {
    return true;
  }
  if (optionAnimationActive_ && nowMs - optionAnimationStartMs_ < kOptionAnimationMs) {
    return true;
  }
  return viewTransition_.active || PopupView::isAnimating(abandonConfirm_.anim);
}

bool FocusClockPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                         uint32_t nowMs) const {
  return isAnimating(homeFocus, sectionFocus, nowMs);
}

bool FocusClockPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                                  DisplayMonoTft& display, uint32_t nowMs) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (yOffset >= display.height()) {
    return true;
  }

  if (viewTransition_.active) {
    // 旧视图向上滑出、新视图从下方滑入，靠屏幕边缘自然裁切。
    const uint16_t raw =
        AnimMath::fixedClampProgress(nowMs - viewTransition_.startMs, kViewTransitionMs);
    const uint16_t eased = AnimMath::fixedEaseInOut(raw);
    const int16_t height = static_cast<int16_t>(display.height());
    const int16_t outOffset = static_cast<int16_t>(-height * eased / kEaseScale);
    const int16_t inOffset = static_cast<int16_t>(height - height * eased / kEaseScale);
    drawView(viewTransition_.from, display, static_cast<int16_t>(yOffset + outOffset), nowMs);
    drawView(viewTransition_.to, display, static_cast<int16_t>(yOffset + inOffset), nowMs);
  } else {
    drawView(view_, display, yOffset, nowMs);
  }

  PopupView::drawConfirm(display, yOffset, abandonConfirm_, "放弃本次专注？", "继续", "放弃",
                         nowMs);
  return true;
}

void FocusClockPage::drawView(View view, DisplayMonoTft& display, int16_t yOffset,
                              uint32_t nowMs) const {
  switch (view) {
    case View::Selection:
      drawSelection(display, yOffset, nowMs);
      break;
    case View::Focus:
    case View::Break:
      drawTimer(display, yOffset, nowMs);
      break;
    case View::Finished:
      drawFinished(display, yOffset);
      break;
  }
}

void FocusClockPage::drawSelection(DisplayMonoTft& display, int16_t yOffset,
                                   uint32_t nowMs) const {
  if (!cardAnimation_.active) {
    drawCardContent(display, cardIndex_, kCardX, static_cast<int16_t>(yOffset + kCardY),
                    optionPosition(nowMs));
    return;
  }

  const uint16_t raw =
      AnimMath::fixedClampProgress(nowMs - cardAnimation_.startMs, kCardAnimationMs);
  const uint16_t eased = AnimMath::fixedEaseInOut(raw);
  const uint8_t source = cardAnimation_.fromIndex;
  const uint8_t target = cardAnimation_.toIndex;
  const int16_t restingY = static_cast<int16_t>(yOffset + kCardY);
  const int16_t offscreenY = static_cast<int16_t>(yOffset + display.height() + 2);
  const int16_t travel = static_cast<int16_t>(offscreenY - restingY);
  const int16_t sourceOption = static_cast<int16_t>(optionIndexForCard(source) * kEaseScale);

  if (cardAnimation_.direction > 0) {
    const int16_t incomingY = static_cast<int16_t>(restingY -
        6 * (kEaseScale - eased) / kEaseScale);
    const int16_t outgoingY = static_cast<int16_t>(restingY + travel * eased / kEaseScale);
    drawCardContent(display, target, kCardX, incomingY, optionPosition(nowMs));
    // The outgoing card keeps its own title/options as it crosses the bottom edge.
    drawCardContent(display, source, kCardX, outgoingY, sourceOption);
  } else {
    const int16_t incomingY = static_cast<int16_t>(
        offscreenY - travel * AnimMath::fixedEaseOutCubic(raw) / kEaseScale);
    drawCardContent(display, source, kCardX, restingY, sourceOption);
    drawCardContent(display, target, kCardX, incomingY, optionPosition(nowMs));
  }
}

void FocusClockPage::drawTimer(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const bool breakPhase = view_ == View::Break;

  char headerLeft[48];
  if (cardIndex_ == kContinuousCardIndex) {
    snprintf(headerLeft, sizeof(headerLeft), "%s · %s %u/%u", sessionTitle(),
             breakPhase ? "休息" : "专注", static_cast<unsigned>(roundIndex_ + 1U),
             static_cast<unsigned>(kContinuousRounds));
  } else {
    snprintf(headerLeft, sizeof(headerLeft), "%s", sessionTitle());
  }

  char headerRight[16];
  if (timerPaused_) {
    snprintf(headerRight, sizeof(headerRight), "已暂停");
  } else if (breakPhase) {
    snprintf(headerRight, sizeof(headerRight), "休息 %u 分钟",
             static_cast<unsigned>(kBreakMinutes));
  } else {
    formatMinutesSeconds(phaseTargetMs(), headerRight, sizeof(headerRight));
  }
  drawHeaderBand(display, yOffset, headerLeft, headerRight);

  // 大字：当前阶段已用时间（正计时累计）。
  char elapsedText[16];
  formatMinutesSeconds(phaseElapsedMs(nowMs), elapsedText, sizeof(elapsedText));
  const int16_t digitsWidth = Segment7Font::measureText(elapsedText, kTimerDigitStyle);
  Segment7Font::drawText(canvas, static_cast<int16_t>((width - digitsWidth) / 2),
                         static_cast<int16_t>(yOffset + kTimerDigitTop), elapsedText,
                         kTimerDigitStyle);

  char infoText[64];
  if (timerPaused_) {
    snprintf(infoText, sizeof(infoText), "已暂停 · OK 继续");
  } else if (cardIndex_ == kContinuousCardIndex) {
    char sessionText[16];
    formatMinutesSeconds(sessionElapsedMs(nowMs), sessionText, sizeof(sessionText));
    snprintf(infoText, sizeof(infoText), "累计 %s · 第 %u/%u 次", sessionText,
             static_cast<unsigned>(roundIndex_ + 1U),
             static_cast<unsigned>(kContinuousRounds));
  } else {
    char targetText[16];
    formatMinutesSeconds(phaseTargetMs(), targetText, sizeof(targetText));
    snprintf(infoText, sizeof(infoText), "目标 %s", targetText);
  }
  text.setFont(chinese_font_all);
  drawTextCentered(text, infoText, static_cast<int16_t>(width / 2),
                   static_cast<int16_t>(yOffset + kTimerInfoBaseline), ST7305_COLOR_BLACK,
                   ST7305_COLOR_WHITE);

  // 阶段进度条：外框 + 已用部分。宽度保持整屏，避免出现"部分列 + 全行"的推送窗口。
  const int16_t barWidth = static_cast<int16_t>(width - kTimerBarX * 2);
  const int16_t barY = static_cast<int16_t>(yOffset + kTimerBarY);
  DrawUtils::drawRoundRect(canvas, kTimerBarX, barY, barWidth, kTimerBarHeight,
                           static_cast<int16_t>(kTimerBarHeight / 2), ST7305_COLOR_WHITE,
                           ST7305_COLOR_BLACK);
  const int16_t fillWidth = static_cast<int16_t>(static_cast<int32_t>(barWidth - 4) *
                                                 phaseProgressFixed(nowMs) / kEaseScale);
  if (fillWidth > 0) {
    DrawUtils::drawRoundRect(canvas, static_cast<int16_t>(kTimerBarX + 2),
                             static_cast<int16_t>(barY + 2), fillWidth,
                             static_cast<int16_t>(kTimerBarHeight - 4),
                             static_cast<int16_t>((kTimerBarHeight - 4) / 2),
                             ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
  }
}

void FocusClockPage::drawFinished(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  drawHeaderBand(display, yOffset, "专注完成", nullptr);

  char totalText[16];
  formatMinutesSeconds(finishedTotalMs_, totalText, sizeof(totalText));
  const int16_t digitsWidth = Segment7Font::measureText(totalText, kTimerDigitStyle);
  Segment7Font::drawText(canvas, static_cast<int16_t>((width - digitsWidth) / 2),
                         static_cast<int16_t>(yOffset + kTimerDigitTop), totalText,
                         kTimerDigitStyle);

  text.setFont(chinese_font_all);
  drawTextCentered(text, "本次专注已完成", static_cast<int16_t>(width / 2),
                   static_cast<int16_t>(yOffset + kTimerInfoBaseline), ST7305_COLOR_BLACK,
                   ST7305_COLOR_WHITE);
  drawTextCentered(text, "OK 再来一次 · Left 返回", static_cast<int16_t>(width / 2),
                   static_cast<int16_t>(yOffset + 158), ST7305_COLOR_BLACK,
                   ST7305_COLOR_WHITE);
}
