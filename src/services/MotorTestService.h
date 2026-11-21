#pragma once

#include <stdint.h>

#include "../bsp/MotorDriver.h"

// 设备自检的马达测试：执行一次不超过 300ms 的短振动，由用户确认是否感觉到。
// 驱动由 MotorDriver 的硬件定时器强制关闭，页面退出或超时也会立即停止。
class MotorTestService {
public:
  enum class State : uint8_t {
    Idle,
    Pulsing,
    WaitingConfirm,
    Passed,
    Failed
  };

  enum class Error : uint8_t {
    None,
    StartFailed,
    NotFelt,
    Timeout,
    ChipNoResponse,
    ChipConfig
  };

  static constexpr uint16_t kPulseMs = 300;
  static constexpr uint32_t kTimeoutMs = 1200;

  bool begin();
  bool start(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void cancel();
  void toggleChoice();
  bool confirm(uint32_t nowMs);

  State state() const;
  Error error() const;
  bool feltSelected() const;
  bool isActive() const;
  bool consumeChanged();
  uint32_t lastPulseMs() const;
  uint32_t lastDurationMs() const;

private:
  void fail(Error error, uint32_t nowMs);
  static uint16_t errorCode(Error error);

  MotorDriver motor_;
  State state_ = State::Idle;
  Error error_ = Error::None;
  uint32_t startMs_ = 0;
  uint32_t lastDurationMs_ = 0;
  bool feltSelected_ = true;
  bool changed_ = false;
};
