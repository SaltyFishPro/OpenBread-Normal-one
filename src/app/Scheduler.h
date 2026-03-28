#pragma once

#include <Arduino.h>

struct TaskTick {
  uint32_t intervalMs = 0;
  uint32_t lastRunMs = 0;

  bool due(uint32_t nowMs) {
    if (nowMs - lastRunMs >= intervalMs) {
      lastRunMs = nowMs;
      return true;
    }
    return false;
  }
};