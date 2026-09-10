#include "DacDriver.h"

#include <Arduino.h>

#include "BoardConfig.h"

namespace {
constexpr uint32_t kAudioLdoSettleMs = 100;

#ifndef AUDIO_DEBUG_LOG
#define AUDIO_DEBUG_LOG 1
#endif

#if AUDIO_DEBUG_LOG
#define AUDIO_LOG(fmt, ...) Serial.printf("[AUDIO] " fmt "\n", ##__VA_ARGS__)
#else
#define AUDIO_LOG(fmt, ...) \
  do {                      \
  } while (0)
#endif

void setOutput(int pin, int level) {
  if (pin < 0) {
    return;
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, level);
}
}  // namespace

bool DacDriver::begin() {
  pinMode(BoardConfig::kPinHpCon, INPUT_PULLUP);
  setOutput(BoardConfig::kPinAudioLdoEn, LOW);
  setOutput(BoardConfig::kPinPcmXsmt, LOW);
  setOutput(BoardConfig::kPinNsCtrl, LOW);
  initialized_ = true;
  powered_ = false;
  muted_ = true;
  speakerEnabled_ = false;
  return true;
}

bool DacDriver::powerOn() {
  if (!initialized_ && !begin()) {
    return false;
  }

  if (!powered_) {
    setOutput(BoardConfig::kPinAudioLdoEn, HIGH);
    delay(kAudioLdoSettleMs);
    powered_ = true;
  }

  setMuted(false);
  updateOutputRoute(true);
  AUDIO_LOG("power on ldo=1 xsmt=1");
  return true;
}

void DacDriver::powerOff() {
  if (!initialized_) {
    (void)begin();
  }

  setMuted(true);
  setOutput(BoardConfig::kPinNsCtrl, LOW);
  setOutput(BoardConfig::kPinAudioLdoEn, LOW);
  powered_ = false;
  speakerEnabled_ = false;
  AUDIO_LOG("power off ldo=0 xsmt=0 ns=0");
}

void DacDriver::setMuted(bool muted) {
  muted_ = muted;
  setOutput(BoardConfig::kPinPcmXsmt, muted ? LOW : HIGH);
}

void DacDriver::setVolume(unsigned char percent) { (void)percent; }

void DacDriver::updateOutputRoute(bool audioActive) {
  if (!initialized_) {
    (void)begin();
  }

  if (!audioActive || headphonesInserted()) {
    setOutput(BoardConfig::kPinNsCtrl, LOW);
    speakerEnabled_ = false;
    AUDIO_LOG("output=headphone ns=0");
    return;
  }

  setOutput(BoardConfig::kPinNsCtrl, HIGH);
  speakerEnabled_ = true;
  AUDIO_LOG("output=speaker ns=1");
}

bool DacDriver::headphonesInserted() const { return digitalRead(BoardConfig::kPinHpCon) == LOW; }

bool DacDriver::powered() const { return powered_; }
