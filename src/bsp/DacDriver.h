#pragma once

#include <stdint.h>

class DacDriver {
public:
  // 音频 LDO 上电到 DAC 可稳定输出的等待时间。
  static constexpr uint32_t kPowerSettleMs = 100;

  bool begin();
  // 阻塞版本：上电并等待稳定后解除静音，音乐播放路径使用。
  bool powerOn();
  // 非阻塞版本：只拉高音频 LDO，由调用方等待 powerSettled() 后再解除静音。
  bool startPowerOn(uint32_t nowMs);
  bool powerSettled(uint32_t nowMs) const;
  bool finishPowerOn();
  void powerOff();
  void setMuted(bool muted);
  void setVolume(unsigned char percent);
  void updateOutputRoute(bool audioActive);
  bool headphonesInserted() const;
  bool powered() const;

private:
  bool initialized_ = false;
  bool powered_ = false;
  bool muted_ = true;
  bool speakerEnabled_ = false;
  bool powerSettling_ = false;
  uint32_t powerOnMs_ = 0;
};
