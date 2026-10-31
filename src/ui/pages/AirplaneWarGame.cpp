#include "AirplaneWarGame.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../log/Log.h"

namespace {
constexpr uint32_t kStepMs = 32;
constexpr uint32_t kSpawnMs = 650;
constexpr uint32_t kFireMs = 260;
constexpr int16_t kPlaneX = 26;
constexpr int16_t kPlaneW = 10;
constexpr int16_t kPlaneH = 14;
constexpr int16_t kPlaneHalfH = kPlaneH / 2;
constexpr int16_t kPlayTop = 54;
constexpr int16_t kPlayBottom = 154;
constexpr int16_t kPlayLeft = 8;
constexpr int16_t kPlayRight = 374;
constexpr int16_t kPlaneCenterY = (kPlayTop + kPlayBottom) / 2;
constexpr int16_t kEnemySize = 9;
constexpr int16_t kBulletW = 6;
constexpr int16_t kBulletH = 2;
constexpr int16_t kEnemyStep = 2;
constexpr int16_t kBulletStep = 6;
constexpr int16_t kPlaneStep = 4;

bool overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
             int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}
}  // namespace

void AirplaneWarGame::enter() {
  resetRun();
  state_ = State::Running;
  changed_ = true;
  LogI("GAME", "start");
}

void AirplaneWarGame::exit() {
  if (state_ == State::Stopped) return;
  state_ = State::Stopped;
  changed_ = true;
  LogI("GAME", "exit score=%u", static_cast<unsigned>(score_));
}

void AirplaneWarGame::tick(uint32_t nowMs, bool upPressed, bool downPressed) {
  if (state_ != State::Running) return;
  if (static_cast<int32_t>(nowMs - lastStepMs_) < kStepMs) return;
  lastStepMs_ = nowMs;
  movePlane(upPressed, downPressed);
  step(nowMs);
}

void AirplaneWarGame::handleOk() {
  if (state_ == State::GameOver) {
    resetRun();
    state_ = State::Running;
    changed_ = true;
    LogI("GAME", "restart");
  }
}

void AirplaneWarGame::resetRun() {
  score_ = 0;
  lives_ = kLives;
  for (Enemy& e : enemies_) e = Enemy{};
  for (Bullet& b : bullets_) b = Bullet{};
  planeY_ = kPlaneCenterY;
  lastStepMs_ = 0;
  lastSpawnMs_ = 0;
  lastFireMs_ = 0;
  seed_ = 0;
  initStars();
}

void AirplaneWarGame::movePlane(bool upPressed, bool downPressed) {
  if (upPressed && !downPressed) {
    planeY_ = static_cast<int16_t>(planeY_ - kPlaneStep);
  } else if (downPressed && !upPressed) {
    planeY_ = static_cast<int16_t>(planeY_ + kPlaneStep);
  }
  if (planeY_ < kPlayTop) planeY_ = kPlayTop;
  if (planeY_ > kPlayBottom - kPlaneH) planeY_ = kPlayBottom - kPlaneH;
}

void AirplaneWarGame::step(uint32_t nowMs) {
  scrollStars();
  for (Enemy& e : enemies_) {
    if (!e.alive) continue;
    e.x -= kEnemyStep;
    if (e.x + kEnemySize < kPlayLeft) {
      e.alive = false;  // 躲过敌机，不扣生命
    }
  }
  for (Bullet& b : bullets_) {
    if (!b.alive) continue;
    b.x += kBulletStep;
    if (b.x > kPlayRight) b.alive = false;
  }
  for (Bullet& b : bullets_) {
    if (!b.alive) continue;
    for (Enemy& e : enemies_) {
      if (!e.alive) continue;
      if (overlap(b.x, b.y, kBulletW, kBulletH, e.x, e.y, kEnemySize, kEnemySize)) {
        b.alive = false;
        e.alive = false;
        ++score_;
        break;
      }
    }
  }
  for (Enemy& e : enemies_) {
    if (!e.alive) continue;
    if (overlap(kPlaneX, planeY_, kPlaneW, kPlaneH, e.x, e.y, kEnemySize, kEnemySize)) {
      e.alive = false;
      loseLife();
      if (state_ != State::Running) return;
    }
  }
  if (static_cast<int32_t>(nowMs - lastFireMs_) >= kFireMs) {
    lastFireMs_ = nowMs;
    spawnBullet();
  }
  if (static_cast<int32_t>(nowMs - lastSpawnMs_) >= kSpawnMs) {
    lastSpawnMs_ = nowMs;
    spawnEnemy();
  }
}

void AirplaneWarGame::spawnEnemy() {
  Enemy* slot = nullptr;
  for (Enemy& e : enemies_) {
    if (!e.alive) { slot = &e; break; }
  }
  if (slot == nullptr) return;
  seed_ = seed_ * 1664525UL + 1013904223UL;
  const int16_t range = static_cast<int16_t>(kPlayBottom - kPlayTop - kEnemySize + 1);
  slot->y = static_cast<int16_t>(kPlayTop + static_cast<int16_t>(seed_ % static_cast<uint32_t>(range)));
  slot->x = kPlayRight;
  slot->alive = true;
}

void AirplaneWarGame::spawnBullet() {
  Bullet* slot = nullptr;
  for (Bullet& b : bullets_) {
    if (!b.alive) { slot = &b; break; }
  }
  if (slot == nullptr) return;
  slot->x = static_cast<int16_t>(kPlaneX + kPlaneW);
  slot->y = static_cast<int16_t>(planeY_ + kPlaneHalfH - kBulletH / 2);
  slot->alive = true;
}

