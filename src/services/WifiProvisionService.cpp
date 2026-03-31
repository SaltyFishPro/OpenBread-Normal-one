#include "WifiProvisionService.h"

#include <stdarg.h>
#include <WiFi.h>

namespace {
constexpr uint8_t kDnsPort = 53;
constexpr const char* kPrefsNs = "wifi";
constexpr const char* kPrefsKeySsid = "ssid";
constexpr const char* kPrefsKeyPass = "pass";

#ifndef OB_WIFI_LOG_ENABLED
#define OB_WIFI_LOG_ENABLED 1
#endif

void wifiLog(const char* fmt, ...) {
#if OB_WIFI_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[WIFI] ");
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

bool WifiProvisionService::begin() {
  loadCredentials();
  WiFi.mode(WIFI_OFF);
  wifiLog("init complete, radio off");
  setState(State::Idle, Error::None);
  return true;
}

void WifiProvisionService::tick(uint32_t nowMs) {
  if (isPortalActive()) {
    dns_.processNextRequest();
    server_.handleClient();
  }

  if (connectPending_) {
    connectPending_ = false;
    wifiLog("starting STA connect to '%s'", targetSsid_);
    startConnecting(nowMs);
  }

  // Late-success fallback: DHCP may complete after connect timeout classification.
  if (state_ != State::Connected && (WiFi.status() == WL_CONNECTED || hasStaIp())) {
    finishProvisionSuccess();
    return;
  }

  if (state_ == State::PortalReady && (nowMs - portalStartMs_) >= kPortalTimeoutMs) {
    wifiLog("portal timeout");
    setState(State::PortalTimeout, Error::Timeout);
    cleanupRadio(false);
    return;
  }

  if (state_ != State::Connecting) {
    return;
  }

  const wl_status_t s = WiFi.status();
  if (s == WL_CONNECTED || hasStaIp()) {
    finishProvisionSuccess();
    return;
  }

  if ((nowMs - connectStartMs_) < kConnectTimeoutMs) {
    return;
  }

  // Keep showing "connecting" for a short grace window after timeout.
  // Some routers assign DHCP a bit later than association/authentication.
  if ((nowMs - connectStartMs_) < (kConnectTimeoutMs + kConnectGraceMs)) {
    return;
  }

  if (s == WL_CONNECT_FAILED) {
    wifiLog("connect failed: auth");
    setState(State::Failed, Error::AuthFailed);
  } else if (s == WL_NO_SSID_AVAIL) {
    wifiLog("connect failed: ap not found");
    setState(State::Failed, Error::ApNotFound);
  } else {
    wifiLog("connect failed: timeout");
    setState(State::Failed, Error::Timeout);
  }
  cleanupRadio(false);
}

void WifiProvisionService::startPortal(uint32_t nowMs) {
  if (!canStartPortal()) {
    return;
  }

  const uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFU);
  snprintf(apSsid_, sizeof(apSsid_), "OpenBread-Setup-%04X", static_cast<unsigned>(chip));

  cleanupRadio(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPdisconnect(true);
  WiFi.softAP(apSsid_);
  wifiLog("portal started, AP='%s'", apSsid_);

  if (!routesReady_) {
    setupWebRoutes();
  }

  dns_.start(kDnsPort, "*", WiFi.softAPIP());
  server_.begin();

  portalStartMs_ = nowMs;
  setState(State::PortalReady, Error::None);
}

void WifiProvisionService::stopPortal() {
  dns_.stop();
  server_.stop();
  WiFi.softAPdisconnect(true);
}

void WifiProvisionService::cancelProvision() {
  if (!isPortalActive()) {
    return;
  }
  wifiLog("provision canceled by user");
  cleanupRadio(false);
  setState(State::Idle, Error::None);
}

void WifiProvisionService::finishOnlineSession() {
  wifiLog("online session finished, radio off");
  memset(staIp_, 0, sizeof(staIp_));
  cleanupRadio(false);
  setState(State::Idle, Error::None);
}

WifiProvisionService::State WifiProvisionService::state() const { return state_; }

WifiProvisionService::Error WifiProvisionService::error() const { return error_; }

bool WifiProvisionService::isPortalActive() const {
  return state_ == State::PortalReady || state_ == State::Connecting;
}

bool WifiProvisionService::canStartPortal() const {
  return state_ == State::Idle || state_ == State::Failed || state_ == State::PortalTimeout;
}

bool WifiProvisionService::consumeChanged() {
  const bool wasChanged = changed_;
  changed_ = false;
  return wasChanged;
}

bool WifiProvisionService::consumeTimeSyncRequest() {
  const bool requested = timeSyncRequested_;
  timeSyncRequested_ = false;
  return requested;
}

const char* WifiProvisionService::apSsid() const { return apSsid_; }

const char* WifiProvisionService::targetSsid() const { return targetSsid_; }

const char* WifiProvisionService::staIp() const { return staIp_; }

void WifiProvisionService::setupWebRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/connect", HTTP_POST, [this]() { handleConnect(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.onNotFound([this]() { handleNotFound(); });
  routesReady_ = true;
}

void WifiProvisionService::handleRoot() {
  const char* page =
      "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' "
      "content='width=device-width,initial-scale=1'>"
      "<title>OpenBread WiFi Setup</title></head>"
      "<body><h2>OpenBread WiFi Setup</h2>"
      "<form method='POST' action='/connect'>"
      "<label>WiFi SSID</label><br/><input name='ssid' maxlength='32'/><br/><br/>"
      "<label>Password</label><br/><input name='pass' maxlength='64' type='password'/><br/><br/>"
      "<button type='submit'>Connect</button></form>"
      "<p><a href='/status'>Status</a></p></body></html>";
  server_.send(200, "text/html", page);
}

void WifiProvisionService::handleConnect() {
  if (!server_.hasArg("ssid") || !server_.hasArg("pass")) {
    setState(State::Failed, Error::InvalidInput);
    server_.send(400, "text/plain", "missing ssid or pass");
    return;
  }

  const String ssid = server_.arg("ssid");
  const String pass = server_.arg("pass");
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
    setState(State::Failed, Error::InvalidInput);
    server_.send(400, "text/plain", "invalid ssid/pass");
    return;
  }

