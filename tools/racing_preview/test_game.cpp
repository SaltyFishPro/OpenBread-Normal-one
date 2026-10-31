#include <cmath>
#include <cstdio>

#define private public
#include "src/ui/pages/RacingGame.h"
#undef private

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

void prepare(RacingGame& game) {
  game.enter();
  game.position_ = 0.0f;
  game.speed_ = 5000.0f;
  game.acceleration_ = 0.0f;
  game.playerX_ = 0.0f;
  for (auto& car : game.traffic_) car.position = 20000.0f;
  const uint16_t segment = game.segmentAt(game.playerTrackPosition());
  game.segments_[segment].curve = 0.0f;
  game.segments_[segment].tunnel = false;
  game.segments_[segment].scenery = 1;
  game.segments_[segment].scenerySide = 1;
}
}  // namespace

int main() {
  RacingGame game;
  prepare(game);
  game.playerX_ = -1.5f;
  game.checkCollisions(1000, 0.03f);
  if (!near(game.speed_, 5000.0f) || game.impactVisible_) return 1;

  game.playerX_ = 1.5f;
  game.lap_ = 2;
  game.checkCollisions(1000, 0.03f);
  if (!near(game.speed_, 2500.0f) || !game.impactVisible_ || game.lap_ != 2U) return 2;
  const float position = game.position_;
  game.tick(1030, false, false, 0.0f);
  if (game.position_ <= position || game.speed_ <= 0.0f || game.lap_ != 2U) return 5;
  game.checkCollisions(1050, 0.03f);
  if (game.impactCount_ != 1U) return 6;
  game.playerX_ = 0.0f;
  game.tick(1700, false, false, 0.0f);
  if (game.impactVisible_ || game.lap_ != 2U || game.speed_ <= 0.0f) return 7;

  prepare(game);
  game.segments_[game.segmentAt(game.playerTrackPosition())].scenery = 0;
  const float startingX = game.playerX_;
  game.updatePhysics(0.03f, false, false, -1.0f);
  if (!(game.playerX_ < startingX)) return 3;
  game.playerX_ = 0.0f;
  game.updatePhysics(0.03f, false, false, 1.0f);
  if (!(game.playerX_ > startingX)) return 4;

  std::puts("PASS: impact slows without stopping; cooldown, lap progress and tilt steering");
  return 0;
}
