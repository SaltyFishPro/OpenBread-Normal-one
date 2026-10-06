#include "AnswersBookPage.h"

#include <cstdio>
#include <cstring>

#include <esp_system.h>

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../TextUtils.h"
#include "AnswersBookData.h"

namespace {
constexpr int16_t kCardX = 16;
constexpr int16_t kCardY = 40;
constexpr int16_t kCardWidth = 352;
constexpr int16_t kCardHeight = 88;
constexpr int16_t kCardRadius = 12;
constexpr int16_t kFooterBaselineOffset = 10;
constexpr int16_t kProgressX = 56;
constexpr int16_t kProgressY = 132;
constexpr int16_t kProgressWidth = 272;
constexpr int16_t kProgressHeight = 8;
constexpr int16_t kAnswerMaxWidth = 320;
constexpr int16_t kAnswerFirstBaseline = 94;
constexpr int16_t kAnswerLineGap = 18;
constexpr uint8_t kHistoryCapacity = 3;
constexpr int16_t kHistoryFirstBaseline = 49;
constexpr int16_t kHistoryLineGap = 24;

void drawCentered(U8G2_FOR_ST73XX& text, const char* value, int16_t centerX,
                 int16_t baseline) {
  text.drawUTF8(TextUtils::centeredTextX(text, value, centerX), baseline, value);
}

void drawWrappedCentered(U8G2_FOR_ST73XX& text, const char* value, int16_t centerX,
                        int16_t firstBaseline, int16_t maxWidth, int16_t lineGap) {
  const char* cursor = value;
  char line[64] = {0};
  uint8_t lineCount = 0;

  while (*cursor != '\0' && lineCount < 3U) {
    size_t lineBytes = 0;
    const char* probe = cursor;
    while (*probe != '\0') {
      const uint8_t charBytes = TextUtils::utf8CharLen(*probe);
      if (lineBytes + charBytes >= sizeof(line)) {
        break;
      }
      memcpy(line + lineBytes, probe, charBytes);
      line[lineBytes + charBytes] = '\0';
      if (text.getUTF8Width(line) > maxWidth && lineBytes > 0U) {
        break;
      }
      lineBytes += charBytes;
      probe += charBytes;
    }

    if (lineBytes == 0U) {
      const uint8_t charBytes = TextUtils::utf8CharLen(*cursor);
      if (charBytes >= sizeof(line)) {
        return;
      }
      memcpy(line, cursor, charBytes);
      line[charBytes] = '\0';
      lineBytes = charBytes;
    }

    drawCentered(text, line, centerX,
                 static_cast<int16_t>(firstBaseline + lineCount * lineGap));
    cursor += lineBytes;
    ++lineCount;
  }
}

void drawCardShell(ST7305_2p9_BW_DisplayDriver& canvas, int16_t yOffset) {
  DrawUtils::drawRoundRect(canvas, kCardX, static_cast<int16_t>(kCardY + yOffset),
                           kCardWidth, kCardHeight,
                           kCardRadius, ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
}

void drawFooter(U8G2_FOR_ST73XX& text, int16_t yOffset, const char* value,
                int16_t displayWidth, int16_t displayHeight) {
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  drawCentered(text, value, static_cast<int16_t>(displayWidth / 2),
               static_cast<int16_t>(yOffset + displayHeight - kFooterBaselineOffset));
}

void drawHistoryRow(ST7305_2p9_BW_DisplayDriver& canvas, U8G2_FOR_ST73XX& text,
                    const char* value, int16_t rowTop, int16_t centerX,
                    bool selected) {
  if (selected) {
    canvas.drawFilledRectangle(32, rowTop, 351,
                               static_cast<int16_t>(rowTop + 21),
                               ST7305_COLOR_BLACK);
  }
  text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextX(text, value, centerX),
                static_cast<int16_t>(rowTop + 16), value);
}
}  // namespace

bool AnswersBookPage::isSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == 0U;
}

void AnswersBookPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return;
  }
  view_ = View::Prompt;
  phaseStartMs_ = 0;
  answerIndex_ = 0;
  lastAnswerIndex_ = 0xFFFFU;
  historyCount_ = 0;
  historyCursor_ = 0;
  hasAnswer_ = false;
}

void AnswersBookPage::startThinking(uint32_t nowMs) {
  view_ = View::Thinking;
  phaseStartMs_ = nowMs;
}

void AnswersBookPage::chooseAnswer() {
  if (AnswersBookData::kAnswerCount == 0U) {
    hasAnswer_ = false;
    return;
  }

  uint16_t selected = 0;
  do {
    selected = static_cast<uint16_t>(esp_random() % AnswersBookData::kAnswerCount);
  } while (AnswersBookData::kAnswerCount > 1U && selected == lastAnswerIndex_);

  answerIndex_ = selected;
  lastAnswerIndex_ = selected;
  hasAnswer_ = true;
  recordAnswer();
}

void AnswersBookPage::recordAnswer() {
  if (historyCount_ < kHistoryCapacity) {
    for (uint8_t i = historyCount_; i > 0U; --i) {
      historyIndices_[i] = historyIndices_[i - 1U];
    }
    historyIndices_[0] = answerIndex_;
    ++historyCount_;
  } else {
    historyIndices_[2] = historyIndices_[1];
    historyIndices_[1] = historyIndices_[0];
    historyIndices_[0] = answerIndex_;
  }
  historyCursor_ = 0;
}

