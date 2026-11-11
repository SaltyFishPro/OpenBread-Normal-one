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
    {"font-14.ui.bin", 39633U,
     "fcb7252b2a84b72670b0e4386544e738d9f4e9564562cb8becef5780627b27fd",
     14, 807U, 0xD66EDD4DU},
    {"font-20.bin", 3342737U,
     "2ce6aedd66c52fc34887e66c30e538d8e1b506e749ddb1f1ec354665c0ed528a",
     20, 44650U, 0x1BFCA471U},
    {"font-24.bin", 4004358U,
     "143a68bb1557b23b24d64953743cfb64bf81a9dffef95cc185a067c1d09cbb1b",
     24, 44650U, 0xEFA0F389U},
};
constexpr uint8_t kFileCount = sizeof(kFiles) / sizeof(kFiles[0]);
constexpr uint32_t kTotalBytes = 9656261U;
}  // namespace FontBundle
