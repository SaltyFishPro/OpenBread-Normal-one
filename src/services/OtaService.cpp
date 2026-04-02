#include "OtaService.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "../app/FirmwareInfo.h"

namespace {
constexpr const char* kPrefsNs = "wifi";
constexpr const char* kPrefsKeySsid = "ssid";
constexpr const char* kPrefsKeyPass = "pass";
constexpr const char* kOtaPrefsNs = "ota";
constexpr const char* kOtaExpectedBuild = "expect_b";
constexpr const char* kOtaExpectedVer = "expect_v";
constexpr uint32_t kHttpTimeoutMs = 5000;

#ifndef OB_OTA_LOG_ENABLED
#define OB_OTA_LOG_ENABLED 1
#endif

void otaLog(const char* fmt, ...) {
#if OB_OTA_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[OTA] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

void otaErr(const char* fmt, ...) {
#if OB_OTA_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[ERR][OTA] ");
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

bool OtaService::begin() {
  memset(staSsid_, 0, sizeof(staSsid_));
  memset(staPass_, 0, sizeof(staPass_));
  memset(&manifest_, 0, sizeof(manifest_));
  lastHttpStatus_ = 0;
  clearProgress();
  phase_ = CheckPhase::None;
  pendingAction_ = Action::None;
  hasPostApplyResult_ = false;
  postApplySucceeded_ = false;
  memset(postApplyMessage_, 0, sizeof(postApplyMessage_));
  loadAndVerifyPostApplyResult();
  setState(State::Idle, Error::None);
  return true;
}

void OtaService::tick(uint32_t nowMs) {
  if (phase_ == CheckPhase::None) {
    return;
  }

  if (phase_ == CheckPhase::Connect) {
    const wl_status_t wifiStatus = WiFi.status();
    if ((nowMs - lastConnectLogMs_) >= 1000U) {
      lastConnectLogMs_ = nowMs;
      const IPAddress ip = WiFi.localIP();
      otaLog("connect wait status=%s ip=%u.%u.%u.%u rssi=%d",
             wifiStatusText(wifiStatus), ip[0], ip[1], ip[2], ip[3],
             static_cast<int>(WiFi.RSSI()));
    }

    if (wifiStatus == WL_CONNECTED || hasStaIp()) {
      const IPAddress ip = WiFi.localIP();
      otaLog("wifi connected for %s, ip=%u.%u.%u.%u",
             pendingAction_ == Action::DownloadFirmware ? "download" : "manifest check", ip[0],
             ip[1], ip[2], ip[3]);
      phase_ = (pendingAction_ == Action::DownloadFirmware) ? CheckPhase::DownloadFirmware
                                                            : CheckPhase::FetchManifest;
      actionStartMs_ = nowMs;
      return;
    }

    if ((nowMs - actionStartMs_) < kConnectTimeoutMs) {
      return;
    }

    if ((nowMs - actionStartMs_) < (kConnectTimeoutMs + kConnectGraceMs)) {
      return;
    }

    otaErr("wifi connect timeout status=%s", wifiStatusText(wifiStatus));
    cleanupRadio();
    phase_ = CheckPhase::None;
    pendingAction_ = Action::None;
    setState(State::Error, Error::ConnectTimeout);
    return;
  }

  if (phase_ != CheckPhase::FetchManifest) {
    if (phase_ == CheckPhase::DownloadFirmware) {
      if (downloadAndVerifyFirmware()) {
        setState(State::ReadyToApply, Error::None);
      }
      cleanupRadio();
      phase_ = CheckPhase::None;
      pendingAction_ = Action::None;
      return;
    }

    otaErr("invalid check phase");
    cleanupRadio();
    phase_ = CheckPhase::None;
    pendingAction_ = Action::None;
    setState(State::Error, Error::Unknown);
    return;
  }

  if (fetchAndValidateManifest()) {
    if (manifest_.build > FirmwareInfo::kBuild) {
      otaLog("update available local=%lu remote=%lu version=%s",
             static_cast<unsigned long>(FirmwareInfo::kBuild),
             static_cast<unsigned long>(manifest_.build), manifest_.version);
      setState(State::UpdateAvailable, Error::None);
    } else {
      otaLog("already up-to-date local=%lu remote=%lu",
             static_cast<unsigned long>(FirmwareInfo::kBuild),
             static_cast<unsigned long>(manifest_.build));
      setState(State::UpToDate, Error::None);
    }
  }

  cleanupRadio();
  phase_ = CheckPhase::None;
  pendingAction_ = Action::None;
}

bool OtaService::requestCheck(uint32_t nowMs) {
  if (phase_ != CheckPhase::None) {
    otaLog("check request ignored: already checking");
    return false;
  }

  if (!loadSavedCredentials()) {
    otaErr("no stored wifi credentials");
    setState(State::Error, Error::NoCredentials);
    return false;
  }

  memset(&manifest_, 0, sizeof(manifest_));
  lastHttpStatus_ = 0;
  clearProgress();

  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(staSsid_, staPass_);

  otaLog("checking manifest url=%s", FirmwareInfo::kManifestUrl);
  otaLog("connect ssid=%s", staSsid_);

  actionStartMs_ = nowMs;
  lastConnectLogMs_ = nowMs;
  pendingAction_ = Action::CheckManifest;
  phase_ = CheckPhase::Connect;
  setState(State::Checking, Error::None);
  return true;
}

bool OtaService::requestDownload(uint32_t nowMs) {
  if (phase_ != CheckPhase::None) {
    otaLog("download request ignored: busy");
    return false;
  }

  if (state_ != State::UpdateAvailable && state_ != State::ReadyToApply) {
    otaErr("download request rejected: no available update state=%u",
           static_cast<unsigned>(state_));
    setState(State::Error, Error::VersionNotNew);
    return false;
  }

  if (manifest_.firmwareUrl[0] == '\0' || manifest_.size == 0 || manifest_.sha256[0] == '\0') {
    otaErr("download request rejected: manifest not ready");
    setState(State::Error, Error::ManifestInvalid);
    return false;
  }

  if (!loadSavedCredentials()) {
    otaErr("download request rejected: no wifi credentials");
    setState(State::Error, Error::NoCredentials);
    return false;
  }

  lastHttpStatus_ = 0;
  clearProgress();
  totalBytes_ = manifest_.size;

  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(staSsid_, staPass_);

  otaLog("download start url=%s size=%lu", manifest_.firmwareUrl,
         static_cast<unsigned long>(manifest_.size));
  otaLog("connect ssid=%s", staSsid_);

  actionStartMs_ = nowMs;
  lastConnectLogMs_ = nowMs;
  pendingAction_ = Action::DownloadFirmware;
  phase_ = CheckPhase::Connect;
  setState(State::Downloading, Error::None);
  return true;
}

bool OtaService::requestApply() {
  if (state_ != State::ReadyToApply) {
    otaErr("apply request rejected: state=%u", static_cast<unsigned>(state_));
    setState(State::Error, Error::ApplyNotReady);
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kOtaPrefsNs, false)) {
    otaErr("failed to open ota prefs");
    setState(State::Error, Error::Unknown);
    return false;
  }
  prefs.putULong(kOtaExpectedBuild, manifest_.build);
  prefs.putString(kOtaExpectedVer, manifest_.version);
  prefs.end();

  setState(State::Applying, Error::None);
  otaLog("apply confirmed, restart to boot new firmware target_build=%lu target_ver=%s",
         static_cast<unsigned long>(manifest_.build), manifest_.version);
  delay(100);
  ESP.restart();
  return true;
}

void OtaService::cancel() {
  if (phase_ == CheckPhase::None) {
    return;
  }
  otaLog("ota action canceled by user");
  cleanupRadio();
  phase_ = CheckPhase::None;
  pendingAction_ = Action::None;
  setState(State::Idle, Error::None);
}

OtaService::State OtaService::state() const { return state_; }

OtaService::Error OtaService::error() const { return error_; }

const OtaService::ManifestInfo& OtaService::manifest() const { return manifest_; }

int OtaService::lastHttpStatus() const { return lastHttpStatus_; }

uint32_t OtaService::downloadedBytes() const { return downloadedBytes_; }

uint32_t OtaService::totalBytes() const { return totalBytes_; }

uint8_t OtaService::progressPercent() const {
  if (totalBytes_ == 0) {
    return 0;
  }
  const uint32_t pct = static_cast<uint32_t>((static_cast<uint64_t>(downloadedBytes_) * 100ULL) /
                                             static_cast<uint64_t>(totalBytes_));
  return static_cast<uint8_t>(pct > 100 ? 100 : pct);
}

bool OtaService::hasPostApplyResult() const { return hasPostApplyResult_; }

bool OtaService::postApplySucceeded() const { return postApplySucceeded_; }

const char* OtaService::postApplyMessage() const { return postApplyMessage_; }

void OtaService::clearPostApplyResult() {
  hasPostApplyResult_ = false;
  postApplySucceeded_ = false;
  memset(postApplyMessage_, 0, sizeof(postApplyMessage_));
}

bool OtaService::consumeChanged() {
  const bool value = changed_;
  changed_ = false;
  return value;
}

void OtaService::setState(State next, Error err) {
  if (state_ == next && error_ == err) {
    return;
  }
  state_ = next;
  error_ = err;
  changed_ = true;
}

bool OtaService::loadSavedCredentials() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, true)) {
    return false;
  }
  const String ssid = prefs.getString(kPrefsKeySsid, "");
  const String pass = prefs.getString(kPrefsKeyPass, "");
  prefs.end();

  memset(staSsid_, 0, sizeof(staSsid_));
  memset(staPass_, 0, sizeof(staPass_));
  ssid.toCharArray(staSsid_, sizeof(staSsid_));
  pass.toCharArray(staPass_, sizeof(staPass_));

  return staSsid_[0] != '\0';
}

