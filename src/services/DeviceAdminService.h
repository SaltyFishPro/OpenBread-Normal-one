#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <FS.h>
#include <esp_ota_ops.h>
#include "DeviceSettings.h"
class OtaService;
class TimeService;
class PowerDiagnosticService;

class DeviceAdminService {
public:
  bool start(uint32_t now, DeviceSettings& settings, OtaService& ota,
             TimeService& time, PowerDiagnosticService& power);
  void stop();
  void tick(uint32_t now);
  bool active() const { return active_; }
  bool consumeChanged() { bool result = changed_; changed_ = false; return result; }
  const char* ssid() const { return ssid_; }
  const char* code() const { return code_; }
  const char* statusText() const;
  bool restarting() const { return restartPending_; }
private:
  void routes();
  bool authorized(bool activity = true);
  void registerFileRoutes();
  void registerUpdateRoutes();
  bool filePath(const String& value, bool write);
  void abortUpload();
  WebServer server_{80};
  DNSServer dns_;
  DeviceSettings* settings_ = nullptr;
  OtaService* ota_ = nullptr;
  TimeService* time_ = nullptr;
  PowerDiagnosticService* power_ = nullptr;
  File uploadFile_;
  char uploadTarget_[242] = {};
  bool uploadOk_ = false;
  bool uploadStarted_ = false;
  esp_ota_handle_t localHandle_ = 0;
  const esp_partition_t* localPartition_ = nullptr;
  bool localWriting_ = false, localReady_ = false;
  bool restartPending_ = false;
  uint32_t restartAtMs_ = 0;
  bool stopPending_ = false;
  bool active_ = false, routesReady_ = false, changed_ = false;
  uint32_t activityMs_ = 0, blockedUntilMs_ = 0;
  uint8_t failures_ = 0;
  char ssid_[33] = {}, code_[7] = {}, token_[33] = {};
};
