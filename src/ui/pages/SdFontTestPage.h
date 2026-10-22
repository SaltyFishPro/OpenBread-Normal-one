#pragma once

#include <stdint.h>
class DisplayMonoTft;
class SdBitmapFont;
class SdCardService;

class SdFontTestPage {
public:
  void begin(SdBitmapFont& font, SdCardService& sd);
  bool handleInput(bool up, bool down, bool ok, SdBitmapFont& font, SdCardService& sd);
  void render(DisplayMonoTft& display, int16_t yOffset, SdBitmapFont& font) const;

private:
  void open(SdBitmapFont& font, SdCardService& sd, bool refreshCard);
  uint8_t sizeIndex_ = 0;
  bool sdUnavailable_ = false;
};