void OtaService::cleanupRadio() {
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
}

void OtaService::clearProgress() {
  downloadedBytes_ = 0;
  totalBytes_ = 0;
  lastProgressStep_ = 0;
}

void OtaService::loadAndVerifyPostApplyResult() {
  Preferences prefs;
  if (!prefs.begin(kOtaPrefsNs, false)) {
    return;
  }

  const uint32_t expectedBuild = prefs.getULong(kOtaExpectedBuild, 0);
  const String expectedVer = prefs.getString(kOtaExpectedVer, "");
  if (expectedBuild == 0) {
    prefs.end();
    return;
  }

  const bool ok = FirmwareInfo::kBuild >= expectedBuild;
  hasPostApplyResult_ = true;
  postApplySucceeded_ = ok;

  if (ok) {
    snprintf(postApplyMessage_, sizeof(postApplyMessage_), "OTA applied: %s (%lu)",
             FirmwareInfo::kVersion, static_cast<unsigned long>(FirmwareInfo::kBuild));
    otaLog("post-apply verify success expected=%lu/%s actual=%lu/%s",
           static_cast<unsigned long>(expectedBuild), expectedVer.c_str(),
           static_cast<unsigned long>(FirmwareInfo::kBuild), FirmwareInfo::kVersion);
  } else {
    snprintf(postApplyMessage_, sizeof(postApplyMessage_), "OTA verify failed exp=%lu got=%lu",
             static_cast<unsigned long>(expectedBuild),
             static_cast<unsigned long>(FirmwareInfo::kBuild));
    otaErr("post-apply verify failed expected=%lu/%s actual=%lu/%s",
           static_cast<unsigned long>(expectedBuild), expectedVer.c_str(),
           static_cast<unsigned long>(FirmwareInfo::kBuild), FirmwareInfo::kVersion);
  }

  prefs.remove(kOtaExpectedBuild);
  prefs.remove(kOtaExpectedVer);
  prefs.end();
}

