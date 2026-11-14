#include "WifiProvisionService.h"
#include "TimeService.h"
#include "WifiPortalPage.h"

#include <WiFi.h>

#include "../log/Log.h"

namespace {
constexpr uint8_t kDnsPort = 53;

void jsonEscape(char* out, size_t capacity, const char* value) {
  if (capacity == 0) {
    return;
  }
  size_t pos = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value);
       p != nullptr && *p != '\0' && pos + 6U < capacity; ++p) {
    const unsigned char c = *p;
    if (c == '"' || c == '\\') {
      out[pos++] = '\\';
      out[pos++] = static_cast<char>(c);
    } else if (c == '\n') {
      out[pos++] = '\\';
      out[pos++] = 'n';
    } else if (c == '\r') {
      out[pos++] = '\\';
      out[pos++] = 'r';
    } else if (c == '\t') {
      out[pos++] = '\\';
      out[pos++] = 't';
    } else if (c < 0x20U) {
      out[pos++] = '?';
    } else {
      out[pos++] = static_cast<char>(c);
    }
  }
  out[pos] = '\0';
}

}  // namespace

bool WifiProvisionService::begin() {
  loadCredentials();
  radioActive_ = false;
  portalActive_ = false;
  holdEndMs_ = 0;
  WiFi.mode(WIFI_OFF);
  LogI("WIFI", "init complete, radio off");
  setState(State::Idle, Error::None);
  return true;
}

void WifiProvisionService::tick(uint32_t nowMs, TimeService& time) {
  if (state_ == State::SyncingTime) {
    if (time.snapshot().syncState != TimeService::SyncState::Syncing) {
      LogI("WIFI", "post-provision time sync finished state=%u error=%u",
              static_cast<unsigned>(time.snapshot().syncState),
              static_cast<unsigned>(time.snapshot().error));
      finishOnlineSession();
    }
    return;
  }

  if (portalActive_) {
    dns_.processNextRequest();
    server_.handleClient();
  }

  if (connectPending_) {
    connectPending_ = false;
    LogI("WIFI", "starting STA connect to '%s'", targetSsid_);
    startConnecting(nowMs);
  }

  // Only accept station success while this service is actively connecting.
  // This avoids stealing externally managed sessions (for example OTA checks).
  if (state_ == State::Connecting && WiFi.status() == WL_CONNECTED && hasStaIp() &&
      WiFi.SSID() == targetSsid_) {
    finishProvisionSuccess(nowMs, time);
    return;
  }

  // Keep the setup AP alive briefly after success so the phone can read the IP.
  if (state_ == State::Connected && portalActive_ && holdEndMs_ != 0 &&
      static_cast<int32_t>(nowMs - holdEndMs_) >= 0) {
    finishPortalHold(nowMs, time);
    return;
  }

  if (portalActive_ && state_ != State::Connected &&
      (nowMs - portalStartMs_) >= kPortalTimeoutMs) {
    LogI("WIFI", "portal timeout state=%u", static_cast<unsigned>(state_));
    setState(State::PortalTimeout, Error::Timeout);
    cleanupRadio(false);
    return;
  }

  if (state_ != State::Connecting) {
    return;
  }

  const wl_status_t s = WiFi.status();
  if ((nowMs - connectStartMs_) < kConnectTimeoutMs) {
    return;
  }

  // Keep showing "connecting" for a short grace window after timeout.
  // Some routers assign DHCP a bit later than association/authentication.
  if ((nowMs - connectStartMs_) < (kConnectTimeoutMs + kConnectGraceMs)) {
    return;
  }

  Error error = Error::Timeout;
  if (s == WL_CONNECT_FAILED) {
    LogW("WIFI", "connect failed: auth portal=%u", portalActive_);
    error = Error::AuthFailed;
  } else if (s == WL_NO_SSID_AVAIL) {
    LogW("WIFI", "connect failed: ap not found portal=%u", portalActive_);
    error = Error::ApNotFound;
  } else {
    LogW("WIFI", "connect failed: timeout portal=%u", portalActive_);
  }
  // Stop this station attempt without touching the setup AP, so the phone can
  // show the failure and retry through the same page.
  WiFi.disconnect(false, false);
  if (!portalActive_) cleanupRadio(false);
  setState(State::Failed, error);
}

void WifiProvisionService::startPortal(uint32_t nowMs, Purpose purpose,
                                        int8_t replacement, const char* prefill) {
  if (!canStartPortal()) {
    return;
  }
  const uint32_t startedMs = millis();
  LogI("WIFI", "portal start requested");

  char initial[33] = {};
  if (prefill) snprintf(initial, sizeof(initial), "%s", prefill);
  replacementSsid_[0] = '\0';
  const auto* replace = replacement >= 0 ? networks_.info(static_cast<uint8_t>(replacement)) : nullptr;
  if (replace) snprintf(replacementSsid_, sizeof(replacementSsid_), "%s", replace->ssid);
  clearTarget();
  snprintf(targetSsid_, sizeof(targetSsid_), "%s", initial);
  fromSaved_ = false;
  saveStatus_ = SaveStatus::None;
  purpose_ = purpose;

  const uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFU);
  snprintf(apSsid_, sizeof(apSsid_), "OpenBread-Setup-%04X", static_cast<unsigned>(chip));

  cleanupRadio(false);
  const uint32_t cleanupMs = millis()-startedMs;
  radioActive_ = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPdisconnect(true);
  const uint32_t driverMs = millis()-startedMs-cleanupMs;
  if (!WiFi.softAP(apSsid_)) {
    LogE("WIFI", "portal start failed AP='%s' elapsed_ms=%lu", apSsid_,
         static_cast<unsigned long>(millis()-startedMs));
    cleanupRadio(false);
    setState(State::Failed, Error::Unknown);
    return;
  }
  portalActive_ = true;
  holdEndMs_ = 0;
  LogI("WIFI", "portal started, AP='%s'", apSsid_);

  if (!routesReady_) {
    setupWebRoutes();
  }

  dns_.start(kDnsPort, "*", WiFi.softAPIP());
  server_.begin();

  portalStartMs_ = nowMs + (millis()-startedMs);
  setState(State::PortalReady, Error::None);
  LogI("WIFI", "portal ready elapsed_ms=%lu cleanup_ms=%lu driver_ms=%lu",
       static_cast<unsigned long>(millis()-startedMs),
       static_cast<unsigned long>(cleanupMs),static_cast<unsigned long>(driverMs));
}

void WifiProvisionService::stopPortal() {
  if (portalActive_) {
    dns_.stop();
    server_.stop();
    WiFi.softAPdisconnect(true);
  }
  portalActive_ = false;
  holdEndMs_ = 0;
}

void WifiProvisionService::cancelProvision(TimeService& time) {
  if (state_ == State::SyncingTime) {
    time.cancelNtpSync();
    finishOnlineSession();
    return;
  }
  if (!radioActive_) {
    return;
  }
  if (state_ == State::Connected && portalActive_ && purpose_ == Purpose::TimeSync) {
    LogI("WIFI", "success hold canceled by user; starting time sync");
    finishPortalHold(millis(), time);
    return;
  }
  LogI("WIFI", "provision canceled by user");
  cleanupRadio(false);
  setState(State::Idle, Error::None);
}

void WifiProvisionService::finishOnlineSession() {
  LogI("WIFI", "online session finished, radio off, keep connected state");
  cleanupRadio(false);
  setState(State::Connected, Error::None);
}

void WifiProvisionService::abortOnlineSession() {
  LogW("WIFI", "online session aborted; radio off");
  cleanupRadio(false);
  setState(State::Idle, Error::None);
}

WifiProvisionService::State WifiProvisionService::state() const { return state_; }

WifiProvisionService::Error WifiProvisionService::error() const { return error_; }

bool WifiProvisionService::isPortalActive() const { return portalActive_; }

bool WifiProvisionService::isRadioActive() const { return radioActive_; }

bool WifiProvisionService::canStartPortal() const {
  if (portalActive_) {
    return false;
  }
  return state_ == State::Idle || state_ == State::Connected || state_ == State::Failed ||
         state_ == State::PortalTimeout;
}

bool WifiProvisionService::canRetryConnect() const {
  return state_ == State::Failed && error_ != Error::Unknown && targetSsid_[0] != '\0';
}

bool WifiProvisionService::retryConnect() {
  if (!canRetryConnect()) {
    return false;
  }
  requestConnecting();
  return true;
}

bool WifiProvisionService::editPortal() {
  if (!portalActive_ || state_ != State::Failed) return false;
  setState(State::PortalReady, Error::None);
  return true;
}

uint32_t WifiProvisionService::portalRemainingSeconds(uint32_t nowMs) const {
  if (!portalActive_) {
    return 0;
  }
  const uint32_t elapsed = nowMs - portalStartMs_;
  if (elapsed >= kPortalTimeoutMs) {
    return 0;
  }
  return (kPortalTimeoutMs - elapsed + 999U) / 1000U;
}

uint32_t WifiProvisionService::holdRemainingSeconds(uint32_t nowMs) const {
  if (!portalActive_ || state_ != State::Connected || holdEndMs_ == 0) {
    return 0;
  }
  const int32_t remaining = static_cast<int32_t>(holdEndMs_ - nowMs);
  if (remaining <= 0) {
    return 0;
  }
  return (static_cast<uint32_t>(remaining) + 999U) / 1000U;
}

