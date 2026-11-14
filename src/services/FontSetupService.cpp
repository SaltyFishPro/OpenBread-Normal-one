#include "FontSetupService.h"
#include "../bsp/SdCardSession.h"

#include <SD_MMC.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "SdCardService.h"
#include "WifiProvisionService.h"
#include "TimeService.h"
#include "../log/Log.h"

namespace {
constexpr uint32_t kCheckDelayMs = 250U;
constexpr uint32_t kDataTimeoutMs = 15000U;
constexpr uint32_t kVerifiedDisplayMs = 600U;
constexpr uint32_t kSpaceMarginBytes = 128U * 1024U;
constexpr uint32_t kHttpTimeoutMs = 5000U;

uint16_t le16(const uint8_t* value) { return value[0] | (uint16_t(value[1]) << 8U); }
uint32_t le32(const uint8_t* value) { return le16(value) | (uint32_t(le16(value + 2)) << 16U); }
uint8_t hexDigit(char value) {
  if (value >= '0' && value <= '9') return static_cast<uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f') return static_cast<uint8_t>(value - 'a' + 10);
  return 0xFFU;
}
bool hashMatches(const uint8_t* actual, const char* expected) {
  for (uint8_t i = 0; i < 32U; ++i) {
    const uint8_t high = hexDigit(expected[i * 2U]);
    const uint8_t low = hexDigit(expected[i * 2U + 1U]);
    if (high > 15U || low > 15U || actual[i] != static_cast<uint8_t>((high << 4U) | low))
      return false;
  }
  return expected[64] == '\0';
}
}

void FontSetupService::begin(uint32_t nowMs) {
  mbedtls_sha256_init(&hash_);
  stateSinceMs_ = nowMs;
  state_ = State::Checking;
  changed_ = true;
  error_ = Error::None;
  badFile_ = "";
  LogI("SETUP", "boot resource check started");
}

void FontSetupService::setState(State state, uint32_t nowMs) {
  if (state_ == state) return;
  state_ = state;
  stateSinceMs_ = nowMs;
  changed_ = true;
  LogI("SETUP", "state=%u error=%u file=%s", static_cast<unsigned>(state_),
       static_cast<unsigned>(error_), badFile_);
}

bool FontSetupService::fileLooksValid(const FontBundle::File& expected) const {
  char path[48];
  snprintf(path, sizeof(path), "%s/%s", FontBundle::kDirectory, expected.name);
  File file = SD_MMC.open(path, FILE_READ);
  if (!file || file.isDirectory() || file.size() != expected.bytes) {
    file.close();
    return false;
  }
  uint8_t header[40];
  const bool read = file.read(header, sizeof(header)) == sizeof(header);
  file.close();
  return read && memcmp(header, "NBFT", 4) == 0 && le16(header + 4) == 1U &&
         le16(header + 6) == 64U && le16(header + 8) == 24U &&
         le16(header + 10) == expected.pixelSize && le16(header + 18) == 1U &&
         le32(header + 20) == expected.glyphCount && le32(header + 32) == expected.bytes &&
         le32(header + 36) == expected.payloadCrc;
}

bool FontSetupService::inspect(SdCardService& sd) {
  const auto status = sd.refresh();
  if (status != SdCardService::Status::Ready || !sd.rootReadable()) {
    error_ = Error::Card;
    badFile_ = "SD";
    setState(State::NeedCard, millis());
    return false;
  }
  for (const auto& expected : FontBundle::kFiles) {
    if (fileLooksValid(expected)) continue;
    badFile_ = expected.name;
    error_ = Error::None;
    setState(State::NeedFonts, millis());
    return false;
  }
  badFile_ = "";
  error_ = Error::None;
  setState(State::Ready, millis());
  return true;
}

void FontSetupService::confirm(uint32_t nowMs, SdCardService& sd,
                               WifiProvisionService& wifi) {
  if (state_ == State::NeedCard) {
    setState(State::Checking, nowMs);
    return;
  }
  if (state_ != State::NeedFonts && state_ != State::Error) return;
  if (sd.refresh() != SdCardService::Status::Ready || !sd.rootReadable()) {
    error_ = Error::Card;
    setState(State::NeedCard, nowMs);
    return;
  }
  if (inspect(sd)) return;
  wifi.startPortal(nowMs, WifiProvisionService::Purpose::FontSetup);
  if (wifi.state() != WifiProvisionService::State::PortalReady) {
    fail(Error::Wifi, "portal start");
    return;
  }
  error_ = Error::None;
  setState(State::Portal, nowMs);
}

