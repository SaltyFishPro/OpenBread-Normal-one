#include "TimeService.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include <cstdarg>
#include <cstring>

namespace {
constexpr const char* kTimePrefsNs = "time";
constexpr const char* kTimeValidKey = "valid";
constexpr const char* kTimeSourceKey = "source";
constexpr const char* kTimeEpochKey = "last_sync";
constexpr uint32_t kMinValidEpoch = 1700000000UL;
constexpr int32_t kUtcOffsetSeconds = 8 * 3600;

#ifndef OB_TIME_LOG_ENABLED
#define OB_TIME_LOG_ENABLED 1
#endif

void timeLog(const char* fmt, ...) {
#if OB_TIME_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[TIME] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void timeErr(const char* fmt, ...) {
#if OB_TIME_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[ERR][TIME] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}
}  // namespace

bool TimeService::begin(RtcDriver& rtc) {
  rtc_ = &rtc;
  snapshot_ = Snapshot{};
  changed_ = false;
  lastRtcReadMs_ = 0;
  syncPhase_ = SyncPhase::None;
  syncStartMs_ = 0;
  memset(syncSsid_, 0, sizeof(syncSsid_));
  memset(syncPass_, 0, sizeof(syncPass_));
  loadPersistedState();

  if (rtc_ == nullptr || !rtc_->isAvailable()) {
    persistedValid_ = false;
    snapshot_.valid = false;
    snapshot_.error = Error::NoRtc;
    timeErr("rtc unavailable at startup");
    changed_ = true;
    return true;
  }

  if (!refreshFromRtc(true)) {
    snapshot_.valid = false;
    snapshot_.error = Error::InvalidRtc;
  }
  changed_ = true;
  return true;
}

void TimeService::tick(uint32_t nowMs) {
  if (syncPhase_ == SyncPhase::Connect) {
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      esp_sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      syncPhase_ = SyncPhase::AwaitTime;
      syncStartMs_ = nowMs;
      timeLog("wifi connected for ntp, awaiting time");
      return;
    }

    if ((nowMs - syncStartMs_) < kWifiConnectTimeoutMs) {
      return;
    }
    if ((nowMs - syncStartMs_) < (kWifiConnectTimeoutMs + kWifiConnectGraceMs)) {
      return;
    }

    timeErr("wifi connect failed status=%s", wifiStatusText(status));
    cleanupRadio();
    syncPhase_ = SyncPhase::None;
    setSyncState(SyncState::Failed);
    setError(Error::WifiConnectFailed);
    return;
  }

  if (syncPhase_ == SyncPhase::AwaitTime) {
    const bool synchronized = esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
    // Read the epoch after observing completion, so an arriving NTP reply cannot
    // make an earlier, stale epoch look like the newly synchronized time.
    const time_t epoch = time(nullptr);
    if (synchronized && epoch >= static_cast<time_t>(kMinValidEpoch)) {
      const DateTime local = epochToLocalDateTime(static_cast<uint32_t>(epoch));
      if (!applyDateTime(local, SyncSource::Ntp, static_cast<uint32_t>(epoch))) {
        setSyncState(SyncState::Failed);
      } else {
        setSyncState(SyncState::Success);
        setError(Error::None);
        timeLog("ntp sync success epoch=%lu", static_cast<unsigned long>(epoch));
      }
      cleanupRadio();
      syncPhase_ = SyncPhase::None;
      return;
    }

    if ((nowMs - syncStartMs_) >= kNtpTimeoutMs) {
      timeErr("ntp sync timeout");
      cleanupRadio();
      syncPhase_ = SyncPhase::None;
      setSyncState(SyncState::Failed);
      setError(Error::NtpTimeout);
      return;
    }
    return;
  }

  if ((nowMs - lastRtcReadMs_) < kRtcPollIntervalMs) {
    return;
  }
  lastRtcReadMs_ = nowMs;
  (void)refreshFromRtc();
}

