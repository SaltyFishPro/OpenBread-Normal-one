#include "Game2048.h"

#include <cstdio>
#include <Preferences.h>

#include "../../bsp/DisplayMonoTft.h"
#include "../Segment7Font.h"
#include "../../log/Log.h"

namespace {
constexpr int16_t kGridX = 30;
constexpr int16_t kGridY = 7;
constexpr int16_t kCell = 36;
constexpr int16_t kGap = 3;
constexpr int16_t kInfoX = 216;
constexpr uint32_t kSlideMs = 150;
constexpr uint32_t kFeedbackMs = 90;
constexpr Segment7Font::Style kTitleStyle = {15, 24, 3, 3, 5, ST7305_COLOR_BLACK};
constexpr Segment7Font::Style kScoreStyle = {12, 20, 2, 2, 5, ST7305_COLOR_BLACK};
constexpr Segment7Font::Style kFiveDigitStyle = {6, 14, 1, 1, 0, ST7305_COLOR_BLACK};

void cellAt(uint8_t dir, uint8_t line, uint8_t slot, uint8_t& row, uint8_t& col) {
  const uint8_t index = (dir == 0U || dir == 2U) ? slot : static_cast<uint8_t>(3U - slot);
  row = dir < 2U ? line : index;
  col = dir < 2U ? index : line;
}
}  // namespace

void Game2048::enter() {
  loadBest();
  resetRun();
  state_ = State::Running;
  changed_ = true;
  LogI("GAME/2048", "start best=%lu", static_cast<unsigned long>(best_));
}

void Game2048::exit() {
  if (state_ == State::Stopped) return;
  saveBest();
  animationActive_ = false;
  state_ = State::Stopped;
  changed_ = true;
}

uint32_t Game2048::nextRand() {
  seed_ = seed_ * 1664525UL + 1013904223UL;
  return seed_;
}

void Game2048::resetRun() {
  seed_ = millis() | 1UL;
  for (uint8_t r = 0; r < kSize; ++r) {
    for (uint8_t c = 0; c < kSize; ++c) grid_[r][c] = 0;
  }
  score_ = 0;
  reached2048_ = false;
  animationActive_ = false;
  movingCount_ = 0;
  mergedMask_ = 0;
  prevRight_ = prevUp_ = prevDown_ = false;
  edgesReady_ = false;
  spawnTile();
  spawnTile();
}

void Game2048::loadBest() {
  Preferences prefs;
  if (!prefs.begin("g2048", true)) return;
  best_ = prefs.getUInt("best", 0U);
  prefs.end();
}

void Game2048::saveBest() {
  Preferences prefs;
  if (!prefs.begin("g2048", false)) return;
  prefs.putUInt("best", best_);
  prefs.end();
}

void Game2048::tick(uint32_t nowMs, bool leftPressed, bool rightPressed, bool upPressed,
                    bool downPressed) {
  (void)leftPressed;  // UiManager routes the short Left release separately.
  if (animationActive_ && nowMs - animationStartMs_ >= kSlideMs + kFeedbackMs) {
    animationActive_ = false;
    changed_ = true;
  }
  if (state_ != State::Running) return;
  if (!edgesReady_) {
    // 进入时同步一次按键状态，避免带着按键进入触发一次滑动
    prevRight_ = rightPressed;
    prevUp_ = upPressed;
    prevDown_ = downPressed;
    edgesReady_ = true;
    return;
  }
  const bool rightEdge = rightPressed && !prevRight_;
  const bool upEdge = upPressed && !prevUp_;
  const bool downEdge = downPressed && !prevDown_;
  prevRight_ = rightPressed;
  prevUp_ = upPressed;
  prevDown_ = downPressed;

  uint8_t dir = 0xFFU;
  if (animationActive_) return;
  if (rightEdge) dir = 1;
  else if (upEdge) dir = 2;
  else if (downEdge) dir = 3;
  if (dir == 0xFFU) return;

  applyMove(dir, nowMs);
}

void Game2048::handleLeft(uint32_t nowMs) {
  applyMove(0, nowMs);
}

void Game2048::applyMove(uint8_t dir, uint32_t nowMs) {
  if (state_ != State::Running || animationActive_) return;
  if (!move(dir)) return;  // 无效滑动不生成新块

  animationStartMs_ = nowMs;
  animationActive_ = true;

  if (!reached2048_) {
    for (uint8_t r = 0; r < kSize && !reached2048_; ++r) {
      for (uint8_t c = 0; c < kSize; ++c) {
        if (grid_[r][c] >= 2048U) {
          reached2048_ = true;
          LogI("GAME/2048", "reach 2048 score=%lu", static_cast<unsigned long>(score_));
          break;
        }
      }
    }
  }
  spawnTile();
  if (score_ > best_) best_ = score_;
  changed_ = true;
  if (!hasMoves()) {
    state_ = State::GameOver;
    saveBest();
    LogI("GAME/2048", "game over score=%lu best=%lu", static_cast<unsigned long>(score_),
         static_cast<unsigned long>(best_));
  }
}

