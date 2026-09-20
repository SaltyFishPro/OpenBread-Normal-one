#pragma once

#include <Arduino.h>
#include <ST7305_2p9_BW_DisplayDriver.h>
#include <ST73xxPins.h>
#include <U8g2_for_ST73XX.h>

class DisplayMonoTft {
public:
  DisplayMonoTft();

  bool begin();
  void clear();
  void present();
  void presentRegion(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
  void prepareForSleepKeepDisplay();
  void prepareForSleep();
  void restoreAfterSleep();
  int width() const;
  int height() const;
  // 帧缓冲整帧快照/回填，供上层缓存静态图层，避免每帧重画整屏
  size_t frameBufferBytes() const;
  void captureFrame(uint8_t* dst) const;
  void restoreFrame(const uint8_t* src);

  ST7305_2p9_BW_DisplayDriver& canvas();
  U8G2_FOR_ST73XX& text();

private:
  ST7305_2p9_BW_DisplayDriver display_;
  U8G2_FOR_ST73XX text_;
};