bool WifiProvisionService::consumeChanged() {
  const bool wasChanged = changed_;
  changed_ = false;
  return wasChanged;
}

const char* WifiProvisionService::apSsid() const { return apSsid_; }

const char* WifiProvisionService::targetSsid() const { return targetSsid_; }

const char* WifiProvisionService::targetPass() const { return targetPass_; }

const char* WifiProvisionService::staIp() const { return staIp_; }

void WifiProvisionService::setupWebRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/connect", HTTP_POST, [this]() { handleConnect(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.onNotFound([this]() { handleNotFound(); });
  routesReady_ = true;
}

void WifiProvisionService::handleRoot() {
  server_.sendHeader("Cache-Control", "no-store, max-age=0");
  server_.send_P(200, "text/html; charset=utf-8", kWifiPortalPage);
}

void WifiProvisionService::handleConnect() {
  if (!server_.hasArg("ssid")) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_ssid\"}");
    return;
  }
  if (state_ == State::Connecting || state_ == State::SyncingTime) {
    server_.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
    return;
  }
  if (!portalActive_ || (state_ != State::PortalReady && state_ != State::Failed)) {
    server_.send(409, "application/json", "{\"ok\":false,\"error\":\"portal_inactive\"}");
    return;
  }

  const String ssid = server_.arg("ssid");
  const String pass = server_.hasArg("pass") ? server_.arg("pass") : String();
  bool invalidName = ssid.length() == 0 || ssid.length() > 32;
  bool nonSpace = false;
  for (size_t i = 0; i < ssid.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(ssid[i]);
    invalidName |= c < 0x20 || c == 0x7F;
    nonSpace |= c != ' ';
  }
  if (invalidName || !nonSpace || pass.length() > 64) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_input\"}");
    return;
  }

  memset(targetSsid_, 0, sizeof(targetSsid_));
  memset(targetPass_, 0, sizeof(targetPass_));
  ssid.toCharArray(targetSsid_, sizeof(targetSsid_));
  pass.toCharArray(targetPass_, sizeof(targetPass_));

  LogI("WIFI", "web connect request ssid='%s'", targetSsid_);
  server_.send(200, "application/json", "{\"ok\":true}");
  requestConnecting();
}

void WifiProvisionService::handleStatus() {
  char targetJson[200];
  jsonEscape(targetJson, sizeof(targetJson), targetSsid_);
  char body[420];
  const uint32_t nowMs = millis();
  snprintf(body, sizeof(body),
           "{\"state\":\"%s\",\"error\":\"%s\",\"portal\":%s,\"ap\":\"%s\","
           "\"target\":\"%s\",\"ip\":\"%s\",\"remaining\":%lu,\"hold\":%lu}",
           stateText(), errorText(error_), portalActive_ ? "true" : "false", apSsid_,
           targetJson, staIp_,
           static_cast<unsigned long>(portalRemainingSeconds(nowMs)),
           static_cast<unsigned long>(holdRemainingSeconds(nowMs)));
  server_.sendHeader("Cache-Control", "no-store, max-age=0");
  server_.send(200, "application/json", body);
}

void WifiProvisionService::handleNotFound() {
  server_.sendHeader("Location", "http://192.168.4.1/");
  server_.send(302, "text/plain", "");
}