void Game2048::handleOk() {
  if (state_ == State::GameOver) {
    resetRun();
    state_ = State::Running;
    changed_ = true;
    LogI("GAME/2048", "restart");
  }
}

bool Game2048::move(uint8_t dir) {
  bool changed = false;
  movingCount_ = 0;
  mergedMask_ = 0;
  for (uint8_t line = 0; line < kSize; ++line) {
    uint16_t values[kSize] = {};
    uint8_t sources[kSize] = {};
    uint16_t result[kSize] = {};
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < kSize; ++slot) {
      uint8_t row, col;
      cellAt(dir, line, slot, row, col);
      if (grid_[row][col] == 0U) continue;
      values[count] = grid_[row][col];
      sources[count++] = slot;
    }
    uint8_t destination = 0;
    for (uint8_t source = 0; source < count; ++source) {
      const bool merge = source + 1U < count && values[source] < 32768U &&
                         values[source] == values[source + 1U];
      uint8_t toRow, toCol, fromRow, fromCol;
      cellAt(dir, line, destination, toRow, toCol);
      cellAt(dir, line, sources[source], fromRow, fromCol);
      moving_[movingCount_++] = {values[source], fromRow, fromCol, toRow, toCol};
      if (merge) {
        uint8_t secondRow, secondCol;
        cellAt(dir, line, sources[++source], secondRow, secondCol);
        moving_[movingCount_++] = {values[source], secondRow, secondCol, toRow, toCol};
        result[destination] = static_cast<uint16_t>(values[source] * 2U);
        mergedMask_ |= static_cast<uint16_t>(1U << (toRow * kSize + toCol));
        score_ += result[destination];
        changed = true;
      } else {
        result[destination] = values[source];
      }
      if (fromRow != toRow || fromCol != toCol) changed = true;
      ++destination;
    }
    for (uint8_t slot = 0; slot < kSize; ++slot) {
      uint8_t row, col;
      cellAt(dir, line, slot, row, col);
      grid_[row][col] = result[slot];
    }
  }
  return changed;
}

void Game2048::spawnTile() {
  uint8_t empty = 0;
  for (uint8_t r = 0; r < kSize; ++r) {
    for (uint8_t c = 0; c < kSize; ++c) {
      if (grid_[r][c] == 0U) ++empty;
    }
  }
  if (empty == 0U) return;
  uint32_t pick = nextRand() % empty;
  for (uint8_t r = 0; r < kSize; ++r) {
    for (uint8_t c = 0; c < kSize; ++c) {
      if (grid_[r][c] != 0U) continue;
      if (pick == 0U) {
        grid_[r][c] = (nextRand() % 10U == 0U) ? 4U : 2U;
        return;
      }
      --pick;
    }
  }
}

bool Game2048::hasMoves() const {
  for (uint8_t r = 0; r < kSize; ++r) {
    for (uint8_t c = 0; c < kSize; ++c) {
      if (grid_[r][c] == 0U) return true;
      if (c + 1U < kSize && grid_[r][c] == grid_[r][c + 1U]) return true;
      if (r + 1U < kSize && grid_[r][c] == grid_[r + 1U][c]) return true;
    }
  }
  return false;
}

bool Game2048::consumeChanged() {
  const bool c = changed_;
  changed_ = false;
  return c;
}