  memset(targetSsid_, 0, sizeof(targetSsid_));
  memset(targetPass_, 0, sizeof(targetPass_));
  ssid.toCharArray(targetSsid_, sizeof(targetSsid_));
  pass.toCharArray(targetPass_, sizeof(targetPass_));

  server_.send(200, "text/html",
                "<html><body><h3>Connecting...</h3><p>Check device screen.</p>"
                "<p><a href='/status'>Refresh status</a></p></body></html>");
  requestConnecting();
}

void WifiProvisionService::handleStatus() {
  char body[256];
  snprintf(body, sizeof(body),
           "{\"state\":%u,\"error\":\"%s\",\"ap\":\"%s\",\"target\":\"%s\",\"ip\":\"%s\"}",
           static_cast<unsigned>(state_), errorText(error_), apSsid_, targetSsid_, staIp_);
  server_.send(200, "application/json", body);
}

void WifiProvisionService::handleNotFound() {
  server_.sendHeader("Location", "http://192.168.4.1/");
  server_.send(302, "text/plain", "");
}

void WifiProvisionService::startConnecting(uint32_t nowMs) {
  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  WiFi.begin(targetSsid_, targetPass_);
  connectStartMs_ = nowMs;
  wifiLog("STA connect requested");
  setState(State::Connecting, Error::None);
}

void WifiProvisionService::requestConnecting() { connectPending_ = true; }

void WifiProvisionService::setState(State next, Error err) {
  if (state_ == next && error_ == err) {
    return;
  }
  state_ = next;
  error_ = err;
  if (next == State::Connected) {
    timeSyncRequested_ = true;
  }
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

void WifiProvisionService::saveCredentials() {
  if (!prefs_.begin(kPrefsNs, false)) {
    return;
  }
  prefs_.putString(kPrefsKeySsid, targetSsid_);
  prefs_.putString(kPrefsKeyPass, targetPass_);
  prefs_.end();
}

void WifiProvisionService::loadCredentials() {
  clearTarget();
  if (!prefs_.begin(kPrefsNs, true)) {
    return;
  }
  const String ssid = prefs_.getString(kPrefsKeySsid, "");
  const String pass = prefs_.getString(kPrefsKeyPass, "");
  prefs_.end();
  ssid.toCharArray(targetSsid_, sizeof(targetSsid_));
  pass.toCharArray(targetPass_, sizeof(targetPass_));
}

void WifiProvisionService::clearTarget() {
  memset(targetSsid_, 0, sizeof(targetSsid_));
  memset(targetPass_, 0, sizeof(targetPass_));
  memset(staIp_, 0, sizeof(staIp_));
}

void WifiProvisionService::finishProvisionSuccess() {
  updateStaIpCache();
  saveCredentials();
  wifiLog("connected, ip=%s", staIp_);
  cleanupRadio(true);
  setState(State::Connected, Error::None);
}

void WifiProvisionService::cleanupRadio(bool keepStation) {
  connectPending_ = false;
  stopPortal();
  if (keepStation) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    return;
  }

  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
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
