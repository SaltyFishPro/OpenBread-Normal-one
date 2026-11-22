#pragma once

#include <esp_timer.h>
#include <stdint.h>

// DRV2605L 触觉驱动（I2C 地址 0x5A + EN GPIO42）。
// 当前硬件为 LRA：使用闭环自动谐振 + ROM 波形库 6，逐次播放单个触觉效果。
// EN 高电平使能芯片，播放结束拉低 EN 省电；EN 低时寄存器不复位。
class MotorDriver {
public:
  enum class State : uint8_t {
    Idle,
    Starting,
    Playing,
    Finished,
    Error
  };

  enum class Error : uint8_t {
    None,
    BusInit,
    NoAck,
    ConfigFailed
  };

  static constexpr uint8_t kLibraryLra = 6;
  static constexpr uint16_t kMaxEffectMs = 2000;

  bool begin();
  // 播放 ROM 库中的单个效果；maxMs 是硬性上限，长振效果也会在该时间被强制停止。
  bool playEffect(uint32_t nowMs, uint8_t effectId, uint16_t maxMs);
  void tick(uint32_t nowMs);
  void stop();
  State state() const;
  Error error() const;
  bool isActive() const;
  bool consumeChanged();
  uint32_t lastEffectMs() const;
  bool lastCapped() const;

private:
  static void timerCallback(void* arg);
  void stopFromTimer();
  bool configureAndPlay();
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t& value);
  void disable();
  void finish(uint32_t nowMs, bool capped);

  esp_timer_handle_t timer_ = nullptr;
  State state_ = State::Idle;
  Error error_ = Error::None;
  volatile bool enabled_ = false;
  volatile bool timerArmed_ = false;
  uint32_t startMs_ = 0;
  uint32_t playStartMs_ = 0;
  uint32_t nextPollMs_ = 0;
  uint16_t maxMs_ = 0;
  uint32_t lastEffectMs_ = 0;
  uint8_t effectId_ = 0;
  bool lastCapped_ = false;
  bool changed_ = false;
};
