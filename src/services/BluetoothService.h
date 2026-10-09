#pragma once

#include <Arduino.h>

class BLESecurity;
class BLEServerCallbacks;
class BLEServer;
class BLEHIDDevice;
class BLECharacteristic;

class BluetoothService {
public:
  enum class State : uint8_t {
    Off,
    Advertising,
    Connected,
    Error
  };

  enum class Error : uint8_t {
    None,
    InitFailed,
    StartFailed,
    Timeout
  };

  bool begin();
  void tick(uint32_t nowMs);

  bool start(uint32_t nowMs);
  void stop();
  bool triggerCameraShutter(uint32_t nowMs);
  bool triggerPageUp(uint32_t nowMs);
  bool triggerPageDown(uint32_t nowMs);
  bool triggerPlayPause(uint32_t nowMs);

  State state() const;
  Error error() const;
  const char* deviceName() const;
  bool isConnected() const;
  // 超时错误可保留在页面上，无线栈释放后不再占用高频与休眠条件。
  bool isRadioActive() const { return initialized_ || bleStackReady_; }
  bool consumeChanged();

  // Internal server callbacks route through these hooks.
  void onConnected(uint16_t connId);
  void onDisconnected();

private:
  void setState(State next, Error err = Error::None);
  bool sendKeyboardUsage(uint32_t nowMs, uint8_t usage, const char* label);
  bool sendConsumerUsage(uint32_t nowMs, uint16_t usage, const char* label);
  bool initializeStack();
  void cleanupStack();
  State state_ = State::Off;
  Error error_ = Error::None;
  bool changed_ = false;
  bool initialized_ = false;
  bool bleStackReady_ = false;
  bool shuttingDown_ = false;
  enum class ActiveReport : uint8_t { None, Keyboard, Consumer };
  ActiveReport activeReport_ = ActiveReport::None;
  uint32_t advertiseStartMs_ = 0;
  uint32_t pendingReleaseMs_ = 0;
  uint32_t lastShutterMs_ = 0;
  bool shutterPressActive_ = false;
  uint16_t activeConnId_ = 0xFFFF;
  char deviceName_[32] = {0};

  BLEServer* server_ = nullptr;
  BLEHIDDevice* hid_ = nullptr;
  BLECharacteristic* inputReport_ = nullptr;
  BLECharacteristic* consumerInputReport_ = nullptr;
  BLECharacteristic* bootInput_ = nullptr;
  BLEServerCallbacks* callbacks_ = nullptr;
  BLESecurity* security_ = nullptr;

  static constexpr uint32_t kAdvertiseTimeoutMs = 60000;
  static constexpr uint32_t kShutterReleaseDelayMs = 60;
  static constexpr uint32_t kShutterCooldownMs = 250;
  static constexpr uint32_t kPagerCooldownMs = 120;
};
