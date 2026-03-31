#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

class WifiProvisionService {
public:
  enum class State : uint8_t {
    Idle,
    PortalReady,
    Connecting,
    Connected,
    Failed,
    PortalTimeout
  };

  enum class Error : uint8_t {
    None,
    InvalidInput,
    AuthFailed,
    ApNotFound,
    Timeout,
    Unknown
  };

  bool begin();
  void tick(uint32_t nowMs);

  void startPortal(uint32_t nowMs);
  void stopPortal();
  void cancelProvision();
  void finishOnlineSession();

  State state() const;
  Error error() const;
  bool isPortalActive() const;
  bool canStartPortal() const;
  bool consumeChanged();
  bool consumeTimeSyncRequest();

  const char* apSsid() const;
  const char* targetSsid() const;
  const char* staIp() const;

private:
  void setupWebRoutes();
  void handleRoot();
  void handleConnect();
  void handleStatus();
  void handleNotFound();

  void startConnecting(uint32_t nowMs);
  void requestConnecting();
  void setState(State next, Error err = Error::None);
  bool hasStaIp() const;
  void updateStaIpCache();
  void saveCredentials();
  void loadCredentials();
  void clearTarget();
  void finishProvisionSuccess();
  void cleanupRadio(bool keepStation);
  const char* errorText(Error err) const;

  static constexpr uint32_t kPortalTimeoutMs = 5UL * 60UL * 1000UL;
  static constexpr uint32_t kConnectTimeoutMs = 35UL * 1000UL;
  static constexpr uint32_t kConnectGraceMs = 12UL * 1000UL;

  DNSServer dns_;
  WebServer server_{80};
  Preferences prefs_;

  State state_ = State::Idle;
  Error error_ = Error::None;
  bool changed_ = false;
  bool timeSyncRequested_ = false;
  bool routesReady_ = false;
  bool connectPending_ = false;

  uint32_t portalStartMs_ = 0;
  uint32_t connectStartMs_ = 0;

  char apSsid_[33] = {0};
  char targetSsid_[33] = {0};
  char targetPass_[65] = {0};
  char staIp_[20] = {0};
};
