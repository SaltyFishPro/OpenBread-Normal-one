#pragma once

#include <stdint.h>

#include "../bsp/MotorDriver.h"

// 设备自检的马达测试：线性马达（LRA）触觉效果试听。
// 提供多种 DRV2605L ROM 库效果，用户可逐项试听并切换；
// 每次播放由硬件定时器强制截止，结束或退出后拉低 EN 省电。
class MotorTestService {
public:
  enum class State : uint8_t {
    Idle,
    Playing,
    Finished,
    Failed
  };

  enum class Error : uint8_t {
    None,
    StartFailed,
    Timeout,
    ChipNoResponse,
    ChipConfig
  };

  static constexpr uint8_t kModeCount = 5;
  static constexpr uint32_t kTimeoutSlackMs = 600;

  struct Mode {
    const char* name;
    const char* hint;
    uint8_t effectId;
    uint16_t maxMs;
  };

  bool begin();
  void selectPrev();
  void selectNext();
  bool play(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void cancel();

  State state() const;
  Error error() const;
  uint8_t selectedIndex() const;
  const char* modeName(uint8_t index) const;
  const char* modeHint(uint8_t index) const;
  uint16_t modeMaxMs(uint8_t index) const;
  bool isActive() const;
  bool consumeChanged();
  uint32_t lastPlayMs() const;
  bool lastCapped() const;

private:
  void fail(Error error, uint32_t nowMs);
  static uint16_t errorCode(Error error);
  static const Mode kModes[kModeCount];

  MotorDriver motor_;
  State state_ = State::Idle;
  Error error_ = Error::None;
  uint8_t selectedIndex_ = 0;
  uint32_t startMs_ = 0;
  uint32_t lastPlayMs_ = 0;
  bool changed_ = false;
};
