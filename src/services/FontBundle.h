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
    {"font-14.ui.bin", 183899U,
     "c222f8da3df3b754bc60efe502d06f06b51033195e3a27eeb22b1e0f08b17ec7",
     14, 3613U, 0x3747DE2BU},
    {"font-20.bin", 3342737U,
     "2ce6aedd66c52fc34887e66c30e538d8e1b506e749ddb1f1ec354665c0ed528a",
     20, 44650U, 0x1BFCA471U},
    {"font-20.ui.bin", 286053U,
     "6fd218ec0b3612aaba7f6fd9d255fa448d556ed9b0fc707f21c553e3e5051db8",
     20, 3613U, 0x119F906EU},
    {"font-24.bin", 4004358U,
     "143a68bb1557b23b24d64953743cfb64bf81a9dffef95cc185a067c1d09cbb1b",
     24, 44650U, 0xEFA0F389U},
    {"font-24.ui.bin", 325920U,
     "a3d7df2d351e0542026c28e711fce29a403a729a95e8f449e753ea7a9efb6491",
     24, 3613U, 0xB994580FU},
};
constexpr uint8_t kFileCount = sizeof(kFiles) / sizeof(kFiles[0]);
constexpr uint32_t kTotalBytes = 10412500U;
}  // namespace FontBundle
