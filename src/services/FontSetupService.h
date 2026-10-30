#pragma once

#include <Arduino.h>
#include <FS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include "FontBundle.h"

class SdCardService;
class WifiProvisionService;
class TimeService;

class FontSetupService {
public:
  enum class State : uint8_t {
    Checking,
    NeedCard,
    NeedFonts,
    Portal,
    Connecting,
    Downloading,
    Verifying,
    Restarting,
    Error,
    Ready,
  };

  enum class Error : uint8_t {
    None,
    Card,
    NoSpace,
    Wifi,
    Http,
    Write,
    Hash,
  };

  void begin(uint32_t nowMs);
  void tick(uint32_t nowMs, SdCardService& sd, WifiProvisionService& wifi);
  void confirm(uint32_t nowMs, SdCardService& sd, WifiProvisionService& wifi);
  void cancel(WifiProvisionService& wifi, TimeService& time);
  bool consumeChanged();
  State state() const { return state_; }
  Error error() const { return error_; }
  uint8_t progressPercent() const;
  uint32_t stateSinceMs() const { return stateSinceMs_; }
  const char* badFile() const { return badFile_; }

private:
  bool inspect(SdCardService& sd);
  bool fileLooksValid(const FontBundle::File& expected) const;
  bool startNextFile(SdCardService& sd);
  void downloadChunk(uint32_t nowMs);
  void finishFile();
  void closeDownload(bool removePart);
  void fail(Error error, const char* stage);
  void setState(State state, uint32_t nowMs);

  State state_ = State::Checking;
  Error error_ = Error::None;
  uint32_t stateSinceMs_ = 0;
  uint32_t lastDataMs_ = 0;
  uint32_t completedBytes_ = 0;
  uint32_t receivedBytes_ = 0;
  uint8_t fileIndex_ = 0;
  uint8_t lastPercent_ = 0;
  bool changed_ = true;
  bool downloadOpen_ = false;
  bool hashOpen_ = false;
  const char* badFile_ = "";
  File partFile_;
  HTTPClient http_;
  WiFiClientSecure client_;
  mbedtls_sha256_context hash_{};
};
