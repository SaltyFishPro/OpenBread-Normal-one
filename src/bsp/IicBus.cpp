#include "IicBus.h"

#include <Arduino.h>
#include <Wire.h>

#include "BoardConfig.h"
#include "../log/Log.h"

namespace IicBus {

bool begin(uint32_t clockHz, bool reset) {
  if (reset) {
    Wire.end();
  }

  const bool ready = Wire.begin(BoardConfig::kPinIicSda, BoardConfig::kPinIicScl);
  if (ready) {
    Wire.setClock(clockHz);
    if (reset) {
      LogI("IIC", "bus reset SDA=%d SCL=%d clock=%lu pullups=esp32s3",
           BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
           static_cast<unsigned long>(clockHz));
    }
  } else {
    LogE("IIC", "bus init failed SDA=%d SCL=%d clock=%lu reset=%u",
         BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
         static_cast<unsigned long>(clockHz), reset ? 1U : 0U);
  }
  return ready;
}

}  // namespace IicBus
