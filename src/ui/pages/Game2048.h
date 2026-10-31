#pragma once
#include <stdint.h>

class DisplayMonoTft;

// 2048 数字合成：方向键滑动合并，长按 Left 退出，OK 结束后重开。
class Game2048 {
public:
  void enter();
  void exit();
  void tick(uint32_t nowMs, bool leftPressed, bool rightPressed, bool upPressed,
            bool downPressed);
  void handleLeft(uint32_t nowMs);
  void handleOk();
  bool running() const { return state_ == State::Running; }
  bool needsAnimationFrame() const { return animationActive_; }
  bool consumeChanged();
  void render(DisplayMonoTft& display, int16_t yOffset) const;

private:
  enum class State : uint8_t { Stopped, Running, GameOver };
  static constexpr uint8_t kSize = 4;
  struct MovingTile {
    uint16_t value;
    uint8_t fromRow, fromCol, toRow, toCol;
  };

  void resetRun();
  void loadBest();
  void saveBest();
  void applyMove(uint8_t dir, uint32_t nowMs);
  bool move(uint8_t dir);
  void spawnTile();
  bool hasMoves() const;
  uint32_t nextRand();
  void renderCell(DisplayMonoTft& display, int16_t x, int16_t y, uint16_t v,
                  bool flash = false) const;

  State state_ = State::Stopped;
  uint16_t grid_[kSize][kSize];
  MovingTile moving_[kSize * kSize]{};
  uint16_t mergedMask_ = 0;
  uint8_t movingCount_ = 0;
  uint32_t animationStartMs_ = 0;
  uint32_t score_ = 0;
  uint32_t best_ = 0;
  bool prevRight_ = false;
  bool prevUp_ = false;
  bool prevDown_ = false;
  bool edgesReady_ = false;
  bool reached2048_ = false;
  bool animationActive_ = false;
  uint32_t seed_ = 0;
  bool changed_ = false;
};
