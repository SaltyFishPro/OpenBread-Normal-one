#include "IicScanService.h"

#include "../bsp/BoardConfig.h"

#ifndef OB_IIC_SCAN_LOG_ENABLED
#define OB_IIC_SCAN_LOG_ENABLED 1
#endif

#if OB_IIC_SCAN_LOG_ENABLED
#define IIC_SCAN_LOG(format, ...) \
  Serial.printf("[SELFTEST][IIC] " format "\r\n", ##__VA_ARGS__)
#define IIC_SCAN_ERROR(format, ...) \
  Serial.printf("[ERR][SELFTEST][IIC] " format "\r\n", ##__VA_ARGS__)
#else
#define IIC_SCAN_LOG(format, ...) ((void)0)
#define IIC_SCAN_ERROR(format, ...) ((void)0)
#endif

namespace {
constexpr uint32_t kPowerOnDelayMs = 100;
constexpr uint32_t kBusSettleDelayMs = 20;
constexpr uint8_t kFirstAddress = 0x03;
constexpr uint8_t kLastAddress = 0x77;
constexpr uint8_t kProgressNotifyStep = 8;
}

bool IicScanService::begin() { return true; }

bool IicScanService::start(uint32_t nowMs) {
  if (state_ == State::Powering || state_ == State::Settling || state_ == State::Scanning) {
    return false;
  }
  result_ = IicBusScanner::Result{};
  currentAddress_ = kFirstAddress;
  scanner_.powerOn();
  nextActionMs_ = nowMs + kPowerOnDelayMs;
  state_ = State::Powering;
  changed_ = true;
  IIC_SCAN_LOG("start SDA=%d SCL=%d clock=100000 pullups=esp32s3 sensor_ldo=%d",
               BoardConfig::kPinIicSda, BoardConfig::kPinIicScl,
               BoardConfig::kPinSensorLdoEn);
  return true;
}

void IicScanService::tick(uint32_t nowMs) {
  if (state_ == State::Powering && static_cast<int32_t>(nowMs - nextActionMs_) >= 0) {
    result_.busReady = scanner_.beginBus();
    if (!result_.busReady) {
      scanner_.end();
      state_ = State::Error;
      changed_ = true;
      IIC_SCAN_ERROR("bus init failed SDA=%d SCL=%d", BoardConfig::kPinIicSda,
                     BoardConfig::kPinIicScl);
      return;
    }
    nextActionMs_ = nowMs + kBusSettleDelayMs;
    state_ = State::Settling;
    changed_ = true;
    return;
  }

  if (state_ == State::Settling && static_cast<int32_t>(nowMs - nextActionMs_) >= 0) {
    state_ = State::Scanning;
    changed_ = true;
    return;
  }
  if (state_ != State::Scanning) {
    return;
  }

  const uint8_t error = scanner_.probe(currentAddress_);
  if (error == 0U) {
    if (result_.deviceCount < sizeof(result_.addresses)) {
      result_.addresses[result_.deviceCount++] = currentAddress_;
    } else {
      ++result_.overflowCount;
    }
    changed_ = true;
    IIC_SCAN_LOG("found address=0x%02X", currentAddress_);
  } else if (error != 2U) {
    ++result_.errorCount;
    changed_ = true;
    IIC_SCAN_ERROR("probe address=0x%02X error=%u", currentAddress_, error);
  }

  if (currentAddress_ >= kLastAddress) {
    scanner_.end();
    lastScanMs_ = nowMs;
    state_ = State::Scanned;
    changed_ = true;
    IIC_SCAN_LOG("finish found=%u overflow=%u errors=%u", result_.deviceCount,
                 result_.overflowCount, result_.errorCount);
    return;
  }
  ++currentAddress_;
  if ((currentAddress_ - kFirstAddress) % kProgressNotifyStep == 0U) {
    changed_ = true;
  }
}

void IicScanService::stop() {
  const bool active = state_ == State::Powering || state_ == State::Settling ||
                      state_ == State::Scanning;
  if (active) {
    scanner_.end();
    IIC_SCAN_LOG("cancel address=0x%02X", currentAddress_);
  }
  state_ = State::Idle;
  changed_ = true;
}

void IicScanService::reset() {
  stop();
  result_ = IicBusScanner::Result{};
  state_ = State::Idle;
  lastScanMs_ = 0;
  nextActionMs_ = 0;
  currentAddress_ = 0;
  changed_ = true;
}

IicScanService::State IicScanService::state() const { return state_; }

const IicBusScanner::Result& IicScanService::result() const { return result_; }

uint32_t IicScanService::lastScanMs() const { return lastScanMs_; }

uint8_t IicScanService::currentAddress() const { return currentAddress_; }

bool IicScanService::isBusy() const {
  return state_ == State::Powering || state_ == State::Settling || state_ == State::Scanning;
}

bool IicScanService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}