void Game2048::render(DisplayMonoTft& display, int16_t yOffset) const {
  auto& c = display.canvas();
  auto& t = display.text();
  t.setFont(u8g2_font_7x14_tf);
  c.drawRectangle(kGridX - 3, yOffset + kGridY - 3, kGridX + 157,
                  yOffset + kGridY + 157, ST7305_COLOR_BLACK);
  for (uint8_t r = 0; r < kSize; ++r) {
    for (uint8_t col = 0; col < kSize; ++col) {
      const int16_t x = static_cast<int16_t>(kGridX + col * (kCell + kGap));
      const int16_t y = static_cast<int16_t>(kGridY + r * (kCell + kGap));
      renderCell(display, x, static_cast<int16_t>(yOffset + y), 0);
    }
  }
  const uint32_t elapsed = animationActive_ ? millis() - animationStartMs_ : 0;
  if (animationActive_ && elapsed < kSlideMs) {
    const int32_t progress = static_cast<int32_t>(elapsed * 256U / kSlideMs);
    for (uint8_t i = 0; i < movingCount_; ++i) {
      const MovingTile& tile = moving_[i];
      const int16_t fromX = kGridX + tile.fromCol * (kCell + kGap);
      const int16_t fromY = kGridY + tile.fromRow * (kCell + kGap);
      const int16_t toX = kGridX + tile.toCol * (kCell + kGap);
      const int16_t toY = kGridY + tile.toRow * (kCell + kGap);
      const int16_t x = static_cast<int16_t>(fromX + (toX - fromX) * progress / 256);
      const int16_t y = static_cast<int16_t>(fromY + (toY - fromY) * progress / 256);
      renderCell(display, x, static_cast<int16_t>(yOffset + y), tile.value);
    }
  } else {
    for (uint8_t r = 0; r < kSize; ++r) {
      for (uint8_t col = 0; col < kSize; ++col) {
        if (grid_[r][col] == 0U) continue;
        const int16_t x = static_cast<int16_t>(kGridX + col * (kCell + kGap));
        const int16_t y = static_cast<int16_t>(yOffset + kGridY + r * (kCell + kGap));
        const bool flash = animationActive_ &&
            (mergedMask_ & (1U << (r * kSize + col))) != 0U;
        renderCell(display, x, y, grid_[r][col], flash);
      }
    }
  }

  c.drawFastVLine(kInfoX - 17, yOffset + 12, 144, ST7305_COLOR_BLACK);
  Segment7Font::drawText(c, kInfoX, yOffset + 10, "2048", kTitleStyle);
  c.drawFastHLine(kInfoX, yOffset + 39, 150, ST7305_COLOR_BLACK);
  t.setForegroundColor(ST7305_COLOR_BLACK);
  t.setBackgroundColor(ST7305_COLOR_WHITE);
  t.setFontMode(1);
  t.drawUTF8(kInfoX, yOffset + 57, "得分");
  char buf[32];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(score_));
  Segment7Font::drawText(c, kInfoX, yOffset + 62, buf, kScoreStyle);
  c.drawFastHLine(kInfoX, yOffset + 89, 150, ST7305_COLOR_BLACK);
  t.drawUTF8(kInfoX, yOffset + 107, "最高");
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(best_));
  Segment7Font::drawText(c, kInfoX, yOffset + 112, buf, kScoreStyle);
  c.drawFastHLine(kInfoX, yOffset + 138, 150, ST7305_COLOR_BLACK);
  uint16_t largest = 2;
  for (const auto& row : grid_) {
    for (const uint16_t value : row) {
      if (value > largest) largest = value;
    }
  }
  snprintf(buf, sizeof(buf), "最大数 %u", static_cast<unsigned>(largest));
  t.drawUTF8(kInfoX, yOffset + 156, buf);

  if (state_ == State::GameOver && !animationActive_) {
    c.drawFilledRectangle(kGridX - 3, yOffset + kGridY - 3,
                          kGridX + 157, yOffset + kGridY + 157, ST7305_COLOR_WHITE);
    c.drawRectangle(kGridX - 3, yOffset + kGridY - 3,
                    kGridX + 157, yOffset + kGridY + 157, ST7305_COLOR_BLACK);
    t.setForegroundColor(ST7305_COLOR_BLACK);
    const int16_t centerX = kGridX + 76;
    const char* title = "游戏结束";
    t.drawUTF8(centerX - t.getUTF8Width(title) / 2, yOffset + 64, title);
    snprintf(buf, sizeof(buf), "本局 %lu", static_cast<unsigned long>(score_));
    t.drawUTF8(centerX - t.getUTF8Width(buf) / 2, yOffset + 94, buf);
    const char* restart = "OK 重新开始";
    t.drawUTF8(centerX - t.getUTF8Width(restart) / 2, yOffset + 125, restart);
  }
}

void Game2048::renderCell(DisplayMonoTft& display, int16_t x, int16_t y, uint16_t v,
                          bool flash) const {
  auto& c = display.canvas();
  auto& t = display.text();
  const int16_t right = static_cast<int16_t>(x + kCell - 1);
  const int16_t bottom = static_cast<int16_t>(y + kCell - 1);
  if (v == 0U) {
    c.drawRectangle(x, y, right, bottom, ST7305_COLOR_BLACK);
    return;
  }
  const bool big = (v >= 128U) != flash;
  if (big) {
    c.drawFilledRectangle(x, y, right, bottom, ST7305_COLOR_BLACK);
    if (v >= 1024U && !flash) {
      c.drawRectangle(static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2),
                      static_cast<int16_t>(right - 2), static_cast<int16_t>(bottom - 2),
                      ST7305_COLOR_WHITE);
    }
  } else {
    c.drawRectangle(x, y, right, bottom, ST7305_COLOR_BLACK);
    if (v >= 16U && !flash) {
      c.drawRectangle(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                      static_cast<int16_t>(right - 1), static_cast<int16_t>(bottom - 1),
                      ST7305_COLOR_BLACK);
    }
  }
  if (v >= 64U && v < 128U && !flash) {
    c.drawFastHLine(x + 5, y + 4, kCell - 10, ST7305_COLOR_BLACK);
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(v));
  if (v >= 10000U) {
    Segment7Font::Style style = kFiveDigitStyle;
    style.onColor = big ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
    const int16_t width = Segment7Font::measureText(buf, style);
    Segment7Font::drawText(c, x + (kCell - width) / 2, y + 11, buf, style);
    return;
  }
  t.setForegroundColor(big ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  t.setBackgroundColor(big ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  t.setFontMode(1);
  const int16_t w = t.getUTF8Width(buf);
  t.drawUTF8(static_cast<int16_t>(x + (kCell - w) / 2), static_cast<int16_t>(y + 22), buf);
}
