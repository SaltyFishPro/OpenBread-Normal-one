#include "ReaderPage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../DrawUtils.h"
#include "../TextUtils.h"
#include "../SdBitmapText.h"
#include "../../services/SdCardService.h"
#include "../../services/SdBitmapFont.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr int16_t kCardSize = 47;
constexpr int16_t kCardRadius = 6;
constexpr int16_t kFirstRowY = 35;
constexpr int16_t kSecondRowY = 103;
constexpr int16_t kFirstColX = 38;
constexpr int16_t kColStep = 87;
constexpr int16_t kTitleBarHeight = 28;
constexpr int16_t kTitleBaselineY = 22;
constexpr int16_t kCardTextHeight = 14;

constexpr int16_t kWordTextX = 3;
constexpr int16_t kWordBaselineY = 28;
constexpr int16_t kPhoneticBaselineY = 27;
constexpr int16_t kPhoneticGap = 6;
constexpr int16_t kDividerY = 36;
constexpr int16_t kDividerHeight = 2;
constexpr int16_t kMeaningX = 3;
constexpr int16_t kMeaningTop = 42;
constexpr int16_t kMeaningBottom = 142;
constexpr int16_t kMeaningRightX = 350;
constexpr int16_t kMeaningWidth = kMeaningRightX - kMeaningX + 1;
constexpr int16_t kPageArrowCenterX = 368;
constexpr int16_t kPageUpArrowCenterY = 70;
constexpr int16_t kPageDownArrowCenterY = 125;
constexpr int16_t kPageArrowHalfWidth = 5;
constexpr int16_t kPageArrowHalfHeight = 5;
constexpr int16_t kScrollX = 356;
constexpr int16_t kScrollWidth = 23;
constexpr int16_t kFooterY = 147;
constexpr int16_t kFooterHeight = 18;

const char* const kLabelsZh[] = {
    "初中",
    "高中",
    "四级",
    "六级",
    "考研",
    "托福",
    "SAT",
    "我的",
};

int16_t cardX(uint8_t index) {
  return static_cast<int16_t>(kFirstColX + (index % 4U) * kColStep);
}

int16_t cardY(uint8_t index) {
  return index < 4U ? kFirstRowY : kSecondRowY;
}

