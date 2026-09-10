#pragma once

#include <Arduino.h>

#include "../bsp/IicBusScanner.h"

class IicScanService {
public:
  enum class State : uint8_t {
    Idle,
    Powering,
    Settling,
    Scanning,
    Scanned,
    Error
  };

  bool begin();
  bool start(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void stop();
  void reset();

  State state() const;
  const IicBusScanner::Result& result() const;
  uint32_t lastScanMs() const;
  uint8_t currentAddress() const;
  bool isBusy() const;
  bool consumeChanged();

private:
  IicBusScanner scanner_;
  IicBusScanner::Result result_{};
  State state_ = State::Idle;
  uint32_t lastScanMs_ = 0;
  uint32_t nextActionMs_ = 0;
  uint8_t currentAddress_ = 0;
  bool changed_ = false;
};
