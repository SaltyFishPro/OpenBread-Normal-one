#include "RacingSteeringService.h"

#include "../log/Log.h"

namespace {
constexpr int32_t kDeadZone = 220;
constexpr int32_t kFullSteer = 1650;
constexpr uint8_t kMaxFailedReads = 3;
}

bool RacingSteeringService::start(uint32_t nowMs) {
  stop();
  driver_.powerOn();  // The LDO also supplies other devices; leave it enabled on stop.
  if (!driver_.beginBus()) {
    LogE("GAME/RACE/IMU", "start failed stage=bus; buttons remain available");
    return false;
  }
  if (!driver_.configureAccelerometer()) {
    LogE("GAME/RACE/IMU", "start failed stage=configure addr=0x%02X; buttons remain available",
         driver_.address());
    driver_.end();
    return false;
  }
  active_ = true;
  ready_ = false;
  calibrationCount_ = 0;
  neutralSum_ = 0;
  filteredAy_ = 0;
  neutralAy_ = 0;
  steering_ = 0.0f;
  failedReads_ = 0;
  nextSampleMs_ = nowMs + 80U;
  LogI("GAME/RACE/IMU", "start addr=0x%02X axis=ay odr=125Hz sample=20ms ctrl7=0x01",
       driver_.address());
  return true;
}

void RacingSteeringService::tick(uint32_t nowMs) {
  if (!active_ || static_cast<int32_t>(nowMs - nextSampleMs_) < 0) return;
  nextSampleMs_ = nowMs + kSampleIntervalMs;
  int16_t ax = 0;
  int16_t ay = 0;
  int16_t az = 0;
  if (!driver_.readAcceleration(ax, ay, az)) {
    if (++failedReads_ >= kMaxFailedReads) {
      LogE("GAME/RACE/IMU", "read failed count=%u; falling back to buttons", failedReads_);
      stop();
    }
    return;
  }
  failedReads_ = 0;
  if (calibrationCount_ < kCalibrationSamples) {
    neutralSum_ += ay;
    if (++calibrationCount_ == kCalibrationSamples) {
      neutralAy_ = static_cast<int16_t>(neutralSum_ / kCalibrationSamples);
      filteredAy_ = neutralAy_;
      ready_ = true;
      LogI("GAME/RACE/IMU", "calibrated ay=%d ax=%d az=%d deadzone=%ld full=%ld",
           neutralAy_, ax, az, static_cast<long>(kDeadZone), static_cast<long>(kFullSteer));
    }
    return;
  }

  filteredAy_ += (static_cast<int32_t>(ay) - filteredAy_) / 4;
  int32_t delta = filteredAy_ - neutralAy_;
  const bool negative = delta < 0;
  if (negative) delta = -delta;
  if (delta <= kDeadZone) {
    steering_ = 0.0f;
    return;
  }
  if (delta > kFullSteer) delta = kFullSteer;
  steering_ = static_cast<float>(delta - kDeadZone) / (kFullSteer - kDeadZone);
  if (negative) steering_ = -steering_;
}

void RacingSteeringService::stop() {
  if (active_) {
    driver_.end();
    LogI("GAME/RACE/IMU", "stopped ctrl7=0x00 sensor_ldo=1");
  }
  active_ = false;
  ready_ = false;
  steering_ = 0.0f;
}