void FontSetupService::cancel(WifiProvisionService& wifi, TimeService& time) {
  if (state_ != State::Portal && state_ != State::Connecting) return;
  wifi.cancelProvision(time);
  setState(State::NeedFonts, millis());
}

void FontSetupService::tick(uint32_t nowMs, SdCardService& sd,
                            WifiProvisionService& wifi) {
  if (state_ == State::Checking) {
    if (nowMs - stateSinceMs_ >= kCheckDelayMs) inspect(sd);
    return;
  }
  if (state_ == State::Verifying) {
    if (nowMs - stateSinceMs_ >= kVerifiedDisplayMs)
      setState(State::Restarting, nowMs);
    return;
  }
  if (state_ == State::Portal || state_ == State::Connecting) {
    switch (wifi.state()) {
      case WifiProvisionService::State::PortalReady: setState(State::Portal, nowMs); return;
      case WifiProvisionService::State::Connecting: setState(State::Connecting, nowMs); return;
      case WifiProvisionService::State::Connected:
        completedBytes_ = receivedBytes_ = 0;
        fileIndex_ = lastPercent_ = 0;
        if (!startNextFile(sd)) {
          if (state_ == State::Verifying) wifi.finishOnlineSession();
          else wifi.abortOnlineSession();
          return;
        }
        setState(State::Downloading, nowMs);
        return;
      case WifiProvisionService::State::Failed:
      case WifiProvisionService::State::PortalTimeout:
        wifi.abortOnlineSession();
        fail(Error::Wifi, "wifi connection");
        return;
      default: return;
    }
  }
  if (state_ == State::Downloading) {
    if (WiFi.status() != WL_CONNECTED) {
      fail(Error::Wifi, "station disconnected");
      wifi.abortOnlineSession();
      return;
    }
    downloadChunk(nowMs);
    if (state_ == State::Error) {
      wifi.abortOnlineSession();
      return;
    }
    if (state_ != State::Verifying) return;
    finishFile();
    if (state_ == State::Error) {
      wifi.abortOnlineSession();
      return;
    }
    if (!startNextFile(sd)) {
      if (state_ == State::Verifying) wifi.finishOnlineSession();
      else wifi.abortOnlineSession();
    }
  }
}

bool FontSetupService::startNextFile(SdCardService& sd) {
  while (fileIndex_ < FontBundle::kFileCount && fileLooksValid(FontBundle::kFiles[fileIndex_])) {
    completedBytes_ += FontBundle::kFiles[fileIndex_].bytes;
    LogI("SETUP", "font present file=%s", FontBundle::kFiles[fileIndex_].name);
    ++fileIndex_;
  }
  if (fileIndex_ == FontBundle::kFileCount) {
    setState(State::Verifying, millis());
    return false;
  }
  const auto& expected = FontBundle::kFiles[fileIndex_];
  badFile_ = expected.name;
  if (sd.refresh() != SdCardService::Status::Ready ||
      sd.freeBytes() < expected.bytes + kSpaceMarginBytes) {
    fail(sd.status() == SdCardService::Status::Ready ? Error::NoSpace : Error::Card,
         "space or card");
    return false;
  }
  if (!SD_MMC.exists(FontBundle::kDirectory) && !SD_MMC.mkdir(FontBundle::kDirectory)) {
    fail(Error::Write, "create Font directory");
    return false;
  }
  if (SD_MMC.exists(FontBundle::kPartPath) && !SD_MMC.remove(FontBundle::kPartPath)) {
    fail(Error::Write, "remove previous part");
    return false;
  }
  char url[128];
  snprintf(url, sizeof(url), "%s%s", FontBundle::kBaseUrl, expected.name);
  client_.setInsecure();
  http_.setTimeout(kHttpTimeoutMs);
  if (!http_.begin(client_, url)) { fail(Error::Http, "http begin"); return false; }
  downloadOpen_ = true;
  const int status = http_.GET();
  if (status != HTTP_CODE_OK || http_.getSize() != static_cast<int>(expected.bytes)) {
    LogE("SETUP", "font GET status=%d length=%d file=%s", status, http_.getSize(), expected.name);
    fail(Error::Http, "http response");
    return false;
  }
  partFile_ = SD_MMC.open(FontBundle::kPartPath, FILE_WRITE);
  if (!partFile_) { fail(Error::Write, "part open"); return false; }
  mbedtls_sha256_starts(&hash_, 0);
  hashOpen_ = true;
  receivedBytes_ = 0;
  lastDataMs_ = millis();
  setState(State::Downloading, lastDataMs_);
  LogI("SETUP", "font download file=%s size=%lu", expected.name,
       static_cast<unsigned long>(expected.bytes));
  return true;
}

