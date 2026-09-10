#include "BluetoothService.h"

#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>

#include <cstdarg>

namespace {
#ifndef OB_BT_LOG_ENABLED
#define OB_BT_LOG_ENABLED 1
#endif

constexpr uint8_t kHidReportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224)
    0x29, 0xE7,        //   Usage Maximum (231)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute): Modifier byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant): Reserved byte
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0               // End Collection
};

constexpr uint8_t kKeyboardEnterUsage = 0x28;

void btLog(const char* fmt, ...) {
#if OB_BT_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[BT] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void btErr(const char* fmt, ...) {
#if OB_BT_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[ERR][BT] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

class BluetoothServerCallbacks : public BLEServerCallbacks {
public:
  explicit BluetoothServerCallbacks(BluetoothService& service) : service_(service) {}

  void onConnect(BLEServer* pServer) override {
    service_.onConnected(pServer->getConnId());
  }

  void onDisconnect(BLEServer* /*pServer*/) override { service_.onDisconnected(); }

private:
  BluetoothService& service_;
};
}  // namespace

bool BluetoothService::begin() {
  const uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFU);
  snprintf(deviceName_, sizeof(deviceName_), "OpenBread Remote %04X",
           static_cast<unsigned>(chip));
  pendingReleaseMs_ = 0;
  lastShutterMs_ = 0;
  shutterPressActive_ = false;
  setState(State::Off, Error::None);
  return true;
}

void BluetoothService::tick(uint32_t nowMs) {
  if (shutterPressActive_ && nowMs >= pendingReleaseMs_ && inputReport_ != nullptr) {
    const uint8_t releaseReport[9] = {0x01, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
    const uint8_t bootRelease[8] = {0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00};
    inputReport_->setValue(releaseReport, sizeof(releaseReport));
    inputReport_->notify();
    if (bootInput_ != nullptr) {
      bootInput_->setValue(bootRelease, sizeof(bootRelease));
      bootInput_->notify();
    }
    shutterPressActive_ = false;
    pendingReleaseMs_ = 0;
    btLog("camera shutter release");
  }

  if (state_ != State::Advertising) {
    return;
  }

  if ((nowMs - advertiseStartMs_) < kAdvertiseTimeoutMs) {
    return;
  }

  btLog("advertise timeout, shutting down bluetooth");
  stop();
  setState(State::Error, Error::Timeout);
}

bool BluetoothService::start(uint32_t nowMs) {
  if (state_ == State::Advertising || state_ == State::Connected) {
    return false;
  }

  if (!initializeStack()) {
    btErr("ble init failed");
    setState(State::Error, Error::InitFailed);
    return false;
  }

  advertiseStartMs_ = nowMs;
  pendingReleaseMs_ = 0;
  shutterPressActive_ = false;
  BLEDevice::startAdvertising();
  btLog("advertising started name=%s", deviceName_);
  setState(State::Advertising, Error::None);
  return true;
}

void BluetoothService::stop() {
  if (!initialized_) {
    setState(State::Off, Error::None);
    return;
  }

  shuttingDown_ = true;
  shutterPressActive_ = false;
  pendingReleaseMs_ = 0;
  if (server_ != nullptr && activeConnId_ != 0xFFFF) {
    server_->disconnect(activeConnId_);
  }
  BLEDevice::stopAdvertising();
  cleanupStack();
  shuttingDown_ = false;
  setState(State::Off, Error::None);
}

bool BluetoothService::triggerCameraShutter(uint32_t nowMs) {
  if (!initialized_ || state_ != State::Connected || inputReport_ == nullptr) {
    return false;
  }
  if (shutterPressActive_) {
    return false;
  }
  if ((nowMs - lastShutterMs_) < kShutterCooldownMs) {
    return false;
  }

  const uint8_t pressReport[9] = {0x01, 0x00, 0x00, kKeyboardEnterUsage, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
  const uint8_t bootPress[8] = {0x00, 0x00, kKeyboardEnterUsage, 0x00,
                                0x00, 0x00, 0x00, 0x00};
  inputReport_->setValue(pressReport, sizeof(pressReport));
  inputReport_->notify();
  if (bootInput_ != nullptr) {
    bootInput_->setValue(bootPress, sizeof(bootPress));
    bootInput_->notify();
  }
  shutterPressActive_ = true;
  pendingReleaseMs_ = nowMs + kShutterReleaseDelayMs;
  lastShutterMs_ = nowMs;
  btLog("camera shutter press sent");
  return true;
}

BluetoothService::State BluetoothService::state() const { return state_; }

BluetoothService::Error BluetoothService::error() const { return error_; }

const char* BluetoothService::deviceName() const { return deviceName_; }

bool BluetoothService::isConnected() const { return state_ == State::Connected; }

bool BluetoothService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void BluetoothService::setState(State next, Error err) {
  if (state_ == next && error_ == err) {
    return;
  }
  state_ = next;
  error_ = err;
  changed_ = true;
}

bool BluetoothService::initializeStack() {
  if (initialized_) {
    return true;
  }

  if (!bleStackReady_) {
    BLEDevice::init(deviceName_);
    bleStackReady_ = true;
  }

  if (security_ == nullptr) {
    security_ = new BLESecurity();
    security_->setCapability(ESP_IO_CAP_NONE);
    security_->setAuthenticationMode(true, false, true);
  }

  if (server_ == nullptr) {
    server_ = BLEDevice::createServer();
  }
  if (server_ == nullptr) {
    cleanupStack();
    return false;
  }
  if (callbacks_ == nullptr) {
    callbacks_ = new BluetoothServerCallbacks(*this);
  }
  server_->setCallbacks(callbacks_);
  server_->advertiseOnDisconnect(false);

  if (hid_ == nullptr) {
    hid_ = new BLEHIDDevice(server_);
    hid_->manufacturer()->setValue("OpenBread");
    hid_->pnp(0x02, 0x303A, 0x1001, 0x0100);
    hid_->hidInfo(0x00, 0x01);
    hid_->reportMap(const_cast<uint8_t*>(kHidReportDescriptor), sizeof(kHidReportDescriptor));
    inputReport_ = hid_->inputReport(1);
    bootInput_ = hid_->bootInput();
    hid_->setBatteryLevel(100);
    hid_->startServices();
  }

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid_->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

  activeConnId_ = 0xFFFF;
  pendingReleaseMs_ = 0;
  shutterPressActive_ = false;
  initialized_ = true;
  btLog("ble stack initialized as hid remote");
  return true;
}

void BluetoothService::cleanupStack() {
  if (!initialized_) {
    return;
  }

  initialized_ = false;
  activeConnId_ = 0xFFFF;
  pendingReleaseMs_ = 0;
  shutterPressActive_ = false;
  btLog("ble remote session stopped, stack kept");
}

void BluetoothService::onConnected(uint16_t connId) {
  if (shuttingDown_) {
    return;
  }

  activeConnId_ = connId;
  btLog("phone connected conn_id=%u", static_cast<unsigned>(connId));
  setState(State::Connected, Error::None);
}

void BluetoothService::onDisconnected() {
  if (shuttingDown_) {
    return;
  }

  activeConnId_ = 0xFFFF;
  if (!initialized_) {
    setState(State::Off, Error::None);
    return;
  }

  BLEDevice::startAdvertising();
  btLog("phone disconnected, advertising restarted");
  setState(State::Advertising, Error::None);
}
