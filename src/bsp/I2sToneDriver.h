#pragma once

#include <stdint.h>

#include <driver/i2s_std.h>

// 自检专用 I2S 测试音输出。
// 独立占用 I2S_NUM_1，只在测试期间存在；音乐库长期占用 I2S_NUM_0。
// 通道关闭后引脚路由由音乐播放路径的 setPinout() 恢复。
class I2sToneDriver {
public:
  enum class State : uint8_t {
    Idle,
    Playing,
    Finished,
    Error
  };

  bool start(uint32_t nowMs, uint16_t frequencyHz, uint16_t durationMs);
  void tick(uint32_t nowMs);
  void stop();
  State state() const;
  uint32_t errorCode() const;

private:
  static constexpr uint16_t kChunkFrames = 240;  // 44.1kHz 下约 5.4ms
  static constexpr uint32_t kSampleRate = 44100;
  static constexpr uint32_t kFadeMs = 10;
  static constexpr uint32_t kTailMs = 20;
  static constexpr uint32_t kDrainMs = 60;
  static constexpr int32_t kAmplitude = 100000000;  // 约 -26.7 dBFS，固定低音量

  bool openChannel();
  void closeChannel();
  void fillFrames(uint32_t firstFrame, uint16_t frameCount);

  i2s_chan_handle_t handle_ = nullptr;
  int32_t buffer_[kChunkFrames * 2] = {};
  State state_ = State::Idle;
  uint32_t errorCode_ = 0;
  uint32_t startMs_ = 0;
  uint32_t drainStartMs_ = 0;
  uint32_t frame_ = 0;
  uint32_t endFrame_ = 0;
  uint32_t audioFrames_ = 0;
  uint32_t phaseStep_ = 0;
  uint16_t durationMs_ = 0;
};
