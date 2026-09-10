#include "BluetoothTimeService.h"

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>

#include <cstdarg>
#include <cstring>

namespace {
#ifndef OB_BT_TIME_LOG_ENABLED
#define OB_BT_TIME_LOG_ENABLED 1
#endif

constexpr uint16_t kCurrentTimeServiceUuid = 0x1805;
constexpr uint16_t kCurrentTimeCharacteristicUuid = 0x2A2B;

void btTimeLog(const char* fmt, ...) {
#if OB_BT_TIME_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[BT_TIME] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void btTimeErr(const char* fmt, ...) {
#if OB_BT_TIME_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[ERR][BT_TIME] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
}
}  // namespace

BluetoothTimeService* BluetoothTimeService::activeInstance_ = nullptr;

bool BluetoothTimeService::begin() {
  snprintf(deviceName_, sizeof(deviceName_), "BLE Current Time");
  setState(State::Off, Error::None);
  return true;
}

void BluetoothTimeService::tick(uint32_t nowMs) {
  if (state_ != State::Scanning && state_ != State::Connecting) {
    return;
  }

  if (scanComplete_ && state_ == State::Scanning) {
    processScanResults();
    return;
  }

  if ((nowMs - startMs_) >= kTotalTimeoutMs) {
    btTimeErr("cts sync timeout");
    releaseBle();
    setState(State::Error, Error::Timeout);
  }
}

bool BluetoothTimeService::start(uint32_t nowMs) {
  if (state_ == State::Scanning || state_ == State::Connecting || state_ == State::TimeReceived) {
    return false;
  }

  if (!initializeStack()) {
    btTimeErr("ble cts init failed");
    setState(State::Error, Error::InitFailed);
    return false;
  }

  startMs_ = nowMs;
  receivedPending_ = false;
  scanComplete_ = false;
  setState(State::Scanning, Error::None);
  btTimeLog("scan started for cts service=0x%04X", kCurrentTimeServiceUuid);

  activeInstance_ = this;
  if (!scan_->start(kScanDurationSeconds, BluetoothTimeService::onScanComplete, false)) {
    btTimeErr("cts scan failed");
    activeInstance_ = nullptr;
    releaseBle();
    setState(State::Error, Error::ServiceNotFound);
    return true;
  }

  return true;
}

void BluetoothTimeService::stop() {
  if (state_ == State::Off && !initialized_) {
    return;
  }
  releaseBle();
  receivedPending_ = false;
  setState(State::Off, Error::None);
}

void BluetoothTimeService::completeSync(bool applied) {
  releaseBle();
  receivedPending_ = false;
  if (applied) {
    btTimeLog("cts time sync applied");
    setState(State::Synced, Error::None);
  } else {
    btTimeErr("cts time sync apply failed");
    setState(State::Error, Error::ApplyFailed);
  }
}

BluetoothTimeService::State BluetoothTimeService::state() const { return state_; }

BluetoothTimeService::Error BluetoothTimeService::error() const { return error_; }

const char* BluetoothTimeService::deviceName() const { return deviceName_; }

bool BluetoothTimeService::isActive() const {
  return state_ == State::Scanning || state_ == State::Connecting ||
         state_ == State::TimeReceived;
}

bool BluetoothTimeService::consumeChanged() {
  const bool wasChanged = changed_;
  changed_ = false;
  return wasChanged;
}

bool BluetoothTimeService::consumeReceivedTime(TimeService::DateTime& out) {
  if (!receivedPending_) {
    return false;
  }
  out = receivedTime_;
  receivedPending_ = false;
  return true;
}

void BluetoothTimeService::setState(State next, Error err) {
  if (state_ == next && error_ == err) {
    return;
  }
  state_ = next;
  error_ = err;
  changed_ = true;
}

bool BluetoothTimeService::initializeStack() {
  if (initialized_) {
    return true;
  }

  BLEDevice::init("");

  if (scan_ == nullptr) {
    scan_ = BLEDevice::getScan();
  }
  if (scan_ == nullptr) {
    releaseBle();
    return false;
  }

  scan_->setActiveScan(true);
  scan_->setInterval(80);
  scan_->setWindow(60);
  if (client_ == nullptr) {
    client_ = BLEDevice::createClient();
  }
  if (client_ == nullptr) {
    releaseBle();
    return false;
  }

  initialized_ = true;
  btTimeLog("ble cts client initialized");
  return true;
}

