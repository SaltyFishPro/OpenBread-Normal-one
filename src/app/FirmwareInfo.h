#pragma once

#include <stdint.h>

namespace FirmwareInfo {
constexpr const char* kProduct = "openbread-normal-one";
constexpr const char* kChannel = "stable";
constexpr const char* kVersion = "0.1.1";
constexpr uint32_t kBuild = 2;
constexpr const char* kManifestUrl =
    "https://ota.openbread.net/ota/openbread-normal-one/manifest.json";
}  // namespace FirmwareInfo
