#pragma once

#include <stdint.h>

class Render1bpp {
public:
  struct Rect {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
  };

  bool begin();
  void beginFrame();
  void markDirty(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
  bool hasDirty() const;
  Rect dirty() const;

private:
  bool hasDirty_ = false;
  Rect dirty_ = {0, 0, 0, 0};
};