void BluetoothTimeService::releaseBle() {
  if (!initialized_) {
    if (activeInstance_ == this) {
      activeInstance_ = nullptr;
    }
    return;
  }

  if (scan_ != nullptr && scan_->isScanning()) {
    scan_->stop();
  }
  if (scan_ != nullptr) {
    scan_->clearResults();
  }
  if (client_ != nullptr && client_->isConnected()) {
    client_->disconnect();
  }

  initialized_ = false;
  scanComplete_ = false;
  if (activeInstance_ == this) {
    activeInstance_ = nullptr;
  }
  btTimeLog("ble cts session stopped, stack kept");
}

void BluetoothTimeService::processScanResults() {
  if (scan_ == nullptr) {
    releaseBle();
    setState(State::Error, Error::ServiceNotFound);
    return;
  }

  BLEScanResults* results = scan_->getResults();
  const int count = (results == nullptr) ? 0 : results->getCount();
  btTimeLog("scan complete devices=%d", count);
  for (int i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results->getDevice(static_cast<uint32_t>(i));
    if (!device.isAdvertisingService(BLEUUID(kCurrentTimeServiceUuid))) {
      continue;
    }
    btTimeLog("cts device found addr=%s name=%s rssi=%d", device.getAddress().toString().c_str(),
              device.haveName() ? device.getName().c_str() : "", device.getRSSI());
    setState(State::Connecting, Error::None);
    if (tryReadCurrentTime(device)) {
      return;
    }
    return;
  }

  releaseBle();
  btTimeErr("cts service not found");
  setState(State::Error, Error::ServiceNotFound);
}

bool BluetoothTimeService::tryReadCurrentTime(BLEAdvertisedDevice& device) {
  if (client_ == nullptr) {
    return false;
  }

  if (!client_->connectTimeout(&device, 5000)) {
    btTimeErr("cts connect failed");
    releaseBle();
    setState(State::Error, Error::Timeout);
    return false;
  }

  BLERemoteService* service = client_->getService(BLEUUID(kCurrentTimeServiceUuid));
  if (service == nullptr) {
    btTimeErr("cts service disappeared");
    releaseBle();
    setState(State::Error, Error::ServiceNotFound);
    return false;
  }

  BLERemoteCharacteristic* characteristic =
      service->getCharacteristic(BLEUUID(kCurrentTimeCharacteristicUuid));
  if (characteristic == nullptr || !characteristic->canRead()) {
    btTimeErr("cts characteristic unavailable");
    releaseBle();
    setState(State::Error, Error::CharacteristicNotFound);
    return false;
  }

  const String value = characteristic->readValue();
  if (value.length() == 0) {
    btTimeErr("cts read failed");
    releaseBle();
    setState(State::Error, Error::ReadFailed);
    return false;
  }

  TimeService::DateTime parsed;
  if (!parseCurrentTimePayload(reinterpret_cast<const uint8_t*>(value.c_str()), value.length(),
                               parsed)) {
    btTimeErr("cts invalid payload len=%u", static_cast<unsigned>(value.length()));
    releaseBle();
    setState(State::Error, Error::InvalidPayload);
    return false;
  }

  receivedTime_ = parsed;
  receivedPending_ = true;
  btTimeLog("cts time received %04u-%02u-%02u %02u:%02u:%02u",
            static_cast<unsigned>(parsed.year), static_cast<unsigned>(parsed.month),
            static_cast<unsigned>(parsed.day), static_cast<unsigned>(parsed.hour),
            static_cast<unsigned>(parsed.minute), static_cast<unsigned>(parsed.second));
  setState(State::TimeReceived, Error::None);
  return true;
}

bool BluetoothTimeService::parseCurrentTimePayload(const uint8_t* data, size_t len,
                                                   TimeService::DateTime& out) const {
  if (data == nullptr || len < 7U) {
    return false;
  }

  out.year = readLe16(data);
  out.month = data[2];
  out.day = data[3];
  out.hour = data[4];
  out.minute = data[5];
  out.second = data[6];
  out.weekday = 0;
  return true;
}

void BluetoothTimeService::onScanComplete(BLEScanResults /*results*/) {
  if (activeInstance_ == nullptr) {
    return;
  }
  activeInstance_->scanComplete_ = true;
}
