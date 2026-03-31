#pragma once

#include <Arduino.h>

#include "../bsp/DisplayMonoTft.h"
#include "../bsp/RtcDriver.h"
#include "../services/WifiProvisionService.h"
#include "Render1bpp.h"
#include "pages/HomePage.h"
#include "pages/SettingsPage.h"
#include "../services/SdCardService.h"

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
  void renderDetail(int16_t yOffset = 0);
  void startSectionFocusAnimation(uint8_t toIndex, uint32_t nowMs);
  void syncHomeClockFromRtc(uint32_t nowMs);
  bool startNtpSync(uint32_t nowMs);
  void processNtpSync(uint32_t nowMs);
  void refreshSdStatus();
  int16_t easeInCubic(int16_t from, int16_t to, float t) const;
  bool isPressed(uint8_t pin) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;

  DisplayMonoTft display_;
  Render1bpp renderer_;
  HomePage homePage_;
  SettingsPage settingsPage_;
  WifiProvisionService wifiProvisionService_;
  SdCardService sdCardService_;
  RtcDriver rtcDriver_;
  UiState state_ = UiState::Home;

  uint32_t transitionStartMs_ = 0;
  uint8_t sectionFocusIndex_ = 0;
  uint8_t sectionAnimFromIndex_ = 0;
  uint8_t sectionAnimToIndex_ = 0;
  uint32_t sectionAnimStartMs_ = 0;
  uint16_t lastSectionIconFrame_ = 0;
  HomePage::Language language_ = HomePage::Language::Zh;
  SettingsPage::PopupKind popupKind_ = SettingsPage::PopupKind::RestartConfirm;
  bool popupSelectPrimary_ = false;
  uint16_t lastPopupFrame_ = 0;
  uint8_t detailPageIndex_ = 0;
  char deviceIdText_[40] = {0};
  char flashTotalText_[40] = {0};
  char sdStatusText_[48] = {0};
  bool rtcReady_ = false;
  bool ntpSyncActive_ = false;
  uint32_t ntpSyncStartMs_ = 0;
  uint32_t lastRtcReadMs_ = 0;
  bool sectionAnimActive_ = false;
  bool needsRedraw_ = true;

  ButtonEdge buttons_[5] = {
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0}};
};