bool OtaService::fetchAndValidateManifest() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  const char* trackedHeaders[] = {"Date",         "Server",      "ETag",
                                  "Cache-Control", "Last-Modified",
                                  "CF-Cache-Status", "Content-Length",
                                  "Content-Type"};
  http.collectHeaders(trackedHeaders,
                      sizeof(trackedHeaders) / sizeof(trackedHeaders[0]));
  if (!http.begin(client, FirmwareInfo::kManifestUrl)) {
    otaErr("http begin failed");
    setState(State::Error, Error::HttpRequestFailed);
    return false;
  }

  const int httpCode = http.GET();
  lastHttpStatus_ = httpCode;
  if (httpCode < 0) {
    otaErr("manifest http transport error code=%d msg=%s", httpCode,
           HTTPClient::errorToString(httpCode).c_str());
  } else {
    otaLog("manifest http code=%d", httpCode);
  }

  for (size_t i = 0; i < sizeof(trackedHeaders) / sizeof(trackedHeaders[0]); ++i) {
    const String value = http.header(trackedHeaders[i]);
    if (!value.isEmpty()) {
      otaLog("manifest header %s: %s", trackedHeaders[i], value.c_str());
    }
  }

  if (httpCode != HTTP_CODE_OK) {
    if (httpCode == HTTP_CODE_NOT_MODIFIED) {
      otaErr("manifest returned 304 (Not Modified); OTA client requires 200 body");
    } else if (httpCode > 0) {
      otaErr("manifest unexpected http status=%d", httpCode);
    }
    http.end();
    setState(State::Error, (httpCode < 0) ? Error::HttpRequestFailed
                                          : Error::HttpStatusInvalid);
    return false;
  }

  const String payload = http.getString();
  http.end();

  ManifestInfo parsed;
  if (!parseStringField(payload.c_str(), "product", parsed.product,
                        sizeof(parsed.product)) ||
      !parseStringField(payload.c_str(), "channel", parsed.channel,
                        sizeof(parsed.channel)) ||
      !parseStringField(payload.c_str(), "version", parsed.version,
                        sizeof(parsed.version)) ||
      !parseUintField(payload.c_str(), "build", parsed.build) ||
      !parseStringField(payload.c_str(), "firmware_url", parsed.firmwareUrl,
                        sizeof(parsed.firmwareUrl)) ||
      !parseStringField(payload.c_str(), "sha256", parsed.sha256,
                        sizeof(parsed.sha256)) ||
      !parseUintField(payload.c_str(), "size", parsed.size) ||
      !parseStringField(payload.c_str(), "release_note", parsed.releaseNote,
                        sizeof(parsed.releaseNote))) {
    otaErr("manifest parse/field validation failed");
    setState(State::Error, Error::ManifestInvalid);
    return false;
  }

  if (strcmp(parsed.product, FirmwareInfo::kProduct) != 0) {
    otaErr("product mismatch remote=%s local=%s", parsed.product,
           FirmwareInfo::kProduct);
    setState(State::Error, Error::ProductMismatch);
    return false;
  }

  if (strcmp(parsed.channel, FirmwareInfo::kChannel) != 0) {
    otaErr("channel mismatch remote=%s local=%s", parsed.channel,
           FirmwareInfo::kChannel);
    setState(State::Error, Error::ChannelMismatch);
    return false;
  }

  if (!isHttpsUrl(parsed.firmwareUrl)) {
    otaErr("firmware url is not https: %s", parsed.firmwareUrl);
    setState(State::Error, Error::UrlInvalid);
    return false;
  }

  if (!isHexString64(parsed.sha256)) {
    otaErr("sha256 invalid format");
    setState(State::Error, Error::ShaInvalid);
    return false;
  }

  if (parsed.size == 0 || parsed.size > kMaxImageSize) {
    otaErr("image size invalid=%lu", static_cast<unsigned long>(parsed.size));
    setState(State::Error, Error::SizeInvalid);
    return false;
  }

  manifest_ = parsed;
  otaLog("manifest parsed build=%lu size=%lu sha=%c%c%c%c%c%c%c%c...",
         static_cast<unsigned long>(manifest_.build),
         static_cast<unsigned long>(manifest_.size), manifest_.sha256[0],
         manifest_.sha256[1], manifest_.sha256[2], manifest_.sha256[3],
         manifest_.sha256[4], manifest_.sha256[5], manifest_.sha256[6],
         manifest_.sha256[7]);
  return true;
}

