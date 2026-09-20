#include "UsbSerial.h"

#include <Arduino.h>
#include <Preferences.h>

namespace UsbSerial {
namespace {

constexpr const char* kPrefsNamespace = "ob_usb";
constexpr const char* kPrefsKeyEnabled = "cdc";
// 等待主机完成枚举，避免开机日志丢失；关闭状态下这段延时也会一并省掉。
constexpr uint32_t kOpenSettleMs = 300;

bool enabled = true;

void openPort(bool settle) {
  Serial.begin(kBaud);
  if (settle) {
    delay(kOpenSettleMs);
  }
}

void closePort() {
  Serial.flush();
  Serial.end();
}

bool readPersisted(bool& out) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    return false;
  }
  out = prefs.getBool(kPrefsKeyEnabled, true);
  prefs.end();
  return true;
}

void persist(bool value) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  (void)prefs.putBool(kPrefsKeyEnabled, value);
  prefs.end();
}

}  // namespace

bool begin() {
  bool stored = true;
  (void)readPersisted(stored);

  enabled = stored;
  // 无论开关状态都先初始化：HWCDC::begin() 会向外设管理器注册 USB D+/D- 引脚，
  // 之后 end() 才能释放引脚并把 D+/D- 拉低，让主机真正看到设备断开。
  openPort(enabled);
  if (!enabled) {
    closePort();
  }
  return enabled;
}

bool setEnabled(bool value) {
  if (value == enabled) {
    return enabled;
  }

  if (value) {
    enabled = true;
    openPort(true);
    if (Serial) {
      Serial.println("[USB] cdc on");
    }
  } else {
    if (Serial) {
      Serial.println("[USB] cdc off");
    }
    closePort();
    enabled = false;
  }
  persist(enabled);
  return enabled;
}

bool isEnabled() { return enabled; }

}  // namespace UsbSerial