bool AnswersBookPage::update(uint32_t nowMs) {
  if (view_ == View::Thinking) {
    if (nowMs - phaseStartMs_ >= kThinkingDurationMs) {
      chooseAnswer();
      view_ = View::RevealAnimation;
      phaseStartMs_ = nowMs;
      return true;
    }
    return true;
  }

  if (view_ == View::RevealAnimation && nowMs - phaseStartMs_ >= kRevealAnimationMs) {
    view_ = View::Answer;
    return true;
  }
  return false;
}

bool AnswersBookPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus,
                                        bool upEdge, bool downEdge, bool okEdge,
                                        uint32_t nowMs) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (view_ == View::History) {
    if (historyCount_ == 0U) {
      view_ = hasAnswer_ ? View::Answer : View::Prompt;
      return true;
    }
    if (upEdge) {
      historyCursor_ = historyCursor_ == 0U
                           ? static_cast<uint8_t>(historyCount_ - 1U)
                           : static_cast<uint8_t>(historyCursor_ - 1U);
      return true;
    }
    if (downEdge) {
      historyCursor_ = static_cast<uint8_t>((historyCursor_ + 1U) % historyCount_);
      return true;
    }
    if (okEdge) {
      startThinking(nowMs);
      return true;
    }
    return false;
  }

  if ((view_ == View::Answer || view_ == View::Prompt) &&
      (upEdge || downEdge) && historyCount_ > 0U) {
    historyCursor_ = 0;
    view_ = View::History;
    return true;
  }

  if ((view_ == View::Prompt || view_ == View::Answer) && okEdge) {
    startThinking(nowMs);
    return true;
  }
  return false;
}

bool AnswersBookPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (view_ == View::History) {
    view_ = hasAnswer_ ? View::Answer : View::Prompt;
    return true;
  }
  view_ = View::Prompt;
  hasAnswer_ = false;
  return false;
}

bool AnswersBookPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isSelection(homeFocus, sectionFocus) &&
         (view_ == View::Thinking || view_ == View::RevealAnimation);
}

bool AnswersBookPage::isAnimating(uint8_t homeFocus, uint8_t sectionFocus) const {
  return needsAnimationFrame(homeFocus, sectionFocus);
}

bool AnswersBookPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                                   DisplayMonoTft& display, uint32_t nowMs) const {
  if (!isSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  DetailHeader::render(display, view_ == View::History ? "答案记录" : "答案之书", yOffset);
  text.setFont(chinese_font_all);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);

  drawCardShell(canvas, yOffset);

  if (view_ == View::Prompt) {
    drawCentered(text, "想好一个问题", static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 76));
    drawCentered(text, "在心里默念，然后按 OK", static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 101));
    drawFooter(text, yOffset, "OK 开始    LEFT 返回", width, height);
    return true;
  }

  if (view_ == View::Thinking) {
    const uint32_t elapsed = nowMs - phaseStartMs_;
    const uint32_t remaining = elapsed >= kThinkingDurationMs
                                   ? 0U
                                   : (kThinkingDurationMs - elapsed + 999U) / 1000U;
    char seconds[8];
    snprintf(seconds, sizeof(seconds), "%u", static_cast<unsigned>(remaining));
    drawCentered(text, "请专注于你的问题", static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 70));
    drawCentered(text, seconds, static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 105));

    const uint32_t progress = elapsed >= kThinkingDurationMs
                                   ? 0U
                                   : (elapsed * static_cast<uint32_t>(kProgressWidth - 2)) /
                                         kThinkingDurationMs;
    canvas.drawRectangle(kProgressX, static_cast<int16_t>(yOffset + kProgressY),
                         static_cast<int16_t>(kProgressX + kProgressWidth),
                         static_cast<int16_t>(yOffset + kProgressY + kProgressHeight),
                         ST7305_COLOR_BLACK);
    if (progress > 0U) {
      canvas.drawFilledRectangle(static_cast<int16_t>(kProgressX + 1),
                                 static_cast<int16_t>(yOffset + kProgressY + 1),
                                 static_cast<int16_t>(kProgressX + progress),
                                 static_cast<int16_t>(yOffset + kProgressY + kProgressHeight - 1),
                                 ST7305_COLOR_BLACK);
    }
    drawFooter(text, yOffset, "LEFT 取消", width, height);
    return true;
  }

  if (view_ == View::RevealAnimation) {
    drawCentered(text, "答案正在翻开", static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 88));
    drawFooter(text, yOffset, "请稍候", width, height);
    return true;
  }

  if (view_ == View::History) {
    for (uint8_t row = 0; row < historyCount_; ++row) {
      const uint16_t index = historyIndices_[row];
      if (index >= AnswersBookData::kAnswerCount) {
        continue;
      }
      drawHistoryRow(canvas, text, AnswersBookData::kAnswers[index],
                     static_cast<int16_t>(yOffset + kHistoryFirstBaseline +
                                          row * kHistoryLineGap),
                     static_cast<int16_t>(width / 2), row == historyCursor_);
    }
    drawFooter(text, yOffset, "UP/DOWN 浏览   OK 再问   LEFT 返回", width, height);
    return true;
  }

  if (hasAnswer_ && answerIndex_ < AnswersBookData::kAnswerCount) {
    drawCentered(text, "答案", static_cast<int16_t>(width / 2),
                 static_cast<int16_t>(yOffset + 66));
    drawWrappedCentered(text, AnswersBookData::kAnswers[answerIndex_],
                        static_cast<int16_t>(width / 2),
                        static_cast<int16_t>(yOffset + kAnswerFirstBaseline),
                        kAnswerMaxWidth, kAnswerLineGap);
  }
  drawFooter(text, yOffset, "OK 再问一次    LEFT 返回", width, height);
  return true;
}