void drawCenteredText(DisplayMonoTft& display, const char* textValue, int16_t x, int16_t y,
                      int16_t width, int16_t height, bool selected) {
  auto& text = display.text();
  text.setFont(chinese_font_all);
  text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setFontMode(selected ? 0 : 1);

  const int16_t boxCenterX = static_cast<int16_t>(x + width / 2);
  int16_t textX = TextUtils::centeredTextX(text, textValue, boxCenterX);
  if (textX < x + 2) {
    textX = static_cast<int16_t>(x + 2);
  }
  const int16_t baseline =
      static_cast<int16_t>(y + ((height - kCardTextHeight) / 2) + kCardTextHeight);
  text.drawUTF8(textX, baseline, textValue);

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

void drawClippedTextLine(DisplayMonoTft& display, const char* value, int16_t x, int16_t baseline,
                         int16_t maxWidth, bool inverted) {
  if (value == nullptr || value[0] == '\0') {
    return;
  }

  auto& text = display.text();
  text.setFont(chinese_font_all);
  text.setForegroundColor(inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(inverted ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setFontMode(inverted ? 0 : 1);

  char line[ReaderService::kMaxMeaningLen];
  uint16_t out = 0;
  for (uint16_t i = 0; value[i] != '\0' && out < sizeof(line) - 1;) {
    const uint8_t charLen = TextUtils::utf8CharLen(value[i]);
    if (out + charLen >= sizeof(line)) {
      break;
    }
    for (uint8_t j = 0; j < charLen && value[i + j] != '\0'; ++j) {
      line[out++] = value[i + j];
    }
    line[out] = '\0';
    if (text.getUTF8Width(line) > maxWidth) {
      out = static_cast<uint16_t>(out - charLen);
      line[out] = '\0';
      break;
    }
    i = static_cast<uint16_t>(i + charLen);
  }

  text.drawUTF8(x, baseline, line);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

int16_t drawTextLineFast(DisplayMonoTft& display, const uint8_t* font, const char* value,
                         int16_t x, int16_t baseline, int16_t maxWidth, bool inverted) {
  if (value == nullptr || value[0] == '\0' || maxWidth <= 0) {
    return 0;
  }

  auto& text = display.text();
  text.setFont(font);
  text.setForegroundColor(inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(inverted ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setFontMode(inverted ? 0 : 1);

  const int16_t width = text.getUTF8Width(value);
  text.drawUTF8(x, baseline, value);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  return width > maxWidth ? maxWidth : width;
}

bool startsWith(const char* value, const char* token) {
  for (uint8_t i = 0; token[i] != '\0'; ++i) {
    if (value[i] != token[i]) {
      return false;
    }
  }
  return true;
}

uint8_t partOfSpeechLen(const char* value) {
  static const char* const kPartOfSpeechTokens[] = {
      "abbr.", "interj.", "prep.", "pron.", "conj.", "adj.", "adv.", "num.",
      "aux.",  "det.",    "art.",  "vt.",   "vi.",   "int.", "n.",   "v.",
      "a.",
  };

  for (const char* token : kPartOfSpeechTokens) {
    if (startsWith(value, token)) {
      const uint8_t len = static_cast<uint8_t>(strlen(token));
      const char next = value[len];
      if (next == '\0' || next == ' ' || next == '\t') {
        return len;
      }
    }
  }

  return 0;
}

const char* stateMessage(ReaderService::State state) {
  switch (state) {
    case ReaderService::State::SdMissing:
      return "SD卡未就绪";
    case ReaderService::State::DirMissing:
      return "缺少 /words 文件夹";
    case ReaderService::State::FileMissing:
      return "没有找到对应词库";
    case ReaderService::State::Empty:
      return "词库为空";
    case ReaderService::State::Error:
      return "读取失败";
    case ReaderService::State::Idle:
      return "未打开词库";
    case ReaderService::State::Ready:
    default:
      return "";
  }
}

void drawPageArrow(ST7305_2p9_BW_DisplayDriver& canvas, int16_t centerY,
                    bool down, bool active) {
  const int16_t tipY = centerY + (down ? kPageArrowHalfHeight : -kPageArrowHalfHeight);
  const int16_t baseY = centerY + (down ? -kPageArrowHalfHeight : kPageArrowHalfHeight);
  if (!active) {
    canvas.drawTriangle(kPageArrowCenterX, tipY, kPageArrowCenterX - kPageArrowHalfWidth,
                        baseY, kPageArrowCenterX + kPageArrowHalfWidth, baseY, ST7305_COLOR_BLACK);
    return;
  }
  // Logical scanlines avoid the polygon driver's native/rotated bounds and heap allocation.
  for (int16_t row = 0; row <= 2 * kPageArrowHalfHeight; ++row) {
    const int16_t halfWidth = row * kPageArrowHalfWidth / (2 * kPageArrowHalfHeight);
    const int16_t y = tipY + (down ? -row : row);
    canvas.drawFastHLine(kPageArrowCenterX - halfWidth, y, 2 * halfWidth + 1, ST7305_COLOR_BLACK);
  }
}

void drawWordDetail(DisplayMonoTft& display, const ReaderService& reader) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const ReaderService::Entry& entry = reader.entry();

  const int16_t pageBottom = static_cast<int16_t>(display.height() - 1);
  const int16_t scrollRight = static_cast<int16_t>(kScrollX + kScrollWidth - 1);

  const char* title = entry.word[0] != '\0' ? entry.word : stateMessage(reader.state());
  const int16_t titleWidth = drawTextLineFast(
      display, u8g2_font_fur25_tf, title, kWordTextX,
      kWordBaselineY, static_cast<int16_t>(width - kWordTextX - 1), false);

  if (entry.phonetic[0] != '\0' && reader.state() == ReaderService::State::Ready) {
    int16_t phoneticX = static_cast<int16_t>(kWordTextX + titleWidth + kPhoneticGap);
    const int16_t maxPhoneticX = static_cast<int16_t>(width - 24);
    if (phoneticX > maxPhoneticX) {
      phoneticX = maxPhoneticX;
    }
    const int16_t phoneticWidth = static_cast<int16_t>(width - phoneticX - 4);
    (void)drawTextLineFast(display, chinese_font_all, entry.phonetic, phoneticX,
                           kPhoneticBaselineY, phoneticWidth, false);
  }

  canvas.drawFilledRectangle(0, kDividerY, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(kDividerY + kDividerHeight - 1),
                             ST7305_COLOR_BLACK);

  if (reader.state() != ReaderService::State::Ready) {
    const char* message = stateMessage(reader.state());
    int16_t messageX = TextUtils::centeredTextXInBox(text, message, kMeaningX, kMeaningWidth);
    if (messageX < kMeaningX + 4) {
      messageX = static_cast<int16_t>(kMeaningX + 4);
    }
    text.drawUTF8(messageX, 101, message);
    if (reader.errorText()[0] != '\0') {
      drawClippedTextLine(display, reader.errorText(), static_cast<int16_t>(kMeaningX + 6),
                          124, static_cast<int16_t>(kMeaningWidth - 12), false);
    }
  }

  canvas.drawFilledRectangle(0, kFooterY, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(kFooterY + kFooterHeight - 1),
                             ST7305_COLOR_BLACK);
  canvas.drawFilledRectangle(kScrollX, kFooterY, scrollRight,
                             static_cast<int16_t>(kFooterY + kFooterHeight - 1),
                             ST7305_COLOR_WHITE);
  char footer[96];
  const char* book = reader.bookLabelZh();
  if (reader.state() == ReaderService::State::Ready) {
    std::snprintf(footer, sizeof(footer), "%s  #%lu", book,
                  static_cast<unsigned long>(reader.currentIndex() + 1U));
  } else {
    std::snprintf(footer, sizeof(footer), "%s", book);
  }
  drawClippedTextLine(display, footer, 8, 164, static_cast<int16_t>(kScrollX - 14), true);

  canvas.drawFilledRectangle(0, pageBottom, static_cast<int16_t>(width - 1), pageBottom,
                             ST7305_COLOR_BLACK);
}
}  // namespace

bool ReaderPage::isVocabularySelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kVocabularyItemIndex;
}

bool ReaderPage::isStaticWordDetail(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isVocabularySelection(homeFocus, sectionFocus) && detailView_ == DetailView::Word;
}

void ReaderPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus, SdBitmapFont& font) {
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return;
  }

  font.end();
  detailView_ = DetailView::Library;
  selectedIndex_ = 0;
  resetMeaningPage();
}

bool ReaderPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                                   bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                                   ReaderService& reader, SdCardService& sd, SdBitmapFont& font) {
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (detailView_ == DetailView::Word) {
    if (leftEdge) {
      if (reader.move(-1)) {
        resetMeaningPage();
        return true;
      }
      return false;
    }
    if (rightEdge) {
      if (reader.move(1)) {
        resetMeaningPage();
        return true;
      }
      return false;
    }
    if (upEdge) {
      return moveMeaningPage(-1);
    }
    if (downEdge) {
      return moveMeaningPage(1);
    }
    if (okEdge && reader.state() == ReaderService::State::Ready &&
        (font.state() == SdBitmapFont::State::Error || font.state() == SdBitmapFont::State::Closed ||
         meaningState_ == MeaningState::Unsupported)) {
      font.end();  // A retry may remount the card: close the old font first.
      if (sd.refresh() == SdCardService::Status::Ready) (void)font.open(14);
      resetMeaningPage();
    }
    return okEdge;
  }

  if (leftEdge) {
    moveSelection(-1);
    return true;
  }
  if (rightEdge) {
    moveSelection(1);
    return true;
  }
  if (okEdge) {
    font.end();
    if (reader.openBook(selectedIndex_, sd)) (void)font.open(14);
    detailView_ = DetailView::Word;
    resetMeaningPage();
    return true;
  }

  return upEdge || downEdge;
}

