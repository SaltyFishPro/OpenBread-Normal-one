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
  int width() const;
  int height() const;

  ST7305_2p9_BW_DisplayDriver& canvas();
  U8G2_FOR_ST73XX& text();

private:
  ST7305_2p9_BW_DisplayDriver display_;
  U8G2_FOR_ST73XX text_;
};
