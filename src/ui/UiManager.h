#pragma once

#include <Arduino.h>

#include "../bsp/DisplayMonoTft.h"
#include "../bsp/MotorDriver.h"
#include "../bsp/PeripheralPower.h"
#include "../bsp/RtcDriver.h"
#include "../services/BluetoothService.h"
#include "../services/IicScanService.h"
#include "../services/ImuTestService.h"
#include "../services/MusicService.h"
#include "../services/OtaService.h"
#include "../services/PowerDiagnosticService.h"
#include "../services/ReaderService.h"
#include "../services/RemoteService.h"
#include "../services/RtcTestService.h"
#include "../services/TimeService.h"
#include "../services/WifiProvisionService.h"
#include "Render1bpp.h"
#include "pages/TimeCalibrationPage.h"
#include "pages/DeviceSelfTestPage.h"
#include "pages/GamesPage.h"
#include "pages/HomePage.h"
#include "pages/MusicPage.h"
#include "pages/ReaderPage.h"
#include "pages/RemotePage.h"
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
    bool leftLong = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool ok = false;
    bool okPressed = false;
    bool okChanged = false;
  };

  InputEdges pollInputEdges();
  void updateState(const InputEdges& edges, uint32_t nowMs);
  bool shouldRedraw(uint32_t nowMs) const;
  uint32_t targetFrameIntervalMs(uint32_t nowMs) const;
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
  void syncHomeClockFromTimeService();
  void refreshSdStatus();
  bool isSectionAnimationActive(uint32_t nowMs) const;
  uint32_t sectionAnimationRenderTime(uint32_t nowMs) const;
  void enterSleep(uint32_t nowMs);
  bool isSleepAllowed() const;
  void resetButtonDebounceState();
  int16_t easeInCubic(int16_t from, int16_t to, float t) const;
  bool isPressed(uint8_t pin) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;

  DisplayMonoTft display_;
  MotorDriver motorDriver_;
  PeripheralPower peripheralPower_;
  Render1bpp renderer_;
  HomePage homePage_;
  MusicPage musicPage_;
  ReaderPage readerPage_;
  TimeCalibrationPage timeCalibrationPage_;
  DeviceSelfTestPage deviceSelfTestPage_;
  GamesPage gamesPage_;
  RemotePage remotePage_;
  SettingsPage settingsPage_;
  BluetoothService bluetoothService_;
  RemoteService remoteService_;
  OtaService otaService_;
  PowerDiagnosticService powerDiagnosticService_;
  TimeService timeService_;
  WifiProvisionService wifiProvisionService_;
  SdCardService sdCardService_;
  MusicService musicService_;
  IicScanService iicScanService_;
  RtcTestService rtcTestService_;
  ImuTestService imuTestService_;
  ReaderService readerService_;
  RtcDriver rtcDriver_;
  UiState state_ = UiState::Home;

  uint32_t transitionStartMs_ = 0;
  uint8_t sectionFocusIndex_ = 0;
  uint8_t sectionAnimFromIndex_ = 0;
  uint8_t sectionAnimToIndex_ = 0;
  uint32_t sectionAnimStartMs_ = 0;
  uint16_t lastSectionIconFrame_ = 0;
  uint32_t lastSectionInteractionMs_ = 0;
  uint32_t sectionAnimationTimeMs_ = 0;
  SettingsPage::PopupKind popupKind_ = SettingsPage::PopupKind::RestartConfirm;
  bool popupSelectPrimary_ = false;
  uint16_t lastPopupFrame_ = 0;
  uint8_t detailPageIndex_ = 0;
  char deviceIdText_[40] = {0};
  char flashTotalText_[40] = {0};
  char sdStatusText_[48] = {0};
  uint32_t lastRenderMs_ = 0;
  bool sectionAnimActive_ = false;
  bool needsRedraw_ = true;
  bool leftLongReported_ = false;
  uint32_t lastActivityMs_ = 0;

  ButtonEdge buttons_[5] = {
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0}};

  static constexpr uint32_t kSectionIdleAnimationTimeoutMs = 4000;
};
