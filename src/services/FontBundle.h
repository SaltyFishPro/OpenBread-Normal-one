#pragma once

#include <stdint.h>

namespace FontBundle {
constexpr char kBaseUrl[] = "https://ota.openbread.net/ota/openbread-normal-one/Font/";
constexpr char kDirectory[] = "/Font";
constexpr char kPartPath[] = "/Font/download.part";

struct File {
  const char* name;
  uint32_t bytes;
  const char* sha256;
  uint16_t pixelSize;
  uint32_t glyphCount;
  uint32_t payloadCrc;
};

constexpr File kFiles[] = {
    {"font-14.bin", 2269533U,
     "4dd04cceb3efb9a307cdc54e03d2bf69a3b2fd1e069feffbbaabb5ca23c60884",
     14, 44650U, 0xDFCFEB1BU},
    {"font-14.ui.bin", 183807U,
     "dc456168edf47250cf875bc69017f51d68fc8add078b2037eb1c2cb9c7f34a50",
     14, 3610U, 0xB7CB34B1U},
    {"font-20.bin", 3342737U,
     "2ce6aedd66c52fc34887e66c30e538d8e1b506e749ddb1f1ec354665c0ed528a",
     20, 44650U, 0x1BFCA471U},
    {"font-20.ui.bin", 285946U,
     "660a7f1d0124194c40152cd3c8b1e0c0e979eccd697ef2c43a05034c3da99b04",
     20, 3610U, 0x361E90A9U},
    {"font-24.bin", 4004358U,
     "143a68bb1557b23b24d64953743cfb64bf81a9dffef95cc185a067c1d09cbb1b",
     24, 44650U, 0xEFA0F389U},
    {"font-24.ui.bin", 325805U,
     "83ce7b5f50d1956cd820a8568c4206c7ffb5702cc34bfa3e33a30110b88a1880",
     24, 3610U, 0x8BB3548BU},
};
constexpr uint8_t kFileCount = sizeof(kFiles) / sizeof(kFiles[0]);
constexpr uint32_t kTotalBytes = 10412186U;
}  // namespace FontBundle
