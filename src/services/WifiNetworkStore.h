#pragma once

#include <stdint.h>
#include <stddef.h>

// One atomic NVS blob; recent successes first. No radio or display dependencies.
class WifiNetworkStore {
public:
  static constexpr uint8_t kCapacity = 5;
  static constexpr uint8_t kNoIndex = 0xFF;
  struct LastUsed {
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0;
  };
  struct Info {
    char ssid[33] = {};
    LastUsed lastUsed{};
  };
  bool begin();
  uint8_t count() const { return data_.count; }
  const Info* info(uint8_t index) const;
  int8_t find(const char* ssid) const;
  bool credentials(uint8_t index, char* ssid, size_t ssidBytes,
                   char* pass, size_t passBytes) const;
  bool save(const char* ssid, const char* pass, LastUsed used,
            uint8_t replacement = kNoIndex);
  bool forget(uint8_t index);

private:
  struct Record { Info info{}; char pass[65] = {}; };
  struct Data {
    uint32_t magic = 0x57494631U;
    uint8_t count = 0;
    uint8_t reserved[3] = {};
    Record records[kCapacity]{};
  };
  bool write(const Data& next);
  Data data_{};
};
