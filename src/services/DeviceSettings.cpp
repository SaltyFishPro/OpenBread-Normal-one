#include "DeviceSettings.h"

#include <Preferences.h>
#include <cstring>
#include "../log/Log.h"

namespace {
constexpr char kNamespace[] = "device-admin";
constexpr char kKey[] = "settings";
}

bool DeviceSettings::valid(const Values& value) {
  if (value.magic != Values{}.magic) return false;
  if (value.sleepSeconds != 0 && value.sleepSeconds != 20 &&
      value.sleepSeconds != 60 && value.sleepSeconds != 300) return false;
  if (value.adminTimeoutSeconds != 0 && value.adminTimeoutSeconds != 600 &&
      value.adminTimeoutSeconds != 1800) return false;
  const size_t len = strnlen(value.name, sizeof(value.name));
  if (!len || len == sizeof(value.name)) return false;
  bool nonSpace = false;
  for (size_t i = 0; i < len; ++i) {
    const auto c = static_cast<unsigned char>(value.name[i]);
    if (c < 0x20 || c == 0x7f) return false;
    if (c != ' ') nonSpace = true;
  }
  return nonSpace;
}

bool DeviceSettings::begin() {
  values_ = Values{};
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return true;
  const size_t length = prefs.getBytesLength(kKey);
  if (!length) { prefs.end(); return true; }
  Values stored{};
  const bool read = length == sizeof(stored) &&
                    prefs.getBytes(kKey, &stored, sizeof(stored)) == sizeof(stored);
  prefs.end();
  if (!read || !valid(stored)) {
    LogE("ADMIN", "invalid device settings bytes=%u; using defaults", static_cast<unsigned>(length));
    return false;
  }
  values_ = stored;
  return true;
}

bool DeviceSettings::save(const Values& values) {
  if (!valid(values)) return false;
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    LogE("ADMIN", "device settings NVS open failed");
    return false;
  }
  const bool ok = prefs.putBytes(kKey, &values, sizeof(values)) == sizeof(values);
  prefs.end();
  if (!ok) {
    LogE("ADMIN", "device settings NVS write failed");
    return false;
  }
  values_ = values;
  LogI("ADMIN", "device settings saved name=%s sleep_s=%lu timeout_s=%lu", values.name,
       static_cast<unsigned long>(values.sleepSeconds),
       static_cast<unsigned long>(values.adminTimeoutSeconds));
  return true;
}
