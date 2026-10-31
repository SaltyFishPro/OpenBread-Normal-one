#pragma once
#include <stdint.h>

class DisplayMonoTft;

// 横版飞机大战：上下键控制战机移动，子弹自动发射。
class AirplaneWarGame {
public:
  void enter();
  void exit();
  void tick(uint32_t nowMs, bool upPressed, bool downPressed);
  void handleOk();
  bool running() const { return state_ == State::Running; }
  bool consumeChanged();
  void render(DisplayMonoTft& display, int16_t yOffset) const;

private:
  enum class State : uint8_t { Stopped, Running, GameOver };
  struct Enemy { int16_t x = 0; int16_t y = 0; bool alive = false; };
  struct Bullet { int16_t x = 0; int16_t y = 0; bool alive = false; };
  struct Star { int16_t x = 0; int16_t y = 0; uint8_t speed = 1; };

  static constexpr uint8_t kMaxEnemies = 6;
  static constexpr uint8_t kMaxBullets = 4;
  static constexpr uint8_t kLives = 3;
  static constexpr uint8_t kStarCount = 22;

  void resetRun();
  void step(uint32_t nowMs);
  void movePlane(bool upPressed, bool downPressed);
  void spawnEnemy();
  void spawnBullet();
  void loseLife();
  void initStars();
  void scrollStars();
  void drawPlane(DisplayMonoTft& display, int16_t y) const;
  void drawEnemy(DisplayMonoTft& display, int16_t x, int16_t y) const;
  void drawBullet(DisplayMonoTft& display, int16_t x, int16_t y) const;
  void drawStars(DisplayMonoTft& display, int16_t yOffset) const;

  State state_ = State::Stopped;
  Enemy enemies_[kMaxEnemies];
  Bullet bullets_[kMaxBullets];
  Star stars_[kStarCount];
  int16_t planeY_ = 0;
  uint16_t score_ = 0;
  uint8_t lives_ = kLives;
  uint32_t lastStepMs_ = 0;
  uint32_t lastSpawnMs_ = 0;
  uint32_t lastFireMs_ = 0;
  uint32_t seed_ = 0;
  bool changed_ = false;
};
