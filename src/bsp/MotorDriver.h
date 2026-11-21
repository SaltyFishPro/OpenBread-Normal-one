#pragma once

#include <esp_timer.h>
#include <stdint.h>

// DRV2605L 触觉驱动（I2C 地址 0x5A + EN GPIO42）。
// EN 高电平使能芯片，I2C 配置为 RTP 模式后输出 8 位驱动值；
// 测试结束先拉低 EN 省电。EN 低时寄存器不复位，但无法读写 I2C。
class MotorDriver {
public:
  enum class State : uint8_t {
    Idle,
    Starting,
    Driving,
    Finished,
    Error
  };

  enum class Error : uint8_t {
    None,
    BusInit,
    NoAck,
    ConfigFailed
  };

  static constexpr uint16_t kMaxPulseMs = 300;
  // 当前硬件按 ERM（转子马达）配置；若实机为 LRA，需要改为 true 并核对闭环/谐振参数。
  static constexpr bool kActuatorIsLra = false;

  bool begin();
  bool startPulse(uint32_t nowMs, uint16_t durationMs);
  void tick(uint32_t nowMs);
  void stop();
  State state() const;
  Error error() const;
  bool isActive() const;
  bool consumeChanged();
  uint32_t lastPulseMs() const;

private:
  static void timerCallback(void* arg);
  void stopFromTimer();
  bool configureAndStart();
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t& value);
  void disable();
  void finish(uint32_t nowMs, bool byTimer);

  esp_timer_handle_t timer_ = nullptr;
  State state_ = State::Idle;
  Error error_ = Error::None;
  volatile bool enabled_ = false;
  volatile bool timerArmed_ = false;
  uint32_t startMs_ = 0;
  uint32_t driveStartMs_ = 0;
  uint16_t durationMs_ = 0;
  uint32_t lastPulseMs_ = 0;
  bool changed_ = false;
};