bool ReaderPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                  ReaderService& reader, SdBitmapFont& font) {
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return false;
  }

  font.end();
  reader.reset();
  resetMeaningPage();
  if (detailView_ == DetailView::Word) {
    detailView_ = DetailView::Library;
    return true;
  }

  selectedIndex_ = 0;
  return false;
}

void ReaderPage::moveSelection(int8_t delta) {
  if (delta < 0) {
    selectedIndex_ =
        selectedIndex_ == 0 ? static_cast<uint8_t>(ReaderService::kBookCount - 1)
                            : static_cast<uint8_t>(selectedIndex_ - 1);
  } else if (delta > 0) {
    selectedIndex_ = static_cast<uint8_t>((selectedIndex_ + 1U) % ReaderService::kBookCount);
  }
}

void ReaderPage::resetMeaningPage() {
  meaningPage_ = 0;
  maxMeaningPage_ = 0;
  meaningState_ = MeaningState::Pending;
}

bool ReaderPage::moveMeaningPage(int8_t delta) {
  if (meaningState_ != MeaningState::Ready) return false;
  if (delta < 0) {
    if (meaningPage_ == 0) {
      return false;
    }
    --meaningPage_;
    meaningState_ = MeaningState::Pending;
    return true;
  }

  if (delta > 0) {
    if (meaningPage_ >= maxMeaningPage_) {
      return false;
    }
    ++meaningPage_;
    meaningState_ = MeaningState::Pending;
    return true;
  }

  return false;
}

