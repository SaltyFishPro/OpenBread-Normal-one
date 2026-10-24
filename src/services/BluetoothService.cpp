#include "BluetoothService.h"

#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>

#include "../log/Log.h"

namespace {
constexpr uint8_t kHidReportDescriptor[] = {
    0x05, 0x0D,        // Usage Page (Digitizers)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x09, 0x32,        //     Usage (In Range)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x06,        //   Report Count (6)
    0x81, 0x03,        //   Input (Constant)
    0x09, 0x51,        //     Usage (Contact Identifier)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0xFF,        //     Logical Maximum (255)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x00, 0x00,  //     Logical Minimum (0)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0xC0,              //   End Finger collection
    0x05, 0x0D,        //   Usage Page (Digitizers)
    0x09, 0x54,        //   Usage (Contact Count)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0xC0               // End touch screen collection
};

constexpr uint16_t kTouchCoordinateMax = 32767;
constexpr uint16_t kTapCenterX = kTouchCoordinateMax / 2;
constexpr uint16_t kTapCenterY = kTouchCoordinateMax / 2;
constexpr uint16_t kCameraShutterX = kTouchCoordinateMax / 2;
constexpr uint16_t kCameraShutterY = (kTouchCoordinateMax * 9U) / 10U;
constexpr uint16_t kSwipeStartY = (kTouchCoordinateMax * 3U) / 4U;
constexpr uint16_t kSwipeEndY = kTouchCoordinateMax / 4U;
constexpr size_t kTouchReportBytes = 7;  // Tip/in-range + contact ID + X/Y + contact count.
constexpr uint16_t kHidDigitizerAppearance = 0x03C5;

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
  touchGesture_ = TouchGesture::None;
  touchSwipeUp_ = false;
  touchStep_ = 0;
  touchX_ = 0;
  touchY_ = 0;
  setState(State::Off, Error::None);
  return true;
}

void BluetoothService::tick(uint32_t nowMs) {
  if (touchGesture_ != TouchGesture::None && nowMs >= pendingReleaseMs_) {
    if (touchGesture_ == TouchGesture::Tap) {
      sendTouchReport(false, touchX_, touchY_);
      LogD("BT", "touch tap released");
      touchGesture_ = TouchGesture::None;
      touchStep_ = 0;
      pendingReleaseMs_ = 0;
    } else if (touchStep_ <= kTouchSwipeSteps) {
      const uint16_t startY = touchSwipeUp_ ? kSwipeStartY : kSwipeEndY;
      const uint16_t endY = touchSwipeUp_ ? kSwipeEndY : kSwipeStartY;
      const uint32_t progress = touchStep_;
      touchY_ = static_cast<uint16_t>(startY +
                                      ((static_cast<int32_t>(endY) - startY) * progress) /
                                          kTouchSwipeSteps);
      sendTouchReport(true, touchX_, touchY_);
      ++touchStep_;
      pendingReleaseMs_ = nowMs + kTouchSwipeStepMs;
    } else {
      sendTouchReport(false, touchX_, touchY_);
      LogD("BT", "touch swipe released direction=%s", touchSwipeUp_ ? "up" : "down");
      touchGesture_ = TouchGesture::None;
      touchStep_ = 0;
      pendingReleaseMs_ = 0;
    }
  }

  if (state_ != State::Advertising) {
    return;
  }

  if ((nowMs - advertiseStartMs_) < kAdvertiseTimeoutMs) {
    return;
  }

  LogI("BT", "advertise timeout, shutting down bluetooth");
  stop();
  setState(State::Error, Error::Timeout);
}

bool BluetoothService::start(uint32_t nowMs) {
  if (state_ == State::Advertising || state_ == State::Connected) {
    return false;
  }

  if (!initializeStack()) {
    LogE("BT", "ble init failed");
    setState(State::Error, Error::InitFailed);
    return false;
  }

  advertiseStartMs_ = nowMs;
  pendingReleaseMs_ = 0;
  touchGesture_ = TouchGesture::None;
  touchStep_ = 0;
  BLEDevice::startAdvertising();
  LogI("BT", "advertising started name=%s", deviceName_);
  setState(State::Advertising, Error::None);
  return true;
}

void BluetoothService::stop() {
  if (!initialized_) {
    setState(State::Off, Error::None);
    return;
  }

  shuttingDown_ = true;
  touchGesture_ = TouchGesture::None;
  touchStep_ = 0;
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
  if ((nowMs - lastShutterMs_) < kShutterCooldownMs) {
    return false;
  }
  return sendTouchTap(nowMs, kCameraShutterX, kCameraShutterY, "camera shutter");
}

bool BluetoothService::triggerPageUp(uint32_t nowMs) {
  return sendTouchSwipe(nowMs, true, "pager swipe up");
}