bool OtaService::downloadAndVerifyFirmware() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(client, manifest_.firmwareUrl)) {
    otaErr("firmware http begin failed");
    setState(State::Error, Error::DownloadFailed);
    return false;
  }

  const int httpCode = http.GET();
  lastHttpStatus_ = httpCode;
  if (httpCode != HTTP_CODE_OK) {
    otaErr("firmware http status=%d", httpCode);
    http.end();
    setState(State::Error, (httpCode < 0) ? Error::DownloadFailed : Error::HttpStatusInvalid);
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength > 0 && static_cast<uint32_t>(contentLength) != manifest_.size) {
    otaErr("firmware size mismatch header=%d manifest=%lu", contentLength,
           static_cast<unsigned long>(manifest_.size));
    http.end();
    setState(State::Error, Error::SizeInvalid);
    return false;
  }

  if (!Update.begin(manifest_.size, U_FLASH)) {
    otaErr("update begin failed err=%u", static_cast<unsigned>(Update.getError()));
    http.end();
    setState(State::Error, Error::UpdateBeginFailed);
    return false;
  }

  setState(State::Downloading, Error::None);
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t remaining = manifest_.size;

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);

  clearProgress();
  totalBytes_ = manifest_.size;

  while (remaining > 0) {
    const int available = stream->available();
    if (available <= 0) {
      if (!http.connected()) {
        otaErr("download interrupted at %lu/%lu", static_cast<unsigned long>(downloadedBytes_),
               static_cast<unsigned long>(manifest_.size));
        Update.abort();
        mbedtls_sha256_free(&shaCtx);
        http.end();
        setState(State::Error, Error::DownloadFailed);
        return false;
      }
      delay(1);
      continue;
    }

    size_t toRead = static_cast<size_t>(available);
    if (toRead > sizeof(buf)) {
      toRead = sizeof(buf);
    }
    if (toRead > remaining) {
      toRead = remaining;
    }

    const int readLen = stream->readBytes(buf, toRead);
    if (readLen <= 0) {
      otaErr("download read failed");
      Update.abort();
      mbedtls_sha256_free(&shaCtx);
      http.end();
      setState(State::Error, Error::DownloadFailed);
      return false;
    }

    if (Update.write(buf, static_cast<size_t>(readLen)) != static_cast<size_t>(readLen)) {
      otaErr("update write failed err=%u", static_cast<unsigned>(Update.getError()));
      Update.abort();
      mbedtls_sha256_free(&shaCtx);
      http.end();
      setState(State::Error, Error::UpdateWriteFailed);
      return false;
    }

    mbedtls_sha256_update(&shaCtx, buf, static_cast<size_t>(readLen));
    downloadedBytes_ += static_cast<uint32_t>(readLen);
    remaining -= static_cast<size_t>(readLen);

    const uint8_t step = static_cast<uint8_t>(progressPercent() / 10U);
    if (step > lastProgressStep_) {
      lastProgressStep_ = step;
      otaLog("download progress=%u%% (%lu/%lu)", static_cast<unsigned>(progressPercent()),
             static_cast<unsigned long>(downloadedBytes_),
             static_cast<unsigned long>(manifest_.size));
    }
    yield();
  }

  setState(State::Verifying, Error::None);
  uint8_t hash[32] = {0};
  char hashHex[65] = {0};
  mbedtls_sha256_finish(&shaCtx, hash);
  mbedtls_sha256_free(&shaCtx);

  if (!toHexSha256(hash, sizeof(hash), hashHex, sizeof(hashHex))) {
    otaErr("sha256 hex conversion failed");
    Update.abort();
    http.end();
    setState(State::Error, Error::ShaInvalid);
    return false;
  }

  if (strcasecmp(hashHex, manifest_.sha256) != 0) {
    otaErr("sha256 mismatch expected=%s actual=%s", manifest_.sha256, hashHex);
    Update.abort();
    http.end();
    setState(State::Error, Error::ShaMismatch);
    return false;
  }

  if (!Update.end(false)) {
    otaErr("update finalize failed err=%u", static_cast<unsigned>(Update.getError()));
    http.end();
    setState(State::Error, Error::UpdateFinalizeFailed);
    return false;
  }

  otaLog("firmware stored and verified, ready to apply on reboot");
  http.end();
  return true;
}

