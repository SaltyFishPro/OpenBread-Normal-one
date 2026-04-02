#pragma once

#include <Arduino.h>

class OtaService {
public:
  enum class State : uint8_t {
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Downloading,
    Verifying,
    ReadyToApply,
    Applying,
    Error
  };

  enum class Error : uint8_t {
    None,
    Busy,
    NoCredentials,
    ConnectTimeout,
    HttpRequestFailed,
    HttpStatusInvalid,
    ManifestInvalid,
    ProductMismatch,
    ChannelMismatch,
    VersionNotNew,
    UrlInvalid,
    SizeInvalid,
    ShaInvalid,
    DownloadFailed,
    UpdateBeginFailed,
    UpdateWriteFailed,
    UpdateFinalizeFailed,
    ShaMismatch,
    ApplyNotReady,
    PostApplyVerifyFailed,
    Unknown
  };

  struct ManifestInfo {
    char product[40] = {0};
    char channel[16] = {0};
    char version[24] = {0};
    uint32_t build = 0;
    char firmwareUrl[192] = {0};
    char sha256[65] = {0};
    uint32_t size = 0;
    char releaseNote[96] = {0};
  };

  bool begin();
  void tick(uint32_t nowMs);

  bool requestCheck(uint32_t nowMs);
  bool requestDownload(uint32_t nowMs);
  bool requestApply();
  void cancel();

  State state() const;
  Error error() const;
  const ManifestInfo& manifest() const;
  int lastHttpStatus() const;
  uint32_t downloadedBytes() const;
  uint32_t totalBytes() const;
  uint8_t progressPercent() const;
  bool hasPostApplyResult() const;
  bool postApplySucceeded() const;
  const char* postApplyMessage() const;
  void clearPostApplyResult();
  bool consumeChanged();

private:
  enum class CheckPhase : uint8_t {
    None,
    Connect,
    FetchManifest,
    DownloadFirmware
  };

  enum class Action : uint8_t {
    None,
    CheckManifest,
    DownloadFirmware
  };

  static constexpr uint32_t kConnectTimeoutMs = 35000;
  static constexpr uint32_t kConnectGraceMs = 12000;
  static constexpr uint32_t kMaxImageSize = 7340032U;

  void setState(State next, Error err = Error::None);
  bool loadSavedCredentials();
  void cleanupRadio();
  bool fetchAndValidateManifest();

  bool parseStringField(const char* json, const char* key, char* out, size_t outLen) const;
  bool parseUintField(const char* json, const char* key, uint32_t& out) const;
  bool isHexString64(const char* value) const;
  bool isHttpsUrl(const char* value) const;
  bool hasStaIp() const;
  const char* wifiStatusText(uint8_t status) const;
  bool downloadAndVerifyFirmware();
  void clearProgress();
  bool toHexSha256(const uint8_t* hash, size_t hashLen, char* out, size_t outLen) const;
  void loadAndVerifyPostApplyResult();

  State state_ = State::Idle;
  Error error_ = Error::None;
  CheckPhase phase_ = CheckPhase::None;
  Action pendingAction_ = Action::None;
  bool changed_ = false;

  uint32_t actionStartMs_ = 0;
  uint32_t lastConnectLogMs_ = 0;
  int lastHttpStatus_ = 0;
  uint32_t downloadedBytes_ = 0;
  uint32_t totalBytes_ = 0;
  uint8_t lastProgressStep_ = 0;
  bool hasPostApplyResult_ = false;
  bool postApplySucceeded_ = false;
  char postApplyMessage_[96] = {0};

  char staSsid_[33] = {0};
  char staPass_[65] = {0};
  ManifestInfo manifest_{};
};
