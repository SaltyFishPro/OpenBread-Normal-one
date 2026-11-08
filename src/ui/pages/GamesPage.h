#pragma once

#include <stdint.h>

#include "AirplaneWarGame.h"
#include "RacingGame.h"
#include "Game2048.h"
#include "HypnosisApp.h"
#include "../../services/RacingSteeringService.h"

class DisplayMonoTft;

class GamesPage {
public:
  static constexpr uint8_t kHomeIndex = 5;
  static constexpr uint8_t kWoodenFishItemIndex = 0;
  static constexpr uint8_t kAirplaneWarItemIndex = 1;
  static constexpr uint8_t kRacingItemIndex = 2;
  static constexpr uint8_t k2048ItemIndex = 3;
  static constexpr uint8_t kHypnosisItemIndex = 4;

  bool isWoodenFishSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isAirplaneWarSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isRacingSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool is2048Selection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isHypnosisSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool racingPaused() const { return racing_.paused(); }
  void handle2048Left(uint32_t nowMs);
  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge, bool okPressed,
                         bool okChanged, uint32_t nowMs);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus);
  void tick(uint32_t nowMs, bool leftPressed, bool rightPressed, bool upPressed, bool downPressed);
  bool consumeChanged();
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display) const;

private:
  void registerKnock(uint32_t nowMs);

  uint32_t knockCount_ = 0;
  uint32_t messageSeed_ = 0;
  uint8_t lastMessageIndex_ = 0;
  bool hasMessage_ = false;
  bool iconPressed_ = false;
  AirplaneWarGame airplaneWar_;
  RacingGame racing_;
  Game2048 game2048_;
  HypnosisApp hypnosis_;
  RacingSteeringService racingSteering_;
};