void FontSetupService::downloadChunk(uint32_t nowMs) {
  WiFiClient* stream = http_.getStreamPtr();
  if (!stream || nowMs - lastDataMs_ > kDataTimeoutMs) {
    fail(Error::Http, "stream timeout");
    return;
  }
  const int available = stream->available();
  if (available <= 0) {
    if (!http_.connected()) fail(Error::Http, "stream closed");
    return;
  }
  const auto& expected = FontBundle::kFiles[fileIndex_];
  uint8_t buffer[1024];
  const size_t remaining = expected.bytes - receivedBytes_;
  const size_t count = std::min(static_cast<size_t>(available),
                                std::min(remaining, sizeof(buffer)));
  const int read = stream->readBytes(buffer, count);
  if (read <= 0) { fail(Error::Http, "stream read"); return; }
  if (partFile_.write(buffer, read) != static_cast<size_t>(read)) {
    fail(Error::Write, "part write");
    return;
  }
  mbedtls_sha256_update(&hash_, buffer, read);
  receivedBytes_ += static_cast<uint32_t>(read);
  lastDataMs_ = nowMs;
  const uint8_t percent = progressPercent();
  if (percent != lastPercent_) { lastPercent_ = percent; changed_ = true; }
  if (receivedBytes_ == expected.bytes) setState(State::Verifying, nowMs);
}

void FontSetupService::finishFile() {
  const auto& expected = FontBundle::kFiles[fileIndex_];
  uint8_t actual[32];
  mbedtls_sha256_finish(&hash_, actual);
  hashOpen_ = false;
  partFile_.flush();
  partFile_.close();
  http_.end();
  downloadOpen_ = false;
  if (!hashMatches(actual, expected.sha256)) { fail(Error::Hash, "sha256 mismatch"); return; }
  char destination[48];
  snprintf(destination, sizeof(destination), "%s/%s", FontBundle::kDirectory, expected.name);
  SdCardSession::invalidate();
  if (SD_MMC.exists(destination) && !SD_MMC.remove(destination)) {
    fail(Error::Write, "replace old font");
    return;
  }
  if (!SD_MMC.rename(FontBundle::kPartPath, destination) || !fileLooksValid(expected)) {
    fail(Error::Write, "install font");
    return;
  }
  completedBytes_ += expected.bytes;
  receivedBytes_ = 0;
  ++fileIndex_;
  LogI("SETUP", "font installed file=%s sha256=ok", expected.name);
  changed_ = true;
}

void FontSetupService::closeDownload(bool removePart) {
  partFile_.close();
  if (downloadOpen_) http_.end();
  downloadOpen_ = false;
  if (hashOpen_) {
    mbedtls_sha256_free(&hash_);
    mbedtls_sha256_init(&hash_);
    hashOpen_ = false;
  }
  if (removePart) SD_MMC.remove(FontBundle::kPartPath);
}

void FontSetupService::fail(Error error, const char* stage) {
  LogE("SETUP", "font failure stage=%s error=%u file=%s received=%lu", stage,
       static_cast<unsigned>(error), badFile_, static_cast<unsigned long>(receivedBytes_));
  closeDownload(true);
  error_ = error;
  setState(State::Error, millis());
}

uint8_t FontSetupService::progressPercent() const {
  const uint32_t done = completedBytes_ + receivedBytes_;
  return static_cast<uint8_t>((static_cast<uint64_t>(done) * 100U) / FontBundle::kTotalBytes);
}

bool FontSetupService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}