void ReaderPage::finishMeaningLine(uint16_t end) {
  const uint16_t page = meaningLineCount_ / meaningLinesPerPage_;
  if (page == meaningPage_) {
    auto& line = meaningLines_[meaningLineCount_ % meaningLinesPerPage_];
    line.start = meaningLineStart_;
    line.length = end - meaningLineStart_;
  }
  ++meaningLineCount_;
  meaningLineStart_ = end;
  meaningLineWidth_ = 0;
}

bool ReaderPage::updateMeaning(const ReaderService& reader, SdBitmapFont& font) {
  if (detailView_ != DetailView::Word || reader.state() != ReaderService::State::Ready) return false;
  if (font.state() != SdBitmapFont::State::Ready) {
    const bool wasBuilding = meaningState_ == MeaningState::Building;
    meaningState_ = MeaningState::Pending;
    return wasBuilding;
  }
  if (meaningState_ == MeaningState::Ready || meaningState_ == MeaningState::Unsupported) return false;
  if (meaningState_ == MeaningState::Pending) {
    const uint32_t height = font.metrics().lineHeight();
    if (height == 0 || height > static_cast<uint32_t>(kMeaningBottom - kMeaningTop + 1)) {
      meaningState_ = MeaningState::Unsupported;
      maxMeaningPage_ = 0;
      return true;
    }
    meaningLineHeight_ = static_cast<uint16_t>(height);
    const uint16_t count = (kMeaningBottom - kMeaningTop + 1) / height;
    meaningLinesPerPage_ = count < kMaxMeaningLines ? count : kMaxMeaningLines;
    for (auto& line : meaningLines_) line = MeaningLine{};
    meaningScanOffset_ = meaningLineStart_ = meaningLineCount_ = 0;
    meaningLineWidth_ = 0;
    meaningState_ = MeaningState::Building;
  }

  // Bound uncached glyph lookups so input and other services keep running.
  const uint32_t started = millis();
  const char* value = reader.entry().meaning;
  for (uint8_t work = 0; work < 16U; ++work) {
    const char* current = value + meaningScanOffset_;
    if (*current == '\0') {
      if (meaningScanOffset_ > meaningLineStart_) finishMeaningLine(meaningScanOffset_);
      maxMeaningPage_ = meaningLineCount_ ? (meaningLineCount_ - 1U) / meaningLinesPerPage_ : 0;
      meaningState_ = MeaningState::Ready;
      return true;
    }
    if (*current == '\r' || *current == '\n') {
      finishMeaningLine(meaningScanOffset_);
      meaningScanOffset_ += current[0] == '\r' && current[1] == '\n' ? 2U : 1U;
      meaningLineStart_ = meaningScanOffset_;
    } else {
      if (meaningScanOffset_ > meaningLineStart_ && partOfSpeechLen(current))
        finishMeaningLine(meaningScanOffset_);
      const char* next = current;
      uint32_t codepoint;
      (void)SdBitmapText::nextCodepoint(next, codepoint);
      SdBitmapFont::Glyph glyph;
      const bool found = font.glyph(codepoint, glyph);
      if (font.state() != SdBitmapFont::State::Ready) {
        meaningState_ = MeaningState::Pending;
        return true;
      }
      const int32_t advance = found ? glyph.advance : font.metrics().pixelSize;
      if (meaningScanOffset_ > meaningLineStart_ && meaningLineWidth_ + advance > kMeaningWidth)
        finishMeaningLine(meaningScanOffset_);
      meaningLineWidth_ += advance;
      meaningScanOffset_ = static_cast<uint16_t>(next - value);
    }
    if (millis() - started >= 4U) break;
  }
  return false;
}