void WifiProvisionService::startConnecting(uint32_t nowMs) {
  // Keep the setup AP alive while the station connects. The phone uses it to
  // poll progress and to retry after a failure.
  radioActive_ = true;
  WiFi.mode(portalActive_ ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  memset(staIp_, 0, sizeof(staIp_));
  WiFi.begin(targetSsid_, targetPass_);
  connectStartMs_ = nowMs;
  holdEndMs_ = 0;
  LogI("WIFI", "STA connect requested source=%s portal=%u", fromSaved_ ? "saved" : "phone", portalActive_);
  setState(State::Connecting, Error::None);
}

void WifiProvisionService::requestConnecting() {
  connectPending_ = true;
  radioActive_ = true;
  saveStatus_ = SaveStatus::None;
  setState(State::Connecting, Error::None);
}

void WifiProvisionService::setState(State next, Error err) {
  if (state_ == next && error_ == err) {
    return;
  }
  state_ = next;
  error_ = err;
  changed_ = true;
}

bool WifiProvisionService::hasStaIp() const {
  const IPAddress ip = WiFi.localIP();
  return ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0;
}

void WifiProvisionService::updateStaIpCache() {
  const IPAddress ip = WiFi.localIP();
  snprintf(staIp_, sizeof(staIp_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void WifiProvisionService::saveCredentials(const TimeService& time) {
  WifiNetworkStore::LastUsed used{};
  if (time.snapshot().valid) {
    const auto& now = time.snapshot().now;
    used = {now.year, now.month, now.day, now.hour, now.minute};
  }
  const int8_t replacement = networks_.find(replacementSsid_);
  if (networks_.find(targetSsid_) < 0 && networks_.count() == WifiNetworkStore::kCapacity && replacement < 0) {
    saveStatus_ = SaveStatus::Full;
    LogW("WIFI", "connected but history full; existing records retained");
    return;
  }
  const bool saved = networks_.save(targetSsid_, targetPass_, used,
      replacement < 0 ? WifiNetworkStore::kNoIndex : static_cast<uint8_t>(replacement));
  saveStatus_ = saved ? SaveStatus::Saved : SaveStatus::StorageError;
  LogI("WIFI", "history save=%u count=%u", static_cast<unsigned>(saveStatus_), networks_.count());
}

void WifiProvisionService::loadCredentials() {
  clearTarget();
  if (!networks_.begin()) LogE("WIFI", "saved networks could not be loaded");
  (void)networks_.credentials(0, targetSsid_, sizeof(targetSsid_), targetPass_, sizeof(targetPass_));
  LogI("WIFI", "saved networks loaded count=%u", networks_.count());
}

bool WifiProvisionService::connectSaved(uint8_t index) {
  if (radioActive_ || !networks_.info(index)) return false;
  clearTarget();
  if (!networks_.credentials(index, targetSsid_, sizeof(targetSsid_), targetPass_, sizeof(targetPass_))) return false;
  fromSaved_ = true;
  purpose_ = Purpose::TimeSync;
  replacementSsid_[0] = '\0';
  LogI("WIFI", "user selected saved network index=%u", index);
  requestConnecting();
  return true;
}

bool WifiProvisionService::forgetNetwork(uint8_t index) {
  if (radioActive_ || !networks_.forget(index)) return false;
  clearTarget();
  (void)networks_.credentials(0, targetSsid_, sizeof(targetSsid_), targetPass_, sizeof(targetPass_));
  changed_ = true;
  LogI("WIFI", "network forgotten index=%u remaining=%u", index, networks_.count());
  return true;
}

void WifiProvisionService::clearTarget() {
  memset(targetSsid_, 0, sizeof(targetSsid_));
  memset(targetPass_, 0, sizeof(targetPass_));
  memset(staIp_, 0, sizeof(staIp_));
}

void WifiProvisionService::finishProvisionSuccess(uint32_t nowMs, TimeService& time) {
  updateStaIpCache();
  saveCredentials(time);
  if (purpose_ == Purpose::FontSetup) {
    LogI("WIFI", "connected for font setup ip=%s", staIp_);
    cleanupRadio(true);
    setState(State::Connected, Error::None);
    return;
  }
  if (!portalActive_) {
    LogI("WIFI", "saved network connected ip=%s; starting time sync", staIp_);
    finishPortalHold(nowMs, time);
    return;
  }
  holdEndMs_ = nowMs + kSuccessHoldMs;
  LogI("WIFI", "connected ip=%s; keeping setup AP for %lu ms", staIp_,
       static_cast<unsigned long>(kSuccessHoldMs));
  setState(State::Connected, Error::None);
}

void WifiProvisionService::finishPortalHold(uint32_t nowMs, TimeService& time) {
  holdEndMs_ = 0;
  LogI("WIFI", "setup AP hold finished; starting one-shot time sync");
  stopPortal();
  WiFi.setSleep(true);
  if (time.requestNtpSync(nowMs, targetSsid_, targetPass_, true)) {
    setState(State::SyncingTime, Error::None);
    return;
  }
  LogI("WIFI", "post-provision time sync not started error=%u",
       static_cast<unsigned>(time.snapshot().error));
  finishOnlineSession();
}

void WifiProvisionService::cleanupRadio(bool keepStation) {
  connectPending_ = false;
  holdEndMs_ = 0;
  stopPortal();
  if (keepStation) {
    radioActive_ = true;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    return;
  }

  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
  radioActive_ = false;
}

const char* WifiProvisionService::stateText() const {
  switch (state_) {
    case State::Idle:
      return "idle";
    case State::PortalReady:
      return "portal";
    case State::Connecting:
      return "connecting";
    case State::Connected:
      return "connected";
    case State::Failed:
      return "failed";
    case State::PortalTimeout:
      return "timeout";
    case State::SyncingTime:
      return "syncing";
    default:
      return "unknown";
  }
}

const char* WifiProvisionService::errorText(Error err) const {
  switch (err) {
    case Error::None:
      return "none";
    case Error::InvalidInput:
      return "invalid_input";
    case Error::AuthFailed:
      return "auth_failed";
    case Error::ApNotFound:
      return "ap_not_found";
    case Error::Timeout:
      return "timeout";
    default:
      return "unknown";
  }
}
