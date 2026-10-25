#pragma once

#include <Arduino.h>

#include "../bsp/DisplayMonoTft.h"
#include "../bsp/MotorDriver.h"
#include "../bsp/PeripheralPower.h"
#include "../bsp/RtcDriver.h"
#include "../bsp/SdUsbSwitch.h"
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
#include "PopupView.h"
#include "Render1bpp.h"
#include "pages/TimeCalibrationPage.h"
#include "pages/DeviceSelfTestPage.h"
#include "pages/AnswersBookPage.h"
#include "pages/ComputerStatusPage.h"
#include "pages/SdUsbPage.h"
#include "pages/FocusClockPage.h"
#include "pages/GamesPage.h"
#include "pages/HomePage.h"
#include "pages/MusicPage.h"
#include "pages/ReaderPage.h"
#include "pages/RemotePage.h"
#include "pages/SettingsPage.h"
#include "pages/SystemUpdatePage.h"
#include "../services/SdCardService.h"
#include "../services/SdBitmapFont.h"
#include "../services/UiFontCache.h"
#include "../services/ComputerStatusProtocol.h"

class UiManager {
public:
  bool begin();
  void tick();

private:
  enum class UiState : uint8_t {
    Home,
    ToHomeTransition,
    ToDetailTransition,
    Popup,
    Detail
  };

  // 弹窗确认后要执行的动作。
  enum class ConfirmAction : uint8_t {
    None,
    Restart,
    FactoryReset,
    OtaApply,
    WifiStart,
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
  uint32_t nextIdleDelayMs(uint32_t nowMs) const;
  void updateCpuClock(uint32_t nowMs);
  void render(uint32_t nowMs);
  void renderPopup(uint32_t nowMs);
  void setRemotePagePortrait(bool enabled);
  void openConfirm(const char* title, const char* primaryLabel, const char* dangerLabel,
                   ConfirmAction action, uint32_t nowMs);
  void performConfirmAction(uint32_t nowMs);
  void performFactoryReset();
  void initDeviceInfoCache();
  void renderDetail(int16_t yOffset = 0);
  enum class ContentFontUse : uint8_t { None, Answers, Update, Reader, External };
  void syncContentFonts();
  void closeContentFonts();
  void syncUiFont();
  bool isComputerStatusDetail() const;
  void renderComputerStatusFrame();
  void syncHomeClockFromTimeService();
  void syncHomeSubmenu();
  void refreshHomeBattery(uint32_t nowMs);
  void refreshSdStatus();
  void enterSleep(uint32_t nowMs);
  bool isSleepAllowed() const;
  void resetButtonDebounceState();
  int16_t easeInCubic(int16_t from, int16_t to, float t) const;
  bool isPressed(uint8_t pin) const;
  int16_t easeOutCubic(int16_t from, int16_t to, float t) const;

  DisplayMonoTft display_;
  MotorDriver motorDriver_;
  PeripheralPower peripheralPower_;
  SdUsbSwitch sdUsbSwitch_;
  Render1bpp renderer_;
  HomePage homePage_;
  MusicPage musicPage_;
  ReaderPage readerPage_;
  TimeCalibrationPage timeCalibrationPage_;
  DeviceSelfTestPage deviceSelfTestPage_;
  AnswersBookPage answersBookPage_;
  ComputerStatusPage computerStatusPage_;
  SdUsbPage sdUsbPage_;
  FocusClockPage focusClockPage_;
  GamesPage gamesPage_;
  RemotePage remotePage_;
  SettingsPage settingsPage_;
  SystemUpdatePage systemUpdatePage_;
  BluetoothService bluetoothService_;
  RemoteService remoteService_;
  OtaService otaService_;
  PowerDiagnosticService powerDiagnosticService_;
  TimeService timeService_;
  WifiProvisionService wifiProvisionService_;
  SdCardService sdCardService_;
  SdBitmapFont sdBitmapFont_;
  SdBitmapFont sdTitleFont_;
  UiFontCache uiFont_;
  bool uiFontAttempted_ = false;
  ContentFontUse contentFontUse_ = ContentFontUse::None;
  MusicService musicService_;
  IicScanService iicScanService_;
  RtcTestService rtcTestService_;
  ImuTestService imuTestService_;
  ReaderService readerService_;
  ComputerStatusProtocol computerStatusProtocol_;
  RtcDriver rtcDriver_;
  UiState state_ = UiState::Home;

  uint32_t transitionStartMs_ = 0;
  uint8_t sectionFocusIndex_ = 0;
  PopupView::ConfirmState confirmState_;
  ConfirmAction confirmAction_ = ConfirmAction::None;
  const char* confirmTitle_ = "";
  const char* confirmPrimaryLabel_ = "";
  const char* confirmDangerLabel_ = "";
  uint8_t detailPageIndex_ = 0;
  char deviceIdText_[40] = {0};
  char flashTotalText_[40] = {0};
  char sdStatusText_[48] = {0};
  uint32_t lastRenderMs_ = 0;
  uint32_t cpuBoostStartMs_ = 0;
  bool needsRedraw_ = true;
  bool leftLongReported_ = false;
  uint32_t lastActivityMs_ = 0;
  uint32_t lastBatteryRefreshMs_ = 0;
  bool batteryRefreshStarted_ = false;
  bool computerStatusFrameVisible_ = false;

  ButtonEdge buttons_[5] = {
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0},
      {0, false, false, 0}};

};
