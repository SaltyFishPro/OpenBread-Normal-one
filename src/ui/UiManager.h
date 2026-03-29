#pragma once

#include <Arduino.h>

#include "../bsp/DisplayMonoTft.h"
#include "Render1bpp.h"
#include "pages/HomePage.h"

class UiManager {
public:
  bool begin();
  void tick();

private:
  enum class UiState : uint8_t {
    Home,
    ToSectionTransition,
    ToHomeTransition,
    ToDetailTransition,
    ToSectionFromDetailTransition,
    Section,
    Popup,
    Detail
  };

  enum class PopupKind : uint8_t {
    RestartConfirm,
    LanguageSelect
  };

  struct ButtonEdge {
    uint8_t pin;
    bool stablePressed;
    bool lastRawPressed;
    uint32_t lastRawChangeMs;
  };

  struct InputEdges {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool ok = false;
  };

  InputEdges pollInputEdges();
  void updateState(const InputEdges& edges, uint32_t nowMs);
  bool shouldRedraw(uint32_t nowMs) const;
  void render(uint32_t nowMs);
  void renderSection(int16_t xOffset, int16_t yOffset, uint32_t nowMs,
                     int16_t iconExtraOffsetX = 0,
                     const int16_t* rowExtraOffsets = nullptr,
                     uint8_t rowExtraCount = 0,
                     int16_t focusBoxExtraOffsetX = 0);
  void renderPopup(uint32_t nowMs);
  void renderTwoOptionPopup(const char* title, const char* primaryLabel,
                            const char* secondaryLabel, uint32_t nowMs);
  void initDeviceInfoCache();
  bool isAboutDeviceDetail() const;
  void renderDetailHeader(const char* title, int16_t yOffset);
  void renderAboutDeviceDetail(int16_t yOffset);
  void renderDetail(int16_t yOffset = 0);
  void startSectionFocusAnimation(uint8_t toIndex, uint32_t nowMs);
  int16_t easeInCubic(int16_t from, int16_t to, float t) const;
  bool isPressed(uint8_t pin) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;

  DisplayMonoTft display_;
  Render1bpp renderer_;
  HomePage homePage_;
  UiState state_ = UiState::Home;

  uint32_t transitionStartMs_ = 0;
  uint8_t sectionFocusIndex_ = 0;
  uint8_t sectionAnimFromIndex_ = 0;
  uint8_t sectionAnimToIndex_ = 0;
  uint32_t sectionAnimStartMs_ = 0;
  uint16_t lastSectionIconFrame_ = 0;
  HomePage::Language language_ = HomePage::Language::Zh;
  PopupKind popupKind_ = PopupKind::RestartConfirm;
  bool popupSelectPrimary_ = false;
  uint16_t lastPopupFrame_ = 0;
  uint8_t detailPageIndex_ = 0;
  char deviceIdText_[40] = {0};
  char flashTotalText_[40] = {0};
  bool sectionAnimActive_ = false;
  bool needsRedraw_ = true;

  ButtonEdge buttons_[5] = {
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0}};
};
