#include "SdCardDriver.h"

#include <SD_MMC.h>

#include "BoardConfig.h"

namespace {
constexpr uint8_t kMaxMountRetries = 1;
constexpr uint32_t kMountFrequencyKhz = 10000;
}

#ifndef OB_SD_LOG_ENABLED
#define OB_SD_LOG_ENABLED 1
#endif

bool SdCardDriver::begin() {
  Info info;
  return refresh(info);
}

bool SdCardDriver::refresh(Info& info) {
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    const bool pinsReady =
        SD_MMC.setPins(BoardConfig::kPinSdClk, BoardConfig::kPinSdCmd,
                       BoardConfig::kPinSdData0);
    if (!pinsReady) {
      info = Info{};
      return false;
    }
    for (uint8_t attempt = 0; attempt <= kMaxMountRetries; ++attempt) {
      if (SD_MMC.begin("/sdcard", true, false, kMountFrequencyKhz)) {
        break;
      }
    }
  }

  const sdcard_type_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
#if OB_SD_LOG_ENABLED
    Serial.printf("[ERR][SD] mount failed mode=1bit clock_khz=%lu\r\n",
                  static_cast<unsigned long>(kMountFrequencyKhz));
#endif
    info = Info{};
    return false;
  }

  info.mounted = true;
  info.totalBytes = SD_MMC.cardSize();
  info.usedBytes = SD_MMC.usedBytes();
  File root = SD_MMC.open("/");
  info.rootReadable = root && root.isDirectory();
  root.close();
#if OB_SD_LOG_ENABLED
  Serial.printf("[SD] ready total=%llu used=%llu root=%u\r\n",
                static_cast<unsigned long long>(info.totalBytes),
                static_cast<unsigned long long>(info.usedBytes), info.rootReadable ? 1U : 0U);
#endif
  return true;
}

void SdCardDriver::end() { SD_MMC.end(); }
