#include "ReaderPage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../../services/SdCardService.h"

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
constexpr int16_t kMeaningY = 58;
constexpr int16_t kMeaningRightX = 367;
constexpr int16_t kMeaningWidth = kMeaningRightX - kMeaningX + 1;
constexpr uint8_t kMeaningLinesPerPage = 5;
constexpr int16_t kMeaningLineHeight = 17;
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

const char* const kLabelsEn[] = {
    "Junior",
    "Senior",
    "CET4",
    "CET6",
    "NPEE",
    "TOEFL",
    "SAT",
    "Mine",
};

void drawFilledRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, int16_t radius, uint16_t color) {
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

  const int16_t w = static_cast<int16_t>(x2 - x1 + 1);
  const int16_t h = static_cast<int16_t>(y2 - y1 + 1);
  if (w <= 0 || h <= 0) {
    return;
  }

  int16_t r = radius;
  if (r < 0) {
    r = 0;
  }
  if (r > w / 2) {
    r = static_cast<int16_t>(w / 2);
  }
  if (r > h / 2) {
    r = static_cast<int16_t>(h / 2);
  }

  if (r == 0) {
    canvas.drawFilledRectangle(x1, y1, x2, y2, color);
    return;
  }

  canvas.drawFilledRectangle(static_cast<uint>(x1 + r), static_cast<uint>(y1),
                             static_cast<uint>(x2 - r), static_cast<uint>(y2), color);
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x1 + r - 1), static_cast<uint>(y2 - r),
                             color);
  canvas.drawFilledRectangle(static_cast<uint>(x2 - r + 1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x2), static_cast<uint>(y2 - r), color);

  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
}

void drawRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y, int16_t w,
                   int16_t h, int16_t radius, uint16_t color) {
  drawFilledRoundRect(canvas, x, y, static_cast<int16_t>(x + w - 1),
                      static_cast<int16_t>(y + h - 1), radius, color);
  if (w <= 2 || h <= 2) {
    return;
  }
  drawFilledRoundRect(canvas, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                      static_cast<int16_t>(x + w - 2), static_cast<int16_t>(y + h - 2),
                      static_cast<int16_t>(radius - 1), ST7305_COLOR_WHITE);
}

int16_t cardX(uint8_t index) {
  return static_cast<int16_t>(kFirstColX + (index % 4U) * kColStep);
}

int16_t cardY(uint8_t index) {
  return index < 4U ? kFirstRowY : kSecondRowY;
}

