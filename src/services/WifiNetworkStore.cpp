#include "WifiNetworkStore.h"

#include <Preferences.h>
#include <cstring>
#include <cstdio>
#include "../log/Log.h"

namespace {
constexpr const char* kNamespace = "wifi";
constexpr const char* kRecords = "networks";
bool validSsid(const char* value) {
  const size_t length = strnlen(value, 33);
  if (length == 0 || length > 32) return false;
  bool nonSpace = false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x20 || c == 0x7F) return false;
    nonSpace |= c != ' ';
  }
  return nonSpace;
}
}

bool WifiNetworkStore::begin() {
  data_ = Data{};
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return true;  // First boot: no namespace yet.
  const size_t bytes = prefs.getBytesLength(kRecords);
  if (bytes != 0) {
    Data stored{};
    const bool read = bytes == sizeof(stored) &&
        prefs.getBytes(kRecords, &stored, sizeof(stored)) == sizeof(stored);
    prefs.end();
    if (!read || stored.magic != data_.magic || stored.count > kCapacity) {
      LogE("WIFI", "saved network store invalid bytes=%u", static_cast<unsigned>(bytes));
      return false;
    }
    for (uint8_t i = 0; i < stored.count; ++i) {
      if (!validSsid(stored.records[i].info.ssid) ||
          strnlen(stored.records[i].pass, 65) > 64) return false;
      for (uint8_t j = 0; j < i; ++j)
        if (strcmp(stored.records[i].info.ssid, stored.records[j].info.ssid) == 0) return false;
    }
    data_ = stored;
    return true;
  }
  char ssid[33] = {}, pass[65] = {};
  prefs.getString("ssid", ssid, sizeof(ssid));
  prefs.getString("pass", pass, sizeof(pass));
  prefs.end();
  if (!validSsid(ssid)) return true;
  const bool migrated = save(ssid, pass, LastUsed{});
  LogI("WIFI", "legacy network migration %s", migrated ? "complete" : "failed");
  return migrated;
}

const WifiNetworkStore::Info* WifiNetworkStore::info(uint8_t index) const {
  return index < data_.count ? &data_.records[index].info : nullptr;
}

int8_t WifiNetworkStore::find(const char* ssid) const {
  if (!ssid) return -1;
  for (uint8_t i = 0; i < data_.count; ++i)
    if (strcmp(data_.records[i].info.ssid, ssid) == 0) return static_cast<int8_t>(i);
  return -1;
}

bool WifiNetworkStore::credentials(uint8_t index, char* ssid, size_t ssidBytes,
                                    char* pass, size_t passBytes) const {
  if (index >= data_.count || ssidBytes < 33 || passBytes < 65) return false;
  snprintf(ssid, ssidBytes, "%s", data_.records[index].info.ssid);
  snprintf(pass, passBytes, "%s", data_.records[index].pass);
  return true;
}

bool WifiNetworkStore::write(const Data& next) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    LogE("WIFI", "saved network NVS open failed");
    return false;
  }
  const bool ok = prefs.putBytes(kRecords, &next, sizeof(next)) == sizeof(next);
  // The blob is authoritative, including an empty list. Remove legacy secrets
  // only after a successful commit; never resurrect a forgotten network.
  if (ok) { prefs.remove("ssid"); prefs.remove("pass"); }
  prefs.end();
  if (!ok) { LogE("WIFI", "saved network NVS commit failed; previous records retained"); return false; }
  data_ = next;
  return true;
}

bool WifiNetworkStore::save(const char* ssid, const char* pass, LastUsed used,
                             uint8_t replacement) {
  if (!ssid || !pass || !validSsid(ssid) || strnlen(pass, 65) > 64) return false;
  Data next = data_;
  const int8_t existing = find(ssid);
  uint8_t remove = existing >= 0 ? static_cast<uint8_t>(existing) : kNoIndex;
  if (remove == kNoIndex && next.count == kCapacity) {
    if (replacement >= next.count) return false;
    remove = replacement;
  }
  if (remove < next.count) {
    for (uint8_t i = remove; i + 1 < next.count; ++i) next.records[i] = next.records[i + 1];
    --next.count;
  }
  for (uint8_t i = next.count; i > 0; --i) next.records[i] = next.records[i - 1];
  next.records[0] = Record{};
  snprintf(next.records[0].info.ssid, sizeof(next.records[0].info.ssid), "%s", ssid);
  snprintf(next.records[0].pass, sizeof(next.records[0].pass), "%s", pass);
  next.records[0].info.lastUsed = used;
  ++next.count;
  return write(next);
}

bool WifiNetworkStore::forget(uint8_t index) {
  if (index >= data_.count) return false;
  Data next = data_;
  for (uint8_t i = index; i + 1 < next.count; ++i) next.records[i] = next.records[i + 1];
  next.records[--next.count] = Record{};
  return write(next);
}
