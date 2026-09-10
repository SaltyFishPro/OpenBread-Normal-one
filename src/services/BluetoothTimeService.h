#pragma once

#include <Arduino.h>

#include "TimeService.h"

class BLEAdvertisedDevice;
class BLEClient;
class BLEScan;
class BLEScanResults;

class BluetoothTimeService {
public:
  enum class State : uint8_t {
    Off,
    Scanning,
    Connecting,
    TimeReceived,
    Synced,
    Error
  };

  enum class Error : uint8_t {
    None,
    InitFailed,
    Timeout,
    ServiceNotFound,
    CharacteristicNotFound,
    ReadFailed,
    InvalidPayload,
    ApplyFailed
  };

  bool begin();
  void tick(uint32_t nowMs);

  bool start(uint32_t nowMs);
  void stop();
  void completeSync(bool applied);

  State state() const;
  Error error() const;
  const char* deviceName() const;
  bool isActive() const;
  bool consumeChanged();
  bool consumeReceivedTime(TimeService::DateTime& out);

private:
  void setState(State next, Error err = Error::None);
  bool initializeStack();
  void releaseBle();
  void processScanResults();
  bool tryReadCurrentTime(BLEAdvertisedDevice& device);
  bool parseCurrentTimePayload(const uint8_t* data, size_t len,
                               TimeService::DateTime& out) const;
  static void onScanComplete(BLEScanResults results);

  State state_ = State::Off;
  Error error_ = Error::None;
  bool changed_ = false;
  bool initialized_ = false;
  volatile bool scanComplete_ = false;
  uint32_t startMs_ = 0;
  char deviceName_[32] = {0};

  TimeService::DateTime receivedTime_{};
  bool receivedPending_ = false;

  BLEScan* scan_ = nullptr;
  BLEClient* client_ = nullptr;

  static constexpr uint32_t kScanDurationSeconds = 8;
  static constexpr uint32_t kTotalTimeoutMs = 15000;
  static BluetoothTimeService* activeInstance_;
};
