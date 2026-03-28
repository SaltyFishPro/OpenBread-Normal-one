#include "Render1bpp.h"

bool Render1bpp::begin() { return true; }

void Render1bpp::beginFrame() { hasDirty_ = false; }

void Render1bpp::markDirty(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  if (x1 > x2) {
    int16_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    int16_t t = y1;
    y1 = y2;
    y2 = t;
  }

  if (!hasDirty_) {
    dirty_ = {x1, y1, x2, y2};
    hasDirty_ = true;
    return;
  }

  if (x1 < dirty_.x1) dirty_.x1 = x1;
  if (y1 < dirty_.y1) dirty_.y1 = y1;
  if (x2 > dirty_.x2) dirty_.x2 = x2;
  if (y2 > dirty_.y2) dirty_.y2 = y2;
}

bool Render1bpp::hasDirty() const { return hasDirty_; }

Render1bpp::Rect Render1bpp::dirty() const { return dirty_; }