bool BluetoothService::triggerPageDown(uint32_t nowMs) {
  return sendTouchSwipe(nowMs, false, "pager swipe down");
}

bool BluetoothService::triggerPlayPause(uint32_t nowMs) {
  return sendTouchTap(nowMs, kTapCenterX, kTapCenterY, "pager play pause");
}

bool BluetoothService::sendTouchTap(uint32_t nowMs, uint16_t x, uint16_t y, const char* label) {
  if (!initialized_ || state_ != State::Connected || inputReport_ == nullptr ||
      touchGesture_ != TouchGesture::None || (nowMs - lastShutterMs_) < kPagerCooldownMs) {
    return false;
  }

  touchX_ = x;
  touchY_ = y;
  sendTouchReport(true, touchX_, touchY_);
  touchGesture_ = TouchGesture::Tap;
  pendingReleaseMs_ = nowMs + kTouchTapDurationMs;
  lastShutterMs_ = nowMs;
  LogD("BT", "%s sent x=%u y=%u", label, static_cast<unsigned>(x), static_cast<unsigned>(y));
  return true;
}

bool BluetoothService::sendTouchSwipe(uint32_t nowMs, bool upward, const char* label) {
  if (!initialized_ || state_ != State::Connected || inputReport_ == nullptr ||
      touchGesture_ != TouchGesture::None || (nowMs - lastShutterMs_) < kPagerCooldownMs) {
    return false;
  }

  touchX_ = kTapCenterX;
  touchSwipeUp_ = upward;
  touchY_ = upward ? kSwipeStartY : kSwipeEndY;
  touchStep_ = 1;
  sendTouchReport(true, touchX_, touchY_);
  touchGesture_ = TouchGesture::Swipe;
  pendingReleaseMs_ = nowMs + kTouchSwipeStepMs;
  lastShutterMs_ = nowMs;
  LogD("BT", "%s started x=%u y=%u", label, static_cast<unsigned>(touchX_),
        static_cast<unsigned>(touchY_));
  return true;
}

void BluetoothService::sendTouchReport(bool contact, uint16_t x, uint16_t y) {
  if (inputReport_ == nullptr) {
    return;
  }

  const uint8_t report[kTouchReportBytes] = {
      static_cast<uint8_t>(contact ? 0x03 : 0x00), 0x00,
      static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
      static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
      static_cast<uint8_t>(contact ? 0x01 : 0x00)};
  inputReport_->setValue(report, sizeof(report));
  inputReport_->notify();
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
    if (!BLEDevice::init(deviceName_)) {
      LogE("BT", "ble device init returned failure");
      return false;
    }
    bleStackReady_ = true;
    LogI("BT", "ble device initialized, controller memory retained");
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
    if (inputReport_ == nullptr) {
      LogE("BT", "hid input report allocation failed id=1");
      cleanupStack();
      return false;
    }
    hid_->setBatteryLevel(100);
    hid_->startServices();
  }

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  if (advertising == nullptr) {
    LogE("BT", "ble advertising allocation failed");
    cleanupStack();
    return false;
  }
  advertising->setAppearance(kHidDigitizerAppearance);
  advertising->addServiceUUID(hid_->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

  activeConnId_ = 0xFFFF;
  pendingReleaseMs_ = 0;
  touchGesture_ = TouchGesture::None;
  touchStep_ = 0;
  initialized_ = true;
  LogI("BT", "ble stack initialized as touch remote");
  return true;
}

void BluetoothService::cleanupStack() {
  if (!initialized_ && !bleStackReady_ && server_ == nullptr && hid_ == nullptr) {
    return;
  }

  initialized_ = false;
  activeConnId_ = 0xFFFF;
  pendingReleaseMs_ = 0;
  touchGesture_ = TouchGesture::None;
  touchStep_ = 0;
  inputReport_ = nullptr;

  // server_/client 及其服务与特征由 BLEDevice 统一回收；释放期间 callbacks_ 仍然有效。
  if (bleStackReady_) {
    BLEDevice::deinit(false);
    bleStackReady_ = false;
    LogI("BT", "ble stack deinitialized, controller memory retained");
  }
  server_ = nullptr;

  // 本次会话自己分配的对象；BLEHIDDevice 析构为空，服务不在这里释放。
  delete hid_;
  hid_ = nullptr;
  delete callbacks_;
  callbacks_ = nullptr;
  delete security_;
  security_ = nullptr;

  LogI("BT", "ble stack released");
}

void BluetoothService::onConnected(uint16_t connId) {
  if (shuttingDown_) {
    return;
  }

  activeConnId_ = connId;
  LogI("BT", "phone connected conn_id=%u", static_cast<unsigned>(connId));
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
  LogI("BT", "phone disconnected, advertising restarted");
  setState(State::Advertising, Error::None);
}
