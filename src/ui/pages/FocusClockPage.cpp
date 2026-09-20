#include "FocusClockPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"

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

const char* const kContinuousOptions[] = {"10分钟", "15分钟", "20分钟"};
const char* const kShortOptions[] = {"10分钟", "15分钟", "20分钟"};
const char* const kLongOptions[] = {"30分钟", "60分钟"};

const FocusClockPage::Card kCards[] = {
    {"持续任务", kContinuousOptions, 3, 1},
    {"短专注", kShortOptions, 3, 1},
    {"长专注", kLongOptions, 2, 0},
};

void drawTextCentered(U8G2_FOR_ST73XX& text, const char* value, int16_t centerX,
                      int16_t baseline, uint16_t foreground, uint16_t background) {
  text.setBackgroundColor(background);
  text.setForegroundColor(foreground);
  text.setFontMode(1);
  const int16_t width = text.getUTF8Width(value);
  text.drawUTF8(static_cast<int16_t>(centerX - width / 2), baseline, value);
}

void drawCardShell(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y) {
  // The previously drawn white shadow was invisible against the white page and
  // doubled the scanline cost of every card on every animation frame.
  DrawUtils::drawRoundRect(canvas, x, y, kCardWidth, kCardHeight, kCardRadius,
                           ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
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
  return changed;
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
  return okEdge;
}

bool FocusClockPage::isAnimating(uint8_t homeFocus, uint8_t sectionFocus,
                                 uint32_t nowMs) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (cardAnimation_.active && nowMs - cardAnimation_.startMs < kCardAnimationMs) {
    return true;
  }
  return optionAnimationActive_ && nowMs - optionAnimationStartMs_ < kOptionAnimationMs;
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

  if (!cardAnimation_.active) {
    drawCardContent(display, cardIndex_, kCardX, static_cast<int16_t>(yOffset + kCardY),
                    optionPosition(nowMs));
    return true;
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
  return true;
}
