#pragma once

#include <stdint.h>

#include "../bsp/ImuDriver.h"

// Samples the shared IMU only while a race is active.
class RacingSteeringService {
public:
  bool start(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void stop();
  float steering() const { return ready_ ? steering_ : 0.0f; }

private:
  static constexpr uint32_t kSampleIntervalMs = 20;
  static constexpr uint8_t kCalibrationSamples = 8;
  ImuDriver driver_;
  uint32_t nextSampleMs_ = 0;
  int32_t neutralSum_ = 0;
  int32_t filteredAy_ = 0;
  int16_t neutralAy_ = 0;
  float steering_ = 0.0f;
  uint8_t calibrationCount_ = 0;
  uint8_t failedReads_ = 0;
  bool active_ = false;
  bool ready_ = false;
};
