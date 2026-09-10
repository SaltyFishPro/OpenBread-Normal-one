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

  State state() const;
  Error error() const;
  const char* deviceName() const;
  bool isConnected() const;
  bool consumeChanged();

  // Internal server callbacks route through these hooks.
  void onConnected(uint16_t connId);
  void onDisconnected();

private:
  void setState(State next, Error err = Error::None);
  bool initializeStack();
  void cleanupStack();
  State state_ = State::Off;
  Error error_ = Error::None;
  bool changed_ = false;
  bool initialized_ = false;
  bool bleStackReady_ = false;
  bool shuttingDown_ = false;
  uint32_t advertiseStartMs_ = 0;
  uint32_t pendingReleaseMs_ = 0;
  uint32_t lastShutterMs_ = 0;
  bool shutterPressActive_ = false;
  uint16_t activeConnId_ = 0xFFFF;
  char deviceName_[32] = {0};

  BLEServer* server_ = nullptr;
  BLEHIDDevice* hid_ = nullptr;
  BLECharacteristic* inputReport_ = nullptr;
  BLECharacteristic* bootInput_ = nullptr;
  BLEServerCallbacks* callbacks_ = nullptr;
  BLESecurity* security_ = nullptr;

  static constexpr uint32_t kAdvertiseTimeoutMs = 60000;
  static constexpr uint32_t kShutterReleaseDelayMs = 60;
  static constexpr uint32_t kShutterCooldownMs = 250;
};
