#pragma once

#include <stdint.h>

#include "../../services/ReaderService.h"

class DisplayMonoTft;
class SdCardService;
class SdBitmapFont;

class ReaderPage {
public:
  static constexpr uint8_t kHomeIndex = 2;
  static constexpr uint8_t kVocabularyItemIndex = 0;

  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus, SdBitmapFont& font);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         ReaderService& reader, SdCardService& sd, SdBitmapFont& font,
                         SdBitmapFont* titleFont = nullptr);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, ReaderService& reader,
                        SdBitmapFont& font);
  bool updateMeaning(const ReaderService& reader, SdBitmapFont& font);
  bool isPreparingMeaning() const { return meaningState_ == MeaningState::Building; }
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, const ReaderService& reader, SdBitmapFont& font,
                    SdBitmapFont* titleFont = nullptr) const;
  bool isStaticWordDetail(uint8_t homeFocus, uint8_t sectionFocus) const;

private:
  enum class DetailView : uint8_t {
    Library,
    Word
  };
  enum class MeaningState : uint8_t { Pending, Building, Ready, Unsupported };
  struct MeaningLine { uint16_t start = 0, length = 0; };
  static constexpr uint8_t kMaxMeaningLines = 8;

  bool isVocabularySelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  void moveSelection(int8_t delta);
  void resetMeaningPage();
  bool moveMeaningPage(int8_t delta);
  void finishMeaningLine(uint16_t end);
  void drawMeaning(DisplayMonoTft& display, const ReaderService& reader, SdBitmapFont& font) const;

  DetailView detailView_ = DetailView::Library;
  uint8_t selectedIndex_ = 0;
  uint16_t meaningPage_ = 0;
  uint16_t maxMeaningPage_ = 0;
  MeaningState meaningState_ = MeaningState::Pending;
  MeaningLine meaningLines_[kMaxMeaningLines];
  uint8_t meaningLinesPerPage_ = 0;
  uint16_t meaningLineHeight_ = 0;
  uint16_t meaningScanOffset_ = 0;
  uint16_t meaningLineStart_ = 0;
  uint16_t meaningLineCount_ = 0;
  int32_t meaningLineWidth_ = 0;
};
