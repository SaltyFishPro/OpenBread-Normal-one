#pragma once

#include <stdint.h>

// 主频策略：默认满速，只有完全空闲时才降到 kIdleMhz。
// 需要满速的子系统通过 setBoost() 设置自己那一位，任一位置位即保持满速；
// 这样以后新增功能只要申请自己的理由位，不需要修改这里的判断。
//
// 下限取 80MHz 而不是更低：ESP32-S3 在 CPU=80MHz 时 APB 仍为 80MHz，
// SPI/I2C/I2S/UART 的分频不变；更低频率会连带降低 APB 并影响这些外设。
namespace CpuClock {

constexpr uint32_t kPerformanceMhz = 240;
constexpr uint32_t kIdleMhz = 80;

enum class Boost : uint8_t {
  Ui = 1U << 0,        // UI 交互、渲染与过渡
  Audio = 1U << 1,     // 音频播放与解码
  Radio = 1U << 2,     // 配网 / OTA / 蓝牙
  SelfTest = 1U << 3,  // 硬件自检
};

bool begin();
void setBoost(Boost reason, bool enabled);
void clearAllBoosts();
void apply();
uint32_t currentMhz();
bool isBoosted();

}  // namespace CpuClock