uint8_t utf8CharLen(char c) {
  const uint8_t b = static_cast<uint8_t>(c);
  if ((b & 0x80U) == 0) {
    return 1;
  }
  if ((b & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((b & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((b & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

void drawCenteredText(DisplayMonoTft& display, const char* textValue, int16_t x, int16_t y,
                      int16_t width, int16_t height, bool selected) {
  auto& text = display.text();
  text.setFont(chinese_font_all);
  text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setBackgroundColor(selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setFontMode(selected ? 0 : 1);

  const int16_t textWidth = text.getUTF8Width(textValue);
  int16_t textX = static_cast<int16_t>(x + (width - textWidth) / 2);
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
    const uint8_t charLen = utf8CharLen(value[i]);
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

uint8_t drawWrappedMeaningPage(DisplayMonoTft& display, const char* value, int16_t x,
                               int16_t y, int16_t width, uint8_t pageIndex) {
  auto& text = display.text();
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (value == nullptr || value[0] == '\0') {
    const char* empty = "暂无释义";
    text.drawUTF8(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 22), empty);
    return 0;
  }

  const int16_t maxWidth = width;
  char line[96];
  uint16_t lineLen = 0;
  int16_t lineWidth = 0;
  uint16_t lineIndex = 0;
  uint8_t maxPage = 0;

  auto flushLine = [&]() {
    if (line[0] == '\0') {
      return;
    }

    const uint8_t linePage = static_cast<uint8_t>(lineIndex / kMeaningLinesPerPage);
    if (linePage == pageIndex) {
      const uint8_t lineInPage = static_cast<uint8_t>(lineIndex % kMeaningLinesPerPage);
      const int16_t baseline =
          static_cast<int16_t>(y + static_cast<int16_t>(lineInPage) * kMeaningLineHeight);
      text.drawUTF8(x, baseline, line);
    }
    if (linePage > maxPage) {
      maxPage = linePage;
    }
    ++lineIndex;
  };

  for (uint16_t i = 0;;) {
    const char c = value[i];
    const bool hardBreak = c == '\n' || c == '\r';
    const bool atEnd = c == '\0';
    if (!atEnd && !hardBreak && lineLen > 0 && partOfSpeechLen(&value[i]) > 0) {
      flushLine();
      line[0] = '\0';
      lineLen = 0;
      lineWidth = 0;
    }

    const uint8_t charLen = atEnd ? 0 : utf8CharLen(c);
    const bool canAppend =
        !atEnd && !hardBreak && lineLen + charLen < sizeof(line);
    char glyph[5] = {0};
    int16_t glyphWidth = 0;
    if (canAppend) {
      for (uint8_t j = 0; j < charLen && value[i + j] != '\0'; ++j) {
        glyph[j] = value[i + j];
      }
      glyphWidth = text.getUTF8Width(glyph);
    }
    if (canAppend) {
      for (uint8_t j = 0; j < charLen && value[i + j] != '\0'; ++j) {
        line[lineLen++] = value[i + j];
      }
      line[lineLen] = '\0';
      lineWidth = static_cast<int16_t>(lineWidth + glyphWidth);
    }

    if (atEnd || hardBreak || lineWidth > maxWidth ||
        lineLen >= sizeof(line) - 1) {
      if (lineWidth > maxWidth && lineLen > charLen) {
        uint8_t overflowLen = charLen;
        if (overflowLen == 0 || overflowLen >= lineLen) {
          overflowLen = 1;
        }
        char overflow[5] = {0};
        for (uint8_t j = 0; j < overflowLen; ++j) {
          overflow[j] = line[lineLen - overflowLen + j];
        }
        lineLen = static_cast<uint16_t>(lineLen - overflowLen);
        line[lineLen] = '\0';
        lineWidth = static_cast<int16_t>(lineWidth - glyphWidth);
        flushLine();
        for (uint8_t j = 0; j < overflowLen; ++j) {
          line[j] = overflow[j];
        }
        line[overflowLen] = '\0';
        lineLen = overflowLen;
        lineWidth = glyphWidth;
      } else {
        flushLine();
        line[0] = '\0';
        lineLen = 0;
        lineWidth = 0;
      }
    }

    if (atEnd) {
      break;
    }
    if (hardBreak) {
      ++i;
    } else if (canAppend) {
      i = static_cast<uint16_t>(i + charLen);
    } else {
      ++i;
    }
  }

  return maxPage;
}

const char* stateMessage(ReaderService::State state, HomePage::Language language) {
  const bool zh = language == HomePage::Language::Zh;
  switch (state) {
    case ReaderService::State::SdMissing:
      return zh ? "SD卡未就绪" : "SD Not Ready";
    case ReaderService::State::DirMissing:
      return zh ? "缺少 /words 文件夹" : "Missing /words";
    case ReaderService::State::FileMissing:
      return zh ? "没有找到对应词库" : "Book Not Found";
    case ReaderService::State::Empty:
      return zh ? "词库为空" : "Empty Book";
    case ReaderService::State::Error:
      return zh ? "读取失败" : "Read Failed";
    case ReaderService::State::Idle:
      return zh ? "未打开词库" : "No Book";
    case ReaderService::State::Ready:
    default:
      return "";
  }
}

void drawMeaningPageHint(ST7305_2p9_BW_DisplayDriver& canvas, uint8_t pageIndex,
                         uint8_t maxPage) {
  if (pageIndex > 0) {
    canvas.drawFilledTriangle(static_cast<int>(kPageArrowCenterX),
                              static_cast<int>(kPageUpArrowCenterY - kPageArrowHalfHeight),
                              static_cast<int>(kPageArrowCenterX - kPageArrowHalfWidth),
                              static_cast<int>(kPageUpArrowCenterY + kPageArrowHalfHeight),
                              static_cast<int>(kPageArrowCenterX + kPageArrowHalfWidth),
                              static_cast<int>(kPageUpArrowCenterY + kPageArrowHalfHeight),
                              ST7305_COLOR_BLACK);
  } else {
    canvas.drawTriangle(static_cast<uint>(kPageArrowCenterX),
                        static_cast<uint>(kPageUpArrowCenterY - kPageArrowHalfHeight),
                        static_cast<uint>(kPageArrowCenterX - kPageArrowHalfWidth),
                        static_cast<uint>(kPageUpArrowCenterY + kPageArrowHalfHeight),
                        static_cast<uint>(kPageArrowCenterX + kPageArrowHalfWidth),
                        static_cast<uint>(kPageUpArrowCenterY + kPageArrowHalfHeight),
                        ST7305_COLOR_BLACK);
  }

  if (pageIndex < maxPage) {
    canvas.drawFilledTriangle(static_cast<int>(kPageArrowCenterX),
                              static_cast<int>(kPageDownArrowCenterY + kPageArrowHalfHeight),
                              static_cast<int>(kPageArrowCenterX - kPageArrowHalfWidth),
                              static_cast<int>(kPageDownArrowCenterY - kPageArrowHalfHeight),
                              static_cast<int>(kPageArrowCenterX + kPageArrowHalfWidth),
                              static_cast<int>(kPageDownArrowCenterY - kPageArrowHalfHeight),
                              ST7305_COLOR_BLACK);
  } else {
    canvas.drawTriangle(static_cast<uint>(kPageArrowCenterX),
                        static_cast<uint>(kPageDownArrowCenterY + kPageArrowHalfHeight),
                        static_cast<uint>(kPageArrowCenterX - kPageArrowHalfWidth),
                        static_cast<uint>(kPageDownArrowCenterY - kPageArrowHalfHeight),
                        static_cast<uint>(kPageArrowCenterX + kPageArrowHalfWidth),
                        static_cast<uint>(kPageDownArrowCenterY - kPageArrowHalfHeight),
                        ST7305_COLOR_BLACK);
  }
}

void drawWordDetail(DisplayMonoTft& display, HomePage::Language language,
                    const ReaderService& reader, uint8_t meaningPage,
                    uint8_t& maxMeaningPage) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const ReaderService::Entry& entry = reader.entry();

  const int16_t pageBottom = static_cast<int16_t>(display.height() - 1);
  const int16_t scrollRight = static_cast<int16_t>(kScrollX + kScrollWidth - 1);

  const char* title = entry.word[0] != '\0' ? entry.word : stateMessage(reader.state(), language);
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

  if (reader.state() == ReaderService::State::Ready) {
    maxMeaningPage =
        drawWrappedMeaningPage(display, entry.meaning, kMeaningX, kMeaningY, kMeaningWidth,
                               meaningPage);
  } else {
    maxMeaningPage = 0;
    const char* message = stateMessage(reader.state(), language);
    const int16_t messageW = text.getUTF8Width(message);
    int16_t messageX = static_cast<int16_t>(kMeaningX + (kMeaningWidth - messageW) / 2);
    if (messageX < kMeaningX + 4) {
      messageX = static_cast<int16_t>(kMeaningX + 4);
    }
    text.drawUTF8(messageX, 101, message);
    if (reader.errorText()[0] != '\0') {
      drawClippedTextLine(display, reader.errorText(), static_cast<int16_t>(kMeaningX + 6),
                          124, static_cast<int16_t>(kMeaningWidth - 12), false);
    }
  }

  drawMeaningPageHint(canvas, meaningPage, maxMeaningPage);

  canvas.drawFilledRectangle(0, kFooterY, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(kFooterY + kFooterHeight - 1),
                             ST7305_COLOR_BLACK);
  canvas.drawFilledRectangle(kScrollX, kFooterY, scrollRight,
                             static_cast<int16_t>(kFooterY + kFooterHeight - 1),
                             ST7305_COLOR_WHITE);
  char footer[96];
  const char* book = language == HomePage::Language::Zh ? reader.bookLabelZh() : reader.bookLabelEn();
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

void ReaderPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return;
  }

  detailView_ = DetailView::Library;
  selectedIndex_ = 0;
  resetMeaningPage();
}

bool ReaderPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                                   bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                                   ReaderService& reader, SdCardService& sd) {
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
    (void)reader.openBook(selectedIndex_, sd);
    detailView_ = DetailView::Word;
    resetMeaningPage();
    return true;
  }

  return upEdge || downEdge;
}

bool ReaderPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                  ReaderService& reader) {
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return false;
  }

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
}

bool ReaderPage::moveMeaningPage(int8_t delta) {
  if (delta < 0) {
    if (meaningPage_ == 0) {
      return false;
    }
    --meaningPage_;
    return true;
  }

  if (delta > 0) {
    if (meaningPage_ >= maxMeaningPage_) {
      return false;
    }
    ++meaningPage_;
    return true;
  }

  return false;
}

bool ReaderPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                              DisplayMonoTft& display, HomePage::Language language,
                              const ReaderService& reader) const {
  (void)yOffset;
  if (!isVocabularySelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (detailView_ == DetailView::Word) {
    drawWordDetail(display, language, reader, meaningPage_, maxMeaningPage_);
    return true;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const bool zh = language == HomePage::Language::Zh;
  const char* title = zh ? "单词库" : "Vocabulary";
  const char* const* labels = zh ? kLabelsZh : kLabelsEn;

  canvas.drawFilledRectangle(0, 0, static_cast<int16_t>(width - 1),
                             static_cast<int16_t>(kTitleBarHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  const int16_t titleWidth = text.getUTF8Width(title);
  text.drawUTF8(static_cast<int16_t>((width - titleWidth) / 2),
                kTitleBaselineY, title);

  for (uint8_t i = 0; i < 8; ++i) {
    const bool selected = i == selectedIndex_;
    const int16_t x = cardX(i);
    const int16_t y = cardY(i);
    if (selected) {
      drawFilledRoundRect(canvas, x, y, static_cast<int16_t>(x + kCardSize - 1),
                          static_cast<int16_t>(y + kCardSize - 1), kCardRadius,
                          ST7305_COLOR_BLACK);
    } else {
      drawRoundRect(canvas, x, y, kCardSize, kCardSize, kCardRadius, ST7305_COLOR_BLACK);
    }

    drawCenteredText(display, labels[i], x, y, kCardSize, kCardSize, selected);
  }

  return true;
}
