#pragma once

#include <Arduino.h>

#include "../bsp/RtcDriver.h"

class TimeService {
public:
  enum class SyncSource : uint8_t {
    None,
    Bluetooth,
    Ntp
  };

  enum class SyncState : uint8_t {
    Idle,
    Syncing,
    Success,
    Failed
  };

  enum class Error : uint8_t {
    None,
    InvalidRtc,
    NoRtc,
    NoWifiCredentials,
    WifiConnectFailed,
    NtpTimeout,
    RtcWriteFailed,
    InvalidInput,
    Busy
  };

  struct DateTime {
    uint16_t year = 2026;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t weekday = 4;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
  };

  struct Snapshot {
    DateTime now{};
    bool valid = false;
    SyncSource lastSource = SyncSource::None;
    SyncState syncState = SyncState::Idle;
    Error error = Error::None;
    uint32_t lastSyncEpoch = 0;
  };

  bool begin(RtcDriver& rtc);
  void tick(uint32_t nowMs);

  bool setBluetoothTime(const DateTime& dt);
  bool requestNtpSync(uint32_t nowMs, const char* ssid, const char* pass,
                      bool reuseConnectedStation = false);
  void cancelNtpSync();

  const Snapshot& snapshot() const;
  bool consumeChanged();

private:
  enum class SyncPhase : uint8_t {
    None,
    Connect,
    AwaitTime
  };

  static constexpr uint32_t kRtcPollIntervalMs = 1000;
  static constexpr uint32_t kWifiConnectTimeoutMs = 35000;
  static constexpr uint32_t kWifiConnectGraceMs = 12000;
  static constexpr uint32_t kNtpTimeoutMs = 15000;

  void setError(Error err);
  void setSyncState(SyncState state);
  void markChanged();
  void updateSnapshotTime(const DateTime& dt, bool valid);
  bool refreshFromRtc(bool logStartupRead = false);
  bool validateDateTime(const DateTime& dt) const;
  uint8_t daysInMonth(uint16_t year, uint8_t month) const;
  int32_t daysFromCivil(uint16_t year, uint8_t month, uint8_t day) const;
  void civilFromDays(int32_t z, uint16_t& year, uint8_t& month, uint8_t& day) const;
  uint8_t weekdayFromDays(int32_t z) const;
  uint32_t localDateTimeToEpoch(const DateTime& dt) const;
  DateTime epochToLocalDateTime(uint32_t epoch) const;
  void setSystemEpoch(uint32_t epoch) const;
  bool applyDateTime(const DateTime& dt, SyncSource source, uint32_t epoch);
  void loadPersistedState();
  void savePersistedState();
  void cleanupRadio();
  const char* wifiStatusText(uint8_t status) const;

  RtcDriver* rtc_ = nullptr;
  Snapshot snapshot_{};
  bool changed_ = false;
  uint32_t lastRtcReadMs_ = 0;
  bool persistedValid_ = false;
  SyncPhase syncPhase_ = SyncPhase::None;
  uint32_t syncStartMs_ = 0;
  char syncSsid_[33] = {0};
  char syncPass_[65] = {0};
};