bool TimeService::requestNtpSync(uint32_t nowMs, const char* ssid, const char* pass,
                                 bool reuseConnectedStation) {
  if (syncPhase_ != SyncPhase::None) {
    setError(Error::Busy);
    return false;
  }
  if (rtc_ == nullptr || !rtc_->isAvailable()) {
    setSyncState(SyncState::Failed);
    setError(Error::NoRtc);
    return false;
  }
  if (ssid == nullptr || ssid[0] == '\0') {
    setSyncState(SyncState::Failed);
    setError(Error::NoWifiCredentials);
    return false;
  }

  memset(syncSsid_, 0, sizeof(syncSsid_));
  memset(syncPass_, 0, sizeof(syncPass_));
  strncpy(syncSsid_, ssid, sizeof(syncSsid_) - 1U);
  if (pass != nullptr) {
    strncpy(syncPass_, pass, sizeof(syncPass_) - 1U);
  }

  // The provisioning service explicitly hands over its newly connected station.
  if (reuseConnectedStation && WiFi.status() == WL_CONNECTED) {
    timeLog("reusing provisioned wifi for ntp");
  } else {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(syncSsid_, syncPass_);
  }

  syncPhase_ = SyncPhase::Connect;
  syncStartMs_ = nowMs;
  setSyncState(SyncState::Syncing);
  setError(Error::None);
  timeLog("ntp sync requested ssid=%s", syncSsid_);
  return true;
}

void TimeService::cancelNtpSync() {
  if (syncPhase_ == SyncPhase::None) {
    return;
  }
  cleanupRadio();
  syncPhase_ = SyncPhase::None;
  setSyncState(SyncState::Idle);
  setError(Error::None);
  timeLog("ntp sync canceled");
}

const TimeService::Snapshot& TimeService::snapshot() const { return snapshot_; }

bool TimeService::consumeChanged() {
  const bool wasChanged = changed_;
  changed_ = false;
  return wasChanged;
}

void TimeService::setError(Error err) {
  if (snapshot_.error == err) {
    return;
  }
  snapshot_.error = err;
  markChanged();
}

void TimeService::setSyncState(SyncState state) {
  if (snapshot_.syncState == state) {
    return;
  }
  snapshot_.syncState = state;
  markChanged();
}

void TimeService::markChanged() { changed_ = true; }

void TimeService::updateSnapshotTime(const DateTime& dt, bool valid) {
  const bool validityChanged = (snapshot_.valid != valid);
  // 秒不参与任何界面的时钟显示（自检页的秒显示走 RtcTestService），
  // 若把秒变化也算作 changed，会造成每秒一次整屏重绘。
  const bool displayChanged =
      validityChanged || snapshot_.now.year != dt.year || snapshot_.now.month != dt.month ||
      snapshot_.now.day != dt.day || snapshot_.now.weekday != dt.weekday ||
      snapshot_.now.hour != dt.hour || snapshot_.now.minute != dt.minute;

  if (validityChanged) {
    timeLog("validity changed %u->%u persisted=%u",
            snapshot_.valid ? 1U : 0U, valid ? 1U : 0U, persistedValid_ ? 1U : 0U);
  }

  snapshot_.now = dt;
  snapshot_.valid = valid;

  if (displayChanged) {
    markChanged();
  }
}

bool TimeService::refreshFromRtc(bool logStartupRead) {
  if (rtc_ == nullptr || !rtc_->isAvailable()) {
    if (logStartupRead || snapshot_.valid || snapshot_.error != Error::NoRtc) {
      timeErr("rtc unavailable; time display disabled");
    }
    updateSnapshotTime(DateTime{}, false);
    setError(Error::NoRtc);
    return false;
  }

  RtcDriver::DateTime rtcNow;
  if (!rtc_->read(rtcNow)) {
    if (logStartupRead || snapshot_.valid || snapshot_.error != Error::InvalidRtc) {
      timeErr("rtc date/time read failed; check I2C bus and RTC power");
    }
    updateSnapshotTime(DateTime{}, false);
    setError(Error::InvalidRtc);
    return false;
  }

  DateTime dt;
  dt.second = rtcNow.second;
  dt.minute = rtcNow.minute;
  dt.hour = rtcNow.hour;
  dt.day = rtcNow.day;
  dt.weekday = rtcNow.weekday;
  dt.month = rtcNow.month;
  dt.year = static_cast<uint16_t>(2000U + rtcNow.year);

  if (logStartupRead || !snapshot_.valid) {
    timeLog("rtc read accepted %04u-%02u-%02u %02u:%02u:%02u weekday=%u "
            "policy=read_success clock_lost=%u persisted=%u source=%u",
            static_cast<unsigned>(dt.year), static_cast<unsigned>(dt.month),
            static_cast<unsigned>(dt.day), static_cast<unsigned>(dt.hour),
            static_cast<unsigned>(dt.minute), static_cast<unsigned>(dt.second),
            static_cast<unsigned>(dt.weekday),
            rtcNow.clockIntegrityLost ? 1U : 0U, persistedValid_ ? 1U : 0U,
            static_cast<unsigned>(snapshot_.lastSource));
  }

  if (!persistedValid_) {
    snapshot_.lastSource = SyncSource::None;
    snapshot_.lastSyncEpoch = 0;
  }

  // Display availability follows successful RTC reads, not calibration history
  // or clock integrity.
  updateSnapshotTime(dt, true);
  setError(Error::None);
  return true;
}