bool OtaService::parseStringField(const char* json, const char* key, char* out,
                                  size_t outLen) const {
  if (json == nullptr || key == nullptr || out == nullptr || outLen == 0) {
    return false;
  }

  char token[40];
  const int n = snprintf(token, sizeof(token), "\"%s\"", key);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(token)) {
    return false;
  }

  const char* keyPos = strstr(json, token);
  if (keyPos == nullptr) {
    return false;
  }

  const char* colon = strchr(keyPos + n, ':');
  if (colon == nullptr) {
    return false;
  }

  const char* valueStart = colon + 1;
  while (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\n' ||
         *valueStart == '\r') {
    ++valueStart;
  }

  if (*valueStart != '"') {
    return false;
  }
  ++valueStart;

  size_t i = 0;
  while (*valueStart != '\0' && *valueStart != '"') {
    if (*valueStart == '\\') {
      return false;
    }
    if (i + 1 >= outLen) {
      return false;
    }
    out[i++] = *valueStart++;
  }

  if (*valueStart != '"') {
    return false;
  }

  out[i] = '\0';
  return i > 0;
}

bool OtaService::parseUintField(const char* json, const char* key, uint32_t& out) const {
  char token[40];
  const int n = snprintf(token, sizeof(token), "\"%s\"", key);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(token)) {
    return false;
  }

  const char* keyPos = strstr(json, token);
  if (keyPos == nullptr) {
    return false;
  }

  const char* colon = strchr(keyPos + n, ':');
  if (colon == nullptr) {
    return false;
  }

  const char* valueStart = colon + 1;
  while (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\n' ||
         *valueStart == '\r') {
    ++valueStart;
  }

  if (!isdigit(static_cast<unsigned char>(*valueStart))) {
    return false;
  }

  char* endPtr = nullptr;
  const unsigned long value = strtoul(valueStart, &endPtr, 10);
  if (endPtr == valueStart || value > 0xFFFFFFFFUL) {
    return false;
  }

  out = static_cast<uint32_t>(value);
  return true;
}

bool OtaService::isHexString64(const char* value) const {
  if (value == nullptr) {
    return false;
  }

  for (uint8_t i = 0; i < 64; ++i) {
    const char c = value[i];
    if (c == '\0') {
      return false;
    }
    if (!isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }

  return value[64] == '\0';
}

bool OtaService::isHttpsUrl(const char* value) const {
  if (value == nullptr) {
    return false;
  }
  return strncmp(value, "https://", 8) == 0;
}

bool OtaService::toHexSha256(const uint8_t* hash, size_t hashLen, char* out, size_t outLen) const {
  if (hash == nullptr || out == nullptr || outLen < (hashLen * 2U + 1U)) {
    return false;
  }

  static const char* kHex = "0123456789abcdef";
  for (size_t i = 0; i < hashLen; ++i) {
    out[i * 2U] = kHex[(hash[i] >> 4U) & 0x0F];
    out[i * 2U + 1U] = kHex[hash[i] & 0x0F];
  }
  out[hashLen * 2U] = '\0';
  return true;
}

bool OtaService::hasStaIp() const {
  const IPAddress ip = WiFi.localIP();
  return ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0;
}

const char* OtaService::wifiStatusText(uint8_t status) const {
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
