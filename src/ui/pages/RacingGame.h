#pragma once

#include <stdint.h>

class DisplayMonoTft;

// Monochrome pseudo-3D racer. Left/right steer; hold Left to leave the page.
class RacingGame {
public:
  void enter();
  void exit();
  void tick(uint32_t nowMs, bool leftPressed, bool rightPressed, float tiltSteering);
  void handleOk();
  bool running() const { return running_ && !paused_; }
  bool paused() const { return running_ && paused_; }
  bool consumeChanged();
  void render(DisplayMonoTft& display, int16_t yOffset) const;

private:
  static constexpr uint16_t kSegmentCount = 200;
  static constexpr uint8_t kTrafficCount = 6;
  struct Segment {
    float curve = 0;
    float height = 0;
    uint8_t scenery = 0;
    int8_t scenerySide = 0;
    uint8_t building = 0;
    bool tunnel = false;
  };
  struct TrafficCar {
    float position = 0;
    float lane = 0;
    float speed = 0;
  };

  void resetRun(uint32_t nowMs);
  void buildTrack();
  void addSection(uint8_t enter, uint8_t hold, uint8_t leave, float curve, float rise);
  void updatePhysics(float dt, bool leftPressed, bool rightPressed, float tiltSteering);
  void updateTraffic(float dt);
  void checkCollisions(uint32_t nowMs, float dt);
  void registerImpact(uint32_t nowMs, const char* reason, float speedBefore);
  uint32_t nextRandom();
  uint16_t segmentAt(float position) const;
  float wrapPosition(float position) const;
  float playerTrackPosition() const;

  void drawSky(DisplayMonoTft& display, int16_t yOffset) const;
  void drawRoad(DisplayMonoTft& display, int16_t yOffset) const;
  void drawPlayer(DisplayMonoTft& display, int16_t yOffset) const;
  void drawHud(DisplayMonoTft& display, int16_t yOffset) const;

  Segment segments_[kSegmentCount]{};
  TrafficCar traffic_[kTrafficCount]{};
  uint16_t builtSegments_ = 0;
  uint32_t randomState_ = 1;
  uint32_t lastTickMs_ = 0;
  uint32_t impactStartMs_ = 0;
  uint32_t impactCount_ = 0;
  float position_ = 0;
  float previousPosition_ = 0;
  float playerX_ = 0;
  float speed_ = 0;
  float acceleration_ = 0;
  float lateralVelocity_ = 0;
  float lapSeconds_ = 0;
  float bestLapSeconds_ = 0;
  float skyOffset_ = 0;
  float distanceSinceLightChange_ = 0;
  uint8_t lap_ = 1;
  uint8_t lightPhase_ = 0;
  bool impactVisible_ = false;
  bool paused_ = false;
  bool running_ = false;
  bool changed_ = false;
};