void AirplaneWarGame::loseLife() {
  if (lives_ > 0U) --lives_;
  changed_ = true;
  if (lives_ == 0U) {
    state_ = State::GameOver;
    LogI("GAME", "game over score=%u", static_cast<unsigned>(score_));
  }
}

void AirplaneWarGame::initStars() {
  const uint16_t w = static_cast<uint16_t>(kPlayRight - kPlayLeft);
  const uint16_t h = static_cast<uint16_t>(kPlayBottom - kPlayTop);
  for (uint8_t i = 0; i < kStarCount; ++i) {
    stars_[i].x = static_cast<int16_t>(kPlayLeft + (i * 53U) % w);
    stars_[i].y = static_cast<int16_t>(kPlayTop + (i * 37U + 7U) % h);
    stars_[i].speed = static_cast<uint8_t>((i % 3U) + 1U);
  }
}

void AirplaneWarGame::scrollStars() {
  for (uint8_t i = 0; i < kStarCount; ++i) {
    stars_[i].x = static_cast<int16_t>(stars_[i].x - stars_[i].speed);
    if (stars_[i].x < kPlayLeft) {
      stars_[i].x = static_cast<int16_t>(kPlayRight - 1);
    }
  }
}

bool AirplaneWarGame::consumeChanged() {
  const bool c = changed_;
  changed_ = false;
  return c;
}

void AirplaneWarGame::render(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  DetailHeader::render(display, "飞机世界大战", yOffset);

  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line[32];
  snprintf(line, sizeof(line), "得分:%u", static_cast<unsigned>(score_));
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 46), line);
  snprintf(line, sizeof(line), "战机:%u", static_cast<unsigned>(lives_));
  const int16_t livesW = text.getUTF8Width(line);
  text.drawUTF8(static_cast<int16_t>(width - livesW - 8), static_cast<int16_t>(yOffset + 46), line);

  canvas.drawRectangle(6, static_cast<int16_t>(yOffset + 52), width - 7,
                       static_cast<int16_t>(yOffset + 158), ST7305_COLOR_BLACK);

  drawStars(display, yOffset);
  drawPlane(display, static_cast<int16_t>(yOffset + planeY_));

  if (state_ == State::Running) {
    for (const Enemy& e : enemies_) {
      if (!e.alive) continue;
      drawEnemy(display, e.x, static_cast<int16_t>(yOffset + e.y));
    }
    for (const Bullet& b : bullets_) {
      if (!b.alive) continue;
      drawBullet(display, b.x, static_cast<int16_t>(yOffset + b.y));
    }
  }

  if (state_ == State::GameOver) {
    const char* over = "游戏结束";
    const int16_t overW = text.getUTF8Width(over);
    text.drawUTF8(static_cast<int16_t>((width - overW) / 2), static_cast<int16_t>(yOffset + 82), over);
    snprintf(line, sizeof(line), "得分:%u", static_cast<unsigned>(score_));
    const int16_t scoreW = text.getUTF8Width(line);
    text.drawUTF8(static_cast<int16_t>((width - scoreW) / 2), static_cast<int16_t>(yOffset + 108), line);
    const char* restart = "按OK重开";
    const int16_t restartW = text.getUTF8Width(restart);
    text.drawUTF8(static_cast<int16_t>((width - restartW) / 2), static_cast<int16_t>(yOffset + 134), restart);
  }
}

void AirplaneWarGame::drawPlane(DisplayMonoTft& display, int16_t y) const {
  auto& canvas = display.canvas();
  const int16_t cy = static_cast<int16_t>(y + kPlaneH / 2);
  // 机头朝右的三角机身（战斗机）
  canvas.drawFilledTriangle(kPlaneX + 9, cy, kPlaneX, cy - 7, kPlaneX, cy + 7, ST7305_COLOR_BLACK);
}

void AirplaneWarGame::drawEnemy(DisplayMonoTft& display, int16_t x, int16_t y) const {
  auto& canvas = display.canvas();
  const int16_t cx = static_cast<int16_t>(x + kEnemySize / 2);
  const int16_t cy = static_cast<int16_t>(y + kEnemySize / 2);
  const int16_t x2 = static_cast<int16_t>(x + kEnemySize - 1);
  const int16_t y2 = static_cast<int16_t>(y + kEnemySize - 1);
  // 菱形方块
  canvas.drawFilledTriangle(cx, y, x2, cy, x, cy, ST7305_COLOR_BLACK);
  canvas.drawFilledTriangle(cx, y2, x2, cy, x, cy, ST7305_COLOR_BLACK);
}

void AirplaneWarGame::drawBullet(DisplayMonoTft& display, int16_t x, int16_t y) const {
  auto& canvas = display.canvas();
  const int16_t by = static_cast<int16_t>(y + kBulletH / 2);
  // 拖尾线
  canvas.drawLine(static_cast<uint>(x - 3), static_cast<uint>(by),
                  static_cast<uint>(x + 1), static_cast<uint>(by), ST7305_COLOR_BLACK);
  // 右向弹头三角
  canvas.drawFilledTriangle(x + 5, by, x + 1, by - 1, x + 1, by + 1, ST7305_COLOR_BLACK);
}

void AirplaneWarGame::drawStars(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  for (uint8_t i = 0; i < kStarCount; ++i) {
    const Star& s = stars_[i];
    const int16_t sx = s.x;
    const int16_t sy = static_cast<int16_t>(yOffset + s.y);
    if (s.speed >= 3U) {
      canvas.drawFilledRectangle(sx, sy, static_cast<int16_t>(sx + 1), static_cast<int16_t>(sy + 1),
                                 ST7305_COLOR_BLACK);
    } else {
      canvas.drawFilledRectangle(sx, sy, sx, sy, ST7305_COLOR_BLACK);
    }
  }
}
