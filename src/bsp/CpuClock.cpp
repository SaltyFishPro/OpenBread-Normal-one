#include "CpuClock.h"

#include <Arduino.h>

#ifndef OB_CPU_IDLE_SCALING_ENABLED
#define OB_CPU_IDLE_SCALING_ENABLED 1
#endif

#ifndef OB_CPU_CLOCK_LOG_ENABLED
#define OB_CPU_CLOCK_LOG_ENABLED 1
#endif

namespace CpuClock {
namespace {

uint8_t boostMask = 0;
uint32_t activeMhz = kPerformanceMhz;

void logSwitch(uint32_t mhz, uint8_t mask) {
#if OB_CPU_CLOCK_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.printf("[CPU] %luMHz apb=%luMHz boost_mask=0x%02X\r\n",
                static_cast<unsigned long>(mhz),
                static_cast<unsigned long>(getApbFrequency() / 1000000UL),
                static_cast<unsigned>(mask));
#else
  (void)mhz;
  (void)mask;
#endif
}

void setFrequency(uint32_t mhz) {
  if (activeMhz == mhz) {
    return;
  }
  if (!setCpuFrequencyMhz(mhz)) {
    return;
  }
  activeMhz = getCpuFrequencyMhz();
  logSwitch(activeMhz, boostMask);
}

}  // namespace

bool begin() {
  boostMask = 0;
  activeMhz = getCpuFrequencyMhz();
  setFrequency(kPerformanceMhz);
  return true;
}

void setBoost(Boost reason, bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(reason);
  if (enabled) {
    boostMask = static_cast<uint8_t>(boostMask | bit);
  } else {
    boostMask = static_cast<uint8_t>(boostMask & static_cast<uint8_t>(~bit));
  }
}

void clearAllBoosts() { boostMask = 0; }

void apply() {
#if OB_CPU_IDLE_SCALING_ENABLED
  const uint32_t target = (boostMask != 0) ? kPerformanceMhz : kIdleMhz;
#else
  const uint32_t target = kPerformanceMhz;
#endif
  setFrequency(target);
}

uint32_t currentMhz() { return activeMhz; }

bool isBoosted() { return boostMask != 0; }

}  // namespace CpuClock
