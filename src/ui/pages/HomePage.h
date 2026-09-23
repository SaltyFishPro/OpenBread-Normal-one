#pragma once

#include <Arduino.h>

#include "../../bsp/DisplayMonoTft.h"

class HomePage {
public:
  // 主菜单里直接进入详情页、跳过单条目二级目录的入口。
  static constexpr uint8_t kAlarmMenuIndex = 6;
  static constexpr uint8_t kCalendarMenuIndex = 7;
  static constexpr uint8_t kApiStatsMenuIndex = 8;

  struct ClockData {
    uint8_t second = 0;
    uint8_t minute = 0;
    uint8_t hour = 0;
    uint8_t day = 1;
    uint8_t weekday = 0;
    uint8_t month = 1;
    uint16_t year = 2026;
    bool valid = false;
  };

  struct Rect {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
  };

  bool begin();
  bool handleInput(bool upEdge, bool downEdge, bool okEdge, uint32_t nowMs);
  void update(uint32_t nowMs);
  void render(DisplayMonoTft& display, int16_t pageOffsetX, uint32_t nowMs);
  void renderTransition(DisplayMonoTft& display, int16_t backgroundOffsetX,
                        int16_t menuBaseOffsetX, int16_t menuUpExtraOffsetX,
                        int16_t menuFocusExtraOffsetX, int16_t menuDownExtraOffsetX,
                        uint32_t nowMs);

  bool isSliding() const;
  bool hasAnimationTick(uint32_t nowMs) const;
  // 仅未校准小面包动画需要重绘时为真，用于只刷新时间卡片区域。
  bool isBreadOnlyAnimationTick(uint32_t nowMs) const;
  // 仅菜单图标动画需要重绘时为真，用于只刷新菜单图标区域。
  bool isMenuIconsOnlyAnimationTick(uint32_t nowMs) const;
  Rect timeCardBounds() const;
  Rect menuIconBounds(const DisplayMonoTft& display) const;
  Rect menuColumnBounds(const DisplayMonoTft& display) const;
  void renderTimeCardOnly(DisplayMonoTft& display, uint32_t nowMs);
  void renderMenuIconsOnly(DisplayMonoTft& display, uint32_t nowMs);
  // 本帧是否直接回填了静态图层（未重建），用于判断能否只推送菜单列。
  bool frameUsedStaticLayer() const;
  bool isAnimationActive(uint32_t nowMs) const;
  uint8_t focusIndex() const;
  const char* focusName() const;
  void setClockData(const ClockData& data);

private:
  enum class SlideState : uint8_t { Idle, Sliding };

  const char* menuLabel(uint8_t idx) const;
  int16_t currentMenuOffset(uint32_t nowMs) const;
  void beginSlide(int8_t direction, uint32_t nowMs);
  bool isInteractiveAnimationWindow(uint32_t nowMs) const;
  uint32_t animationRenderTime(uint32_t nowMs) const;
  bool hasUncalibratedBreadTick(uint32_t nowMs) const;
  bool hasMenuAnimationTick(uint32_t nowMs) const;
  // 轮盘布局（焦点索引 + 纵向偏移）是否与上一次整屏渲染一致。
  // 局部刷新只重画图标/时间卡片，布局一旦变化必须由整屏重绘更新焦点框与标签。
  bool menuLayoutMatchesLastFullRender(uint32_t nowMs) const;
  void drawMenuWheel(DisplayMonoTft& display, int16_t frameBaseX, int16_t centerY,
                     int16_t menuOffsetY, int16_t menuUpExtraOffsetX,
                     int16_t menuFocusExtraOffsetX, int16_t menuDownExtraOffsetX,
                     uint32_t animNowMs, bool iconsOnly);

  SlideState slideState_ = SlideState::Idle;
  uint8_t focusIndex_ = 0;
  uint8_t targetIndex_ = 0;

  int16_t animFromOffsetY_ = 0;
  int16_t animToOffsetY_ = 0;
  uint32_t animStartMs_ = 0;

  uint16_t lastFocusFrame_ = 0;
  uint16_t lastBackgroundFrame_ = 0;
  uint16_t lastUncalibratedFrame_ = 0;
  // 静态图层：背景 + 时间卡片 + 日期卡片。整帧快照，静止时直接回填。
  static constexpr size_t kStaticLayerBytes = 384U * 168U / 8U;
  uint8_t staticLayer_[kStaticLayerBytes] = {0};
  bool staticLayerValid_ = false;
  bool frameUsedStaticLayer_ = false;
  uint8_t lastRenderedFocusIndex_ = 0xFF;
  int16_t lastRenderedMenuOffsetY_ = 0;
  uint32_t lastInteractionMs_ = 0;
  uint32_t animationTimeMs_ = 0;
  ClockData clockData_;

  static constexpr uint8_t kMenuCount = 9;
  static constexpr uint32_t kIdleAnimationTimeoutMs = 4000;
};
