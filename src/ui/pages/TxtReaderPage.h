#pragma once
#include "../../services/TxtReaderService.h"
class DisplayMonoTft;
class SdBitmapFont;
class SdCardService;
class TxtReaderPage {
public:
  static bool selected(uint8_t home, uint8_t section) { return home == 2 && section == 2; }
  void enter(SdCardService& sd);
  void leave();
  bool input(bool left, bool up, bool down, bool ok, SdCardService& sd);
  // widthFont is the resident subset of the same size as the body font; it
  // supplies advances from PSRAM (no SD lookups) with identical metrics.
  bool update(SdBitmapFont& font, SdBitmapFont* widthFont = nullptr);
  bool busy() const;
  bool needsScan() const {
    return reader_.state() != TxtReaderService::State::Reading &&
           reader_.state() != TxtReaderService::State::Shelf;
  }
  void render(DisplayMonoTft& display, SdBitmapFont& full14, SdBitmapFont& full20,
              SdBitmapFont& ui24);
private:
  enum class View : uint8_t { Shelf, Reading, Menu, Jump };
  static int16_t measure(void* context, uint32_t cp);
  TxtReaderService reader_;
  View view_ = View::Shelf;
  uint8_t selection_ = 0, menu_ = 0;
  uint32_t jump_ = 0;
  SdBitmapFont* font_ = nullptr;
  SdBitmapFont* widthFont_ = nullptr;
  struct Width { uint32_t cp = UINT32_MAX; int16_t advance = 0; };
  static constexpr uint16_t kWidthCacheSize = 256;
  Width widths_[kWidthCacheSize];
};
