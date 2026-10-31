#include "RacingGame.h"

#include <Arduino.h>
#include <cmath>

#include "../../log/Log.h"

namespace {
constexpr float kSegmentLength = 200.0f;
constexpr float kTrackLength = 40000.0f;
constexpr float kCameraAhead = 840.0f;
constexpr float kMaxSpeed = 13000.0f;
constexpr uint32_t kMinStepMs = 24;
constexpr uint32_t kMaxStepMs = 100;
constexpr uint32_t kImpactIndicatorMs = 600;
constexpr uint32_t kImpactCooldownMs = 1000;

float clamp(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}
float smooth(float value) {
  value = clamp(value, 0.0f, 1.0f);
  return value * value * (3.0f - 2.0f * value);
}
}  // namespace

void RacingGame::enter() {
  resetRun(millis());
  paused_ = false;
  running_ = true;
  changed_ = true;
  LogI("GAME/RACE", "start segments=%u traffic=%u", kSegmentCount, kTrafficCount);
}

void RacingGame::exit() {
  if (!running_) return;
  running_ = false;
  paused_ = false;
  changed_ = true;
  LogI("GAME/RACE", "exit lap=%u impacts=%lu", lap_, static_cast<unsigned long>(impactCount_));
}

bool RacingGame::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void RacingGame::handleOk() {
  if (!running_) return;
  paused_ = !paused_;
  if (!paused_) lastTickMs_ = millis();
  changed_ = true;
  LogI("GAME/RACE", "pause=%u", paused_ ? 1U : 0U);
}

uint32_t RacingGame::nextRandom() {
  randomState_ = randomState_ * 1664525UL + 1013904223UL;
  return randomState_;
}

float RacingGame::wrapPosition(float position) const {
  while (position >= kTrackLength) position -= kTrackLength;
  while (position < 0.0f) position += kTrackLength;
  return position;
}

uint16_t RacingGame::segmentAt(float position) const {
  return static_cast<uint16_t>(wrapPosition(position) / kSegmentLength);
}

float RacingGame::playerTrackPosition() const {
  return wrapPosition(position_ + kCameraAhead);
}

void RacingGame::addSection(uint8_t enter, uint8_t hold, uint8_t leave, float curve, float rise) {
  const uint16_t count = static_cast<uint16_t>(enter + hold + leave);
  const float baseHeight = builtSegments_ ? segments_[builtSegments_ - 1].height : 0.0f;
  for (uint16_t n = 0; n < count && builtSegments_ < kSegmentCount; ++n) {
    float curveFactor = 1.0f;
    if (n < enter) curveFactor = smooth(static_cast<float>(n) / enter);
    else if (n >= enter + hold) curveFactor = 1.0f - smooth(static_cast<float>(n - enter - hold) / leave);
    Segment& segment = segments_[builtSegments_++];
    segment.curve = curve * curveFactor;
    segment.height = baseHeight + rise * smooth(static_cast<float>(n + 1) / count);
  }
}

void RacingGame::buildTrack() {
  for (Segment& segment : segments_) segment = Segment{};
  builtSegments_ = 0;
  addSection(4, 8, 4, 0.0f, 0.0f);
  int8_t pendingReturnDir = 0;
  float pendingReturnMagnitude = 0.0f;
  while (builtSegments_ < kSegmentCount - 28) {
    const uint32_t choice = nextRandom();
    const uint8_t enter = 4 + (choice & 3U);
    const uint8_t hold = 6 + ((choice >> 3) & 7U);
    const uint8_t leave = 4 + ((choice >> 6) & 3U);
    if (builtSegments_ + enter + hold + leave > kSegmentCount - 24) break;
    float curve = static_cast<int32_t>((choice >> 10) % 161U) / 10.0f - 8.0f;
    if (curve > -2.0f && curve < 2.0f) curve = 0.0f;
    float hill = 0.0f;
    if (pendingReturnDir != 0) {
      hill = pendingReturnDir * pendingReturnMagnitude;
      pendingReturnDir = 0;
    } else {
      const float height = segments_[builtSegments_ - 1].height;
      if (fabsf(height) > kSegmentLength * 4.0f) {
        hill = height > 0.0f ? -8.0f : 8.0f;
      } else {
        hill = static_cast<int32_t>((choice >> 18) % 25U) - 12.0f;
        if (fabsf(hill) < 6.0f) hill = 0.0f;
        if (hill != 0.0f) {
          pendingReturnDir = hill > 0.0f ? -1 : 1;
          pendingReturnMagnitude = fmaxf(6.0f, fabsf(hill) * 0.6f);
        }
      }
    }
    addSection(enter, hold, leave, curve, hill * kSegmentLength);
  }
  const uint16_t remaining = kSegmentCount - builtSegments_;
  const float lastHeight = segments_[builtSegments_ - 1].height;
  for (uint16_t n = 0; n < remaining; ++n) {
    Segment& segment = segments_[builtSegments_++];
    segment.height = lastHeight * (1.0f - smooth(static_cast<float>(n + 1) / remaining));
  }
  for (uint16_t i = kSegmentCount / 3; i < kSegmentCount / 3 + 60; ++i) segments_[i].tunnel = true;

  uint8_t leftRun = 0;
  uint8_t rightRun = 0;
  bool leftBuilding = false;
  bool rightBuilding = false;
  for (uint16_t i = 5; i < kSegmentCount; ++i) {
    Segment& segment = segments_[i];
    if (segment.tunnel) continue;
    if (leftRun == 0) {
      leftBuilding = (nextRandom() % 10U) < 6U;
      leftRun = static_cast<uint8_t>((leftBuilding ? 6U : 10U) + nextRandom() % 10U);
    }
    if (rightRun == 0) {
      rightBuilding = (nextRandom() % 10U) < 6U;
      rightRun = static_cast<uint8_t>((rightBuilding ? 6U : 10U) + nextRandom() % 10U);
    }
    segment.building = (leftBuilding ? 1U : 0U) | (rightBuilding ? 2U : 0U);
    --leftRun;
    --rightRun;
    if (segment.building == 0U && nextRandom() % 5U == 0U) {
      const uint32_t scenery = nextRandom();
      segment.scenery = static_cast<uint8_t>(1U + scenery % 4U);
      segment.scenerySide = (scenery & 8U) ? -1 : 1;
    }
  }
}

