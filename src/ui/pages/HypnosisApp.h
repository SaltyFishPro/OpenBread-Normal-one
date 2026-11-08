#pragma once

#include <stdint.h>

class DisplayMonoTft;

class HypnosisApp {
public:
  void enter(uint32_t nowMs);
  void exit();
  void replay(uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool consumeChanged();
  bool needsAnimationFrame() const { return open_; }
  void render(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs) const;

private:
  uint32_t startedMs_ = 0;
  uint32_t lastFrame_ = 0;
  bool open_ = false;
  bool changed_ = false;
};