void TimeService::civilFromDays(int32_t z, uint16_t& year, uint8_t& month, uint8_t& day) const {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
  int32_t y = static_cast<int32_t>(yoe) + era * 400;
  const uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
  const uint32_t mp = (5U * doy + 2U) / 153U;
  const uint32_t d = doy - (153U * mp + 2U) / 5U + 1U;
  const uint32_t m = mp + (mp < 10U ? 3U : static_cast<uint32_t>(-9));
  y += (m <= 2U) ? 1 : 0;
  year = static_cast<uint16_t>(y);
  month = static_cast<uint8_t>(m);
  day = static_cast<uint8_t>(d);
}

uint8_t TimeService::weekdayFromDays(int32_t z) const {
  const int32_t weekday = (z + 4) % 7;
  return static_cast<uint8_t>(weekday < 0 ? weekday + 7 : weekday);
}

TimeService::DateTime TimeService::epochToLocalDateTime(uint32_t epoch) const {
  const uint32_t localEpoch = epoch + static_cast<uint32_t>(kUtcOffsetSeconds);
  const int32_t days = static_cast<int32_t>(localEpoch / 86400U);
  const uint32_t secondsOfDay = localEpoch % 86400U;

  DateTime dt;
  civilFromDays(days, dt.year, dt.month, dt.day);
  dt.weekday = weekdayFromDays(days);
  dt.hour = static_cast<uint8_t>(secondsOfDay / 3600U);
  dt.minute = static_cast<uint8_t>((secondsOfDay % 3600U) / 60U);
  dt.second = static_cast<uint8_t>(secondsOfDay % 60U);
  return dt;
}

void TimeService::setSystemEpoch(uint32_t epoch) const {
  timeval tv;
  tv.tv_sec = static_cast<time_t>(epoch);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

bool TimeService::applyDateTime(const DateTime& dt, SyncSource source, uint32_t epoch) {
  if (rtc_ == nullptr || !rtc_->isAvailable()) {
    setError(Error::NoRtc);
    return false;
  }

  RtcDriver::DateTime rtcWrite;
  rtcWrite.second = dt.second;
  rtcWrite.minute = dt.minute;
  rtcWrite.hour = dt.hour;
  rtcWrite.day = dt.day;
  rtcWrite.weekday = dt.weekday;
  rtcWrite.month = dt.month;
  rtcWrite.year = static_cast<uint8_t>(dt.year >= 2000U ? (dt.year - 2000U) : 0U);
  if (!rtc_->write(rtcWrite)) {
    setError(Error::RtcWriteFailed);
    timeErr("rtc write failed");
    return false;
  }

  persistedValid_ = true;
  snapshot_.lastSource = source;
  snapshot_.lastSyncEpoch = epoch;
  updateSnapshotTime(dt, true);
  setSystemEpoch(epoch);
  savePersistedState();
  lastRtcReadMs_ = millis();
  return true;
}

void TimeService::loadPersistedState() {
  Preferences prefs;
  if (!prefs.begin(kTimePrefsNs, true)) {
    persistedValid_ = false;
    return;
  }
  persistedValid_ = prefs.getBool(kTimeValidKey, false);
  const uint8_t source = prefs.getUChar(kTimeSourceKey, static_cast<uint8_t>(SyncSource::None));
  snapshot_.lastSource = source == static_cast<uint8_t>(SyncSource::Ntp)
                             ? SyncSource::Ntp
                             : SyncSource::None;
  snapshot_.lastSyncEpoch = prefs.getULong(kTimeEpochKey, 0);
  prefs.end();
}

void TimeService::savePersistedState() {
  Preferences prefs;
  if (!prefs.begin(kTimePrefsNs, false)) {
    return;
  }
  prefs.putBool(kTimeValidKey, persistedValid_);
  prefs.putUChar(kTimeSourceKey, static_cast<uint8_t>(snapshot_.lastSource));
  prefs.putULong(kTimeEpochKey, snapshot_.lastSyncEpoch);
  prefs.end();
}

void TimeService::cleanupRadio() {
  esp_sntp_stop();
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
  timeLog("ntp session finished, sntp stopped, radio off");
  memset(syncSsid_, 0, sizeof(syncSsid_));
  memset(syncPass_, 0, sizeof(syncPass_));
}

const char* TimeService::wifiStatusText(uint8_t status) const {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}