void RacingGame::resetRun(uint32_t nowMs) {
  randomState_ = nowMs | 1U;
  buildTrack();
  position_ = previousPosition_ = playerX_ = speed_ = acceleration_ = lateralVelocity_ = 0.0f;
  lapSeconds_ = bestLapSeconds_ = skyOffset_ = distanceSinceLightChange_ = 0.0f;
  lap_ = 1;
  lightPhase_ = 0;
  impactCount_ = 0;
  impactStartMs_ = 0;
  impactVisible_ = false;
  lastTickMs_ = nowMs;
  for (TrafficCar& car : traffic_) {
    const uint32_t value = nextRandom();
    car.position = static_cast<float>((value >> 8) % kSegmentCount) * kSegmentLength;
    car.lane = (static_cast<int32_t>((value >> 16) % 17U) - 8) * 0.1f;
    car.speed = kMaxSpeed * (0.2f + ((value >> 24) % 50U) * 0.01f);
  }
}

void RacingGame::tick(uint32_t nowMs, bool leftPressed, bool rightPressed, float tiltSteering) {
  if (!running_ || paused_) return;
  const uint32_t elapsed = nowMs - lastTickMs_;
  if (elapsed < kMinStepMs) return;
  lastTickMs_ = nowMs;
  const float dt = static_cast<float>(elapsed > kMaxStepMs ? kMaxStepMs : elapsed) * 0.001f;
  if (impactVisible_ && nowMs - impactStartMs_ >= kImpactIndicatorMs) impactVisible_ = false;
  updatePhysics(dt, leftPressed, rightPressed, tiltSteering);
  updateTraffic(dt);
  checkCollisions(nowMs, dt);
  changed_ = true;
}

