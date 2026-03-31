#include "SdCardDriver.h"

#include <SD_MMC.h>

#include "BoardConfig.h"

namespace {
constexpr uint8_t kMaxMountRetries = 1;
constexpr uint32_t kMountFrequencyKhz = 10000;
}

bool SdCardDriver::begin() {
  Info info;
  return refresh(info);
}

bool SdCardDriver::refresh(Info& info) {
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    const bool pinsReady =
        SD_MMC.setPins(BoardConfig::kPinSdClk, BoardConfig::kPinSdMosi, BoardConfig::kPinSdMiso);
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
    info = Info{};
    return false;
  }

  info.mounted = true;
  info.totalBytes = SD_MMC.cardSize();
  info.usedBytes = SD_MMC.usedBytes();
  return true;
}
