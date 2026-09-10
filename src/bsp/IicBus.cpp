#include "IicBus.h"

#include <Arduino.h>
#include <Wire.h>

#include "BoardConfig.h"

namespace IicBus {

#ifndef OB_IIC_BUS_LOG_ENABLED
#define OB_IIC_BUS_LOG_ENABLED 1
#endif

bool begin(uint32_t clockHz, bool reset) {
  if (reset) {
    Wire.end();
  }

  const bool ready = Wire.begin(BoardConfig::kPinIicSda, BoardConfig::kPinIicScl);
  if (ready) {
    Wire.setClock(clockHz);
#if OB_IIC_BUS_LOG_ENABLED
    if (reset && Serial) {
      Serial.printf("[IIC] bus reset SDA=%d SCL=%d clock=%lu pullups=esp32s3\r\n",
                    BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
                    static_cast<unsigned long>(clockHz));
    }
#endif
  } else {
#if OB_IIC_BUS_LOG_ENABLED
    if (Serial) {
      Serial.printf("[ERR][IIC] bus init failed SDA=%d SCL=%d clock=%lu reset=%u\r\n",
                    BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
                    static_cast<unsigned long>(clockHz), reset ? 1U : 0U);
    }
#endif
  }
  return ready;
}

}  // namespace IicBus
