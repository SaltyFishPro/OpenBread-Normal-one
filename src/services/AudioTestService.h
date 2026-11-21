#pragma once

#include <stdint.h>

#include "../bsp/DacDriver.h"
#include "../bsp/I2sToneDriver.h"

class PeripheralPower;

// 设备自检的音频测试：播放 1kHz 短提示音，由用户确认是否听到。
// 音频 LDO 采用“记录原状态、测试时打开、结束后恢复”的所有权策略；
// I2S 独立占用 I2S_NUM_1，测试结束立即关闭通道。
class AudioTestService {
public:
  enum class State : uint8_t {
    Idle,
    Powering,
    Playing,
    WaitingConfirm,
    Passed,
    Failed
  };

  enum class Error : uint8_t {
    None,
    MusicBusy,
    DacPower,
    ToneInit,
    ToneWrite,
    NotHeard,
    Timeout
  };

  static constexpr uint16_t kToneFrequencyHz = 1000;
  static constexpr uint16_t kToneDurationMs = 500;
  static constexpr uint32_t kTimeoutMs = 3000;

  bool begin();
  bool start(uint32_t nowMs, PeripheralPower& power, bool musicBusy);
  void tick(uint32_t nowMs);
  void cancel();
  void toggleChoice();
  bool confirm(uint32_t nowMs);

  State state() const;
  Error error() const;
  bool heardSelected() const;
  // 音频硬件仍被本测试占用（上电稳定或正在播放）。
  bool isActive() const;
  bool consumeChanged();
  uint32_t lastDurationMs() const;
  bool headphonesInserted() const;

private:
  void releaseAudio();
  void fail(Error error, uint32_t nowMs);
  static uint16_t errorCode(Error error);

  DacDriver dac_;
  I2sToneDriver tone_;
  PeripheralPower* power_ = nullptr;
  State state_ = State::Idle;
  Error error_ = Error::None;
  uint32_t startMs_ = 0;
  uint32_t lastDurationMs_ = 0;
  bool heardSelected_ = true;
  bool audioWasEnabled_ = false;
  bool resourceHeld_ = false;
  bool changed_ = false;
};
