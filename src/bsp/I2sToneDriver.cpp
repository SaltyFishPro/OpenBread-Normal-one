#include "I2sToneDriver.h"

#include <Arduino.h>
#include <math.h>

#include "BoardConfig.h"
#include "../log/Log.h"

namespace {
constexpr float kPhaseToRadians = 6.28318530718f / 4294967296.0f;  // 2π / 2^32
constexpr uint32_t kBytesPerFrame = 2U * sizeof(int32_t);          // 32bit 立体声
}  // namespace

bool I2sToneDriver::openChannel() {
  i2s_chan_config_t channelConfig = {};
  channelConfig.id = I2S_NUM_1;
  channelConfig.role = I2S_ROLE_MASTER;
  channelConfig.dma_desc_num = 4;
  channelConfig.dma_frame_num = kChunkFrames;
  channelConfig.auto_clear = true;

  esp_err_t err = i2s_new_channel(&channelConfig, &handle_, nullptr);
  if (err != ESP_OK) {
    errorCode_ = static_cast<uint32_t>(err);
    LogE("SELFTEST/AUDIO", "i2s_new_channel failed err=%d", static_cast<int>(err));
    return false;
  }

  // 与音乐库保持同一 slot 格式（32bit 立体声），BCLK = 64fs，PCM5102A 可直接识别。
  i2s_std_config_t standardConfig = {};
  standardConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate);
  standardConfig.slot_cfg =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  standardConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  standardConfig.gpio_cfg.bclk = static_cast<gpio_num_t>(BoardConfig::kPinPcmBck);
  standardConfig.gpio_cfg.ws = static_cast<gpio_num_t>(BoardConfig::kPinPcmLrck);
  standardConfig.gpio_cfg.dout = static_cast<gpio_num_t>(BoardConfig::kPinPcmDin);
  standardConfig.gpio_cfg.din = I2S_GPIO_UNUSED;

  err = i2s_channel_init_std_mode(handle_, &standardConfig);
  if (err == ESP_OK) {
    err = i2s_channel_enable(handle_);
  }
  if (err != ESP_OK) {
    errorCode_ = static_cast<uint32_t>(err);
    i2s_del_channel(handle_);
    handle_ = nullptr;
    LogE("SELFTEST/AUDIO", "i2s init failed err=%d bclk=%d lrck=%d dout=%d", static_cast<int>(err),
         BoardConfig::kPinPcmBck, BoardConfig::kPinPcmLrck, BoardConfig::kPinPcmDin);
    return false;
  }

  LogI("SELFTEST/AUDIO", "i2s ready port=1 rate=%lu bclk=%d lrck=%d dout=%d",
       static_cast<unsigned long>(kSampleRate), BoardConfig::kPinPcmBck,
       BoardConfig::kPinPcmLrck, BoardConfig::kPinPcmDin);
  return true;
}

void I2sToneDriver::closeChannel() {
  if (handle_ == nullptr) {
    return;
  }
  i2s_channel_disable(handle_);
  i2s_del_channel(handle_);
  handle_ = nullptr;
}

bool I2sToneDriver::start(uint32_t nowMs, uint16_t frequencyHz, uint16_t durationMs) {
  if (state_ == State::Playing) {
    return false;
  }
  if (frequencyHz == 0U || durationMs == 0U) {
    return false;
  }
  if (!openChannel()) {
    state_ = State::Error;
    return false;
  }

  state_ = State::Playing;
  errorCode_ = 0;
  startMs_ = nowMs;
  drainStartMs_ = 0;
  frame_ = 0;
  durationMs_ = durationMs;
  audioFrames_ = static_cast<uint32_t>((static_cast<uint64_t>(durationMs) * kSampleRate) / 1000U);
  endFrame_ = static_cast<uint32_t>(
      (static_cast<uint64_t>(durationMs + kTailMs) * kSampleRate) / 1000U);
  phaseStep_ = static_cast<uint32_t>((static_cast<uint64_t>(frequencyHz) << 32) / kSampleRate);
  LogI("SELFTEST/AUDIO", "tone start freq=%u duration_ms=%u amplitude=%ld",
       static_cast<unsigned>(frequencyHz), static_cast<unsigned>(durationMs),
       static_cast<long>(kAmplitude));
  return true;
}

void I2sToneDriver::fillFrames(uint32_t firstFrame, uint16_t frameCount) {
  const uint32_t fadeFrames = (kFadeMs * kSampleRate) / 1000U;
  uint32_t phase = static_cast<uint32_t>(firstFrame * static_cast<uint64_t>(phaseStep_));

  for (uint16_t index = 0; index < frameCount; ++index) {
    const uint32_t frame = firstFrame + index;
    int32_t sample = 0;
    if (frame < audioFrames_) {
      const float sine = sinf(static_cast<float>(phase) * kPhaseToRadians);
      phase += phaseStep_;

      int32_t gain = 1024;
      if (frame < fadeFrames) {
        gain = static_cast<int32_t>((frame * 1024U) / fadeFrames);
      } else if (frame + fadeFrames >= audioFrames_) {
        const uint32_t remain = audioFrames_ - frame;
        gain = static_cast<int32_t>((remain * 1024U) / fadeFrames);
      }

      const float amplitude =
          static_cast<float>(kAmplitude) * (static_cast<float>(gain) / 1024.0f);
      sample = static_cast<int32_t>(sine * amplitude);
    }
    buffer_[index * 2] = sample;
    buffer_[index * 2 + 1] = sample;
  }
}

void I2sToneDriver::tick(uint32_t nowMs) {
  if (state_ != State::Playing) {
    return;
  }
  if (handle_ == nullptr) {
    state_ = State::Error;
    errorCode_ = 0;
    LogE("SELFTEST/AUDIO", "i2s handle missing frame=%lu", static_cast<unsigned long>(frame_));
    return;
  }

  while (frame_ < endFrame_) {
    const uint32_t remain = endFrame_ - frame_;
    const uint16_t frames = static_cast<uint16_t>(remain < kChunkFrames ? remain : kChunkFrames);
    fillFrames(frame_, frames);

    size_t written = 0;
    const esp_err_t err =
        i2s_channel_write(handle_, buffer_, static_cast<size_t>(frames) * kBytesPerFrame, &written, 0);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
      errorCode_ = static_cast<uint32_t>(err);
      closeChannel();
      state_ = State::Error;
      LogE("SELFTEST/AUDIO", "i2s write failed err=%d frame=%lu", static_cast<int>(err),
           static_cast<unsigned long>(frame_));
      return;
    }
    if (written < kBytesPerFrame) {
      break;  // DMA 已满，等下一轮 tick 继续
    }
    frame_ += static_cast<uint32_t>(written / kBytesPerFrame);
  }

  if (frame_ < endFrame_) {
    return;
  }
  if (drainStartMs_ == 0U) {
    drainStartMs_ = nowMs;
  }
  if (nowMs - drainStartMs_ < kDrainMs) {
    return;
  }

  closeChannel();
  state_ = State::Finished;
  LogI("SELFTEST/AUDIO", "tone finished duration_ms=%u", static_cast<unsigned>(durationMs_));
}

void I2sToneDriver::stop() {
  const bool wasPlaying = state_ == State::Playing;
  closeChannel();
  state_ = State::Idle;
  if (wasPlaying) {
    LogI("SELFTEST/AUDIO", "tone stop frame=%lu", static_cast<unsigned long>(frame_));
  }
}

I2sToneDriver::State I2sToneDriver::state() const { return state_; }

uint32_t I2sToneDriver::errorCode() const { return errorCode_; }
