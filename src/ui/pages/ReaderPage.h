#pragma once

#include <stdint.h>

#include "../../services/ReaderService.h"

class DisplayMonoTft;
class SdCardService;

class ReaderPage {
public:
  static constexpr uint8_t kHomeIndex = 2;
  static constexpr uint8_t kVocabularyItemIndex = 0;

  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         ReaderService& reader, SdCardService& sd);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, ReaderService& reader);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, const ReaderService& reader) const;
  bool isStaticWordDetail(uint8_t homeFocus, uint8_t sectionFocus) const;

private:
  enum class DetailView : uint8_t {
    Library,
    Word
  };

  bool isVocabularySelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  void moveSelection(int8_t delta);
  void resetMeaningPage();
  bool moveMeaningPage(int8_t delta);

  DetailView detailView_ = DetailView::Library;
  uint8_t selectedIndex_ = 0;
  uint8_t meaningPage_ = 0;
  mutable uint8_t maxMeaningPage_ = 0;
};