void RacingGame::updatePhysics(float dt, bool leftPressed, bool rightPressed, float tiltSteering) {
  const uint16_t playerSegment = segmentAt(playerTrackPosition());
  const float speedRatio = speed_ / kMaxSpeed;
  if (speed_ < kMaxSpeed * 0.9f) acceleration_ = clamp(acceleration_ + 180.0f * dt, 0.0f, kMaxSpeed * 0.9f);
  else acceleration_ *= 1.0f - 0.9f * dt;
  const uint16_t previousSegment = (playerSegment + kSegmentCount - 1U) % kSegmentCount;
  const float slope = (segments_[playerSegment].height - segments_[previousSegment].height) / kSegmentLength;
  acceleration_ = clamp(acceleration_ - slope * 1600.0f * dt, -kMaxSpeed, kMaxSpeed);
  speed_ = clamp(speed_ * (1.0f - 0.12f * dt) + acceleration_ * dt, 0.0f, kMaxSpeed);

  const float curve = segments_[playerSegment].curve;
  const float steer = (leftPressed || rightPressed)
                          ? (rightPressed ? 1.0f : 0.0f) - (leftPressed ? 1.0f : 0.0f)
                          : tiltSteering;
  playerX_ += steer * 2.0f * dt * (0.25f + speedRatio);
  if (steer == 0.0f) {
    const float target = clamp(-curve * 0.12f, -0.8f, 0.8f);
    playerX_ += (target - playerX_) * 1.8f * dt;
  }
  lateralVelocity_ = (lateralVelocity_ + curve * 0.54f * speedRatio * dt) * (1.0f - 3.2f * dt);
  playerX_ -= lateralVelocity_ * dt + curve * 0.27f * speedRatio * speedRatio * dt;
  playerX_ = clamp(playerX_, -2.5f, 2.5f);
  if (fabsf(playerX_) > 1.0f) speed_ *= 1.0f - 1.8f * dt;

  const float previous = position_;
  previousPosition_ = position_;
  const float travel = speed_ * dt;
  position_ = wrapPosition(position_ + travel);
  lapSeconds_ += dt;
  if (position_ < previous && previous > kTrackLength * 0.9f) {
    if (lapSeconds_ > 5.0f) {
      if (bestLapSeconds_ <= 0.0f || lapSeconds_ < bestLapSeconds_) bestLapSeconds_ = lapSeconds_;
      lap_ = lap_ == 3U ? 1U : lap_ + 1U;
      LogI("GAME/RACE", "lap=%u time_ms=%lu", lap_, static_cast<unsigned long>(lapSeconds_ * 1000.0f));
    }
    lapSeconds_ = 0.0f;
  }
  distanceSinceLightChange_ += travel;
  if (distanceSinceLightChange_ >= 180000.0f) {
    distanceSinceLightChange_ -= 180000.0f;
    lightPhase_ = (lightPhase_ + 1U) % 3U;
  }
  skyOffset_ += curve * speedRatio * 150.0f * dt;
}

void RacingGame::updateTraffic(float dt) {
  for (TrafficCar& car : traffic_) {
    car.position = wrapPosition(car.position + car.speed * dt);
    if (nextRandom() % 200U < 2U) {
      const int8_t change = static_cast<int8_t>(nextRandom() % 3U) - 1;
      car.lane = clamp(car.lane + change * 0.1f, -0.8f, 0.8f);
    }
  }
}

void RacingGame::registerImpact(uint32_t nowMs, const char* reason, float speedBefore) {
  impactVisible_ = true;
  impactStartMs_ = nowMs;
  ++impactCount_;
  LogI("GAME/RACE", "impact reason=%s speed=%u->%u x=%d lap=%u", reason,
       static_cast<unsigned>(speedBefore * 300.0f / kMaxSpeed),
       static_cast<unsigned>(speed_ * 300.0f / kMaxSpeed),
       static_cast<int>(playerX_ * 100.0f), lap_);
}

void RacingGame::checkCollisions(uint32_t nowMs, float dt) {
  if (impactCount_ != 0U && nowMs - impactStartMs_ < kImpactCooldownMs) return;
  const float playerPosition = playerTrackPosition();
  const uint16_t playerSegment = segmentAt(playerPosition);
  for (const TrafficCar& car : traffic_) {
    if (speed_ <= car.speed) continue;
    const float ahead = wrapPosition(car.position - playerPosition);
    const float previousCarPosition = wrapPosition(car.position - car.speed * dt);
    const float previousAhead = wrapPosition(previousCarPosition - previousPosition_ - kCameraAhead);
    const float closingDistance = (speed_ - car.speed) * dt;
    if (ahead > 150.0f && previousAhead > closingDistance + 150.0f) continue;
    if (fabsf(playerX_ - car.lane) >= 0.30f) continue;
    const float impactSpeed = speed_;
    speed_ = fmaxf(car.speed * 0.75f, speed_ * 0.45f);
    position_ = wrapPosition(position_ - speed_ * 0.05f);
    registerImpact(nowMs, "traffic", impactSpeed);
    return;
  }
  if (segments_[playerSegment].tunnel && fabsf(playerX_) > 0.95f) {
    const float impactSpeed = speed_;
    playerX_ = clamp(playerX_, -0.95f, 0.95f);
    lateralVelocity_ = 0.0f;
    speed_ *= 0.55f;
    registerImpact(nowMs, "tunnel", impactSpeed);
    return;
  }
  const Segment& segment = segments_[playerSegment];
  if (segment.scenery && fabsf(playerX_ - segment.scenerySide * 1.5f) < 0.55f) {
    const float impactSpeed = speed_;
    speed_ *= 0.5f;
    registerImpact(nowMs, "scenery", impactSpeed);
    return;
  }
  if (fabsf(playerX_) >= 2.4f) {
    const float impactSpeed = speed_;
    speed_ *= 0.75f;
    registerImpact(nowMs, "offroad", impactSpeed);
  }
}