void ReaderPage::drawMeaning(DisplayMonoTft& display, const ReaderService& reader,
                              SdBitmapFont& font) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const SdBitmapText::Clip clip = {kMeaningX, kMeaningTop, kMeaningRightX, kMeaningBottom};
  if (font.state() == SdBitmapFont::State::Ready && meaningState_ == MeaningState::Ready) {
    if (reader.entry().meaning[0] == '\0') {
      text.drawUTF8(kMeaningX + 4, 84, "暂无释义");
      return;
    }
    char line[ReaderService::kMaxMeaningLen];
    for (uint8_t i = 0; i < meaningLinesPerPage_; ++i) {
      const auto& span = meaningLines_[i];
      std::memcpy(line, reader.entry().meaning + span.start, span.length);
      line[span.length] = '\0';
      const int16_t baseline = kMeaningTop + font.metrics().ascent + i * meaningLineHeight_;
      if (!SdBitmapText::drawUTF8(canvas, font, kMeaningX, baseline, line, ST7305_COLOR_BLACK, clip))
        break;
    }
    if (font.state() == SdBitmapFont::State::Ready) return;
    // A read failure during drawing replaces partial ink with an actionable message.
    canvas.drawFilledRectangle(clip.x1, clip.y1, clip.x2, clip.y2, ST7305_COLOR_WHITE);
  }
  const char* status = "字库不可用，按 OK 重试";
  const char* hint = "请检查 Font/font-14.bin";
  if (font.state() == SdBitmapFont::State::Checking) {
    status = "正在检查字库，请稍候";
    hint = "长按 Left 可返回";
  } else if (font.state() == SdBitmapFont::State::Ready) {
    status = meaningState_ == MeaningState::Unsupported ? "字库行高不适合本页" : "正在排版释义，请稍候";
    hint = meaningState_ == MeaningState::Unsupported ? "重导出 14 字号字库后按 OK" : "长按 Left 可返回";
  }
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.drawUTF8(kMeaningX + 8, 84, status);
  text.drawUTF8(kMeaningX + 8, 112, hint);
}

bool ReaderPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                              DisplayMonoTft& display, const ReaderService& reader, SdBitmapFont& font) const {
  (void)yOffset;
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (detailView_ == DetailView::Word) {
    drawWordDetail(display, reader);
    if (reader.state() == ReaderService::State::Ready) drawMeaning(display, reader, font);
    const bool paged = font.state() == SdBitmapFont::State::Ready &&
                       meaningState_ == MeaningState::Ready;
    drawPageArrow(display.canvas(), kPageUpArrowCenterY, false, paged && meaningPage_ > 0);
    drawPageArrow(display.canvas(), kPageDownArrowCenterY, true, paged && meaningPage_ < maxMeaningPage_);
    return true;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const char* title = "单词库";
  const char* const* labels = kLabelsZh;

  canvas.drawFilledRectangle(0, 0, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(kTitleBarHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, title, 0, width), kTitleBaselineY, title);

  for (uint8_t i = 0; i < 8; ++i) {
    const bool selected = i == selectedIndex_;
    const int16_t x = cardX(i);
    const int16_t y = cardY(i);
    if (selected) {
      DrawUtils::fillRoundRect(canvas, x, y, static_cast<int16_t>(x + kCardSize - 1),
                               static_cast<int16_t>(y + kCardSize - 1), kCardRadius,
                               ST7305_COLOR_BLACK);
    } else {
      DrawUtils::strokeRoundRectOnWhite(canvas, x, y, kCardSize, kCardSize, kCardRadius,
                                        ST7305_COLOR_BLACK);
    }

    drawCenteredText(display, labels[i], x, y, kCardSize, kCardSize, selected);
  }

  return true;
}
