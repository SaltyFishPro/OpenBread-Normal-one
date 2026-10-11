#pragma once

#include <stdint.h>

class DisplayMonoTft;
namespace IconBitmap { struct Anim; }

// Waiting-page presentation only; USB session and full-frame display stay in UiManager.
class ComputerStatusPage {
public:
  static constexpr uint32_t kAnimationFrameIntervalMs = 42U;

  void prepareEnter();
  void begin(uint32_t nowMs);
  bool needsAnimationFrame(uint32_t nowMs, const IconBitmap::Anim& icon) const;
  bool needsRedraw(uint32_t nowMs, const IconBitmap::Anim& icon, bool studioConnected) const;
  void render(DisplayMonoTft& display, int16_t yOffset, const IconBitmap::Anim& icon,
              bool studioConnected, uint32_t nowMs);

private:
  uint16_t iconFrame(uint32_t nowMs, const IconBitmap::Anim& icon) const;

  uint32_t enteredMs_ = 0;
  uint16_t lastRenderedFrame_ = 0xFFFFU;
  bool entered_ = false;
  bool lastStudioConnected_ = false;
};
