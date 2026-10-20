#pragma once

#include <stdint.h>

class SdBitmapFont;
class ST7305_2p9_BW_DisplayDriver;

namespace SdBitmapText {
struct Clip { int16_t x1, y1, x2, y2; };

// Strict Unicode scalar decoding, including four-byte UTF-8. Invalid bytes use U+FFFD.
bool nextCodepoint(const char*& text, uint32_t& codepoint);
int32_t measureUTF8(SdBitmapFont& font, const char* text);
// Transparent ink, baseline coordinates. Missing glyphs use a box and one em advance.
bool drawUTF8(ST7305_2p9_BW_DisplayDriver& canvas, SdBitmapFont& font,
               int16_t x, int16_t baseline, const char* text, uint16_t color, Clip clip);
}
