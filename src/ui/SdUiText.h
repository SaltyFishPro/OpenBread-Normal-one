#pragma once
#include "SdBitmapText.h"
#include "../services/SdBitmapFont.h"
#include "../services/UiFontCache.h"
#include <U8g2_for_ST73XX.h>

class DisplayMonoTft;

// SD typography: fixed UI glyphs use an offline cache; external text can use
// an open NBFT file. Latin/numeric U8g2 faces remain usable if loading fails.
namespace SdUiText {
bool ready(const SdBitmapFont* font, uint16_t maxLineHeight = 101);
int16_t width(DisplayMonoTft& display, SdBitmapFont* font, const char* value);
void draw(DisplayMonoTft& display, SdBitmapFont* font, int16_t x, int16_t baseline,
          const char* value, uint16_t color, SdBitmapText::Clip clip);
void centered(DisplayMonoTft& display, SdBitmapFont* font, int16_t centerX, int16_t baseline,
              const char* value, uint16_t color, SdBitmapText::Clip clip);
// Scoped adapter for existing small-font pages. It redirects the temporary
// Latin/number face to the selected SD font without mounting SD itself.
class Scope {
public:
  Scope(DisplayMonoTft& display, SdBitmapFont* font);
  ~Scope();
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
private:
  static int16_t measure(void* self, const char* value);
  static int16_t render(void* self, int16_t x, int16_t y, const char* value,
                         uint16_t fg, uint16_t bg, bool transparent);
  DisplayMonoTft& display_;
  SdBitmapFont* font_;
  U8G2_FOR_ST73XX::Utf8Renderer previous_;
};

// Outer renderer for menu/headers/popups. Inner content scopes restore it.
class FixedScope {
public:
  FixedScope(DisplayMonoTft& display, const UiFontCache& cache);
  ~FixedScope();
  FixedScope(const FixedScope&) = delete;
  FixedScope& operator=(const FixedScope&) = delete;
private:
  static int16_t measure(void* self, const char* value);
  static int16_t render(void* self, int16_t x, int16_t y, const char* value,
                         uint16_t fg, uint16_t bg, bool transparent);
  DisplayMonoTft& display_;
  const UiFontCache& cache_;
  U8G2_FOR_ST73XX::Utf8Renderer previous_;
};
}
