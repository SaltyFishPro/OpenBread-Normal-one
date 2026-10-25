#include "UiManager.h"

#include <cstdio>

#include "../bsp/BoardConfig.h"
#include "../bsp/CpuClock.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <nvs_flash.h>
#include "../log/Log.h"
#include "AnimMath.h"
#include "ThemeMono.h"
#include "SdUiText.h"
#include "../services/SdCardService.h"

namespace {
// 局部刷新：音乐导航栏/列表动画只把脏区域推送到屏幕，降低 SPI 传输量。
// 置 0 可退回整屏刷新以便对比排查。
#ifndef OB_PARTIAL_REFRESH_ENABLED
#define OB_PARTIAL_REFRESH_ENABLED 1
#endif

constexpr uint32_t kAutoSleepTimeoutMs = 20000;
constexpr uint32_t kSleepTransitionMs = 720;
constexpr uint32_t kSleepTransitionFrameMs = 16;
constexpr uint32_t kBatteryRefreshIntervalMs = 10000;

constexpr uint32_t kDetailTransitionMs = 420;
constexpr uint32_t kButtonDebounceMs = 25;
constexpr uint32_t kButtonLongPressMs = 650;
constexpr uint32_t kHighFrameIntervalMs = 16;
constexpr uint32_t kTransitionFrameIntervalMs = 32;
// 空闲时仍按输入采样周期轮询；射频/自检等需要协作式高频服务的子系统用 1ms。
constexpr uint32_t kActivePollMs = 1;
// 满屏渲染后保持满速的时长，避免降频状态下执行整帧绘制。
constexpr uint32_t kCpuBoostHoldMs = 500;

struct SubmenuContent {
  const char* const* labels;
  uint8_t count;
};

const char* const kMusicItems[] = {"音乐列表", "收藏歌曲"};
const char* const kReaderItems[] = {"单词阅读", "提词器"};
const char* const kWirelessItems[] = {"蓝牙连接", "蓝牙拍照", "翻页器"};
const char* const kGamesItems[] = {"敲木鱼", "飞机世界大战"};

const SubmenuContent kSubmenus[] = {
    {kMusicItems, static_cast<uint8_t>(sizeof(kMusicItems) / sizeof(kMusicItems[0]))},
    {kReaderItems, static_cast<uint8_t>(sizeof(kReaderItems) / sizeof(kReaderItems[0]))},
    {nullptr, 0},
    {kWirelessItems, static_cast<uint8_t>(sizeof(kWirelessItems) / sizeof(kWirelessItems[0]))},
    {kGamesItems, static_cast<uint8_t>(sizeof(kGamesItems) / sizeof(kGamesItems[0]))},
};

SubmenuContent submenuContentFor(uint8_t homeFocus) {
  if (homeFocus == 0U || homeFocus > sizeof(kSubmenus) / sizeof(kSubmenus[0]))
    return {nullptr, 0};
  return kSubmenus[homeFocus - 1U];
}
}  // namespace

bool UiManager::begin() {
  CpuClock::begin();
  sdUsbSwitch_.begin();  // Default LOW before any SD initialization, including boot failures.

  buttons_[0].pin = static_cast<uint8_t>(BoardConfig::kPinBtnLeft);
  buttons_[1].pin = static_cast<uint8_t>(BoardConfig::kPinBtnRight);
  buttons_[2].pin = static_cast<uint8_t>(BoardConfig::kPinBtnUp);
  buttons_[3].pin = static_cast<uint8_t>(BoardConfig::kPinBtnDown);
  buttons_[4].pin = static_cast<uint8_t>(BoardConfig::kPinBtnOk);

  const uint32_t nowMs = millis();
  for (auto& button : buttons_) {
    pinMode(button.pin, INPUT_PULLUP);
    const bool pressed = isPressed(button.pin);
    button.stablePressed = pressed;
    button.lastRawPressed = pressed;
    button.lastRawChangeMs = nowMs;
  }

  if (!display_.begin()) {
    return false;
  }
  if (!peripheralPower_.begin()) {
    return false;
  }
  if (!motorDriver_.begin()) {
    return false;
  }
  if (!renderer_.begin()) {
    return false;
  }
  if (!homePage_.begin()) {
    return false;
  }
  homePage_.setFontStatus(HomePage::FontStatus::Loading);
  if (!bluetoothService_.begin()) {
    return false;
  }
  if (!remoteService_.begin()) {
    return false;
  }
  if (!wifiProvisionService_.begin()) {
    return false;
  }
  if (!otaService_.begin()) {
    return false;
  }
  if (!musicService_.begin()) {
    return false;
  }
  if (!readerService_.begin()) {
    return false;
  }
  if (!iicScanService_.begin()) {
    return false;
  }
  sdCardService_.begin();
  (void)rtcDriver_.begin();
  if (!rtcTestService_.begin(rtcDriver_)) {
    return false;
  }
  if (!timeService_.begin(rtcDriver_)) {
    // Keep booting even when RTC is unavailable; UI will show time as invalid.
  }
  (void)imuTestService_.begin();

  initDeviceInfoCache();
  syncHomeClockFromTimeService();
  refreshHomeBattery(nowMs);
  state_ = UiState::Home;
  lastActivityMs_ = millis();
  leftLongReported_ = false;
  transitionStartMs_ = millis();
  sectionFocusIndex_ = 0;
  confirmState_ = PopupView::ConfirmState{};
  confirmAction_ = ConfirmAction::None;
  lastRenderMs_ = 0;
  needsRedraw_ = true;
  return true;
}

void UiManager::initDeviceInfoCache() {
  const uint64_t efuseMac = ESP.getEfuseMac();
  snprintf(deviceIdText_, sizeof(deviceIdText_), "设备ID：%012llX",
           static_cast<unsigned long long>(efuseMac & 0x0000FFFFFFFFFFFFULL));

  const uint32_t flashBytes = ESP.getFlashChipSize();
  const uint32_t flashMb = flashBytes / (1024U * 1024U);
  if (flashMb > 0) {
    snprintf(flashTotalText_, sizeof(flashTotalText_), "Flash空间：%u MB",
             static_cast<unsigned>(flashMb));
  } else {
    snprintf(flashTotalText_, sizeof(flashTotalText_), "Flash空间：%u B",
             static_cast<unsigned>(flashBytes));
  }

  refreshSdStatus();
}

void UiManager::tick() {
  const uint32_t nowMs = millis();
  const bool checkingBodyFont = sdBitmapFont_.state() == SdBitmapFont::State::Checking;
  sdBitmapFont_.tick();
  if (!checkingBodyFont) sdTitleFont_.tick();
  if (sdBitmapFont_.consumeChanged()) needsRedraw_ = true;
  if (sdTitleFont_.consumeChanged()) needsRedraw_ = true;
  const bool protocolWasConnected = computerStatusProtocol_.isHandshakeComplete();
  computerStatusProtocol_.tick();
  if (computerStatusProtocol_.hasCompletedFrame() && isComputerStatusDetail()) {
    needsRedraw_ = true;
  }
  if (protocolWasConnected != computerStatusProtocol_.isHandshakeComplete()) {
    needsRedraw_ = true;
  }
  const bool staticReaderDetail =
      state_ == UiState::Detail &&
      readerPage_.isStaticWordDetail(homePage_.focusIndex(), sectionFocusIndex_);

  rtcTestService_.tick(nowMs);
  if (rtcTestService_.consumeChanged()) {
    needsRedraw_ = true;
  }
  imuTestService_.tick(nowMs);
  if (imuTestService_.consumeChanged()) needsRedraw_ = true;
  if (!iicScanService_.isBusy() && !rtcTestService_.isBusy() && !imuTestService_.isBusy()) {
    timeService_.tick(nowMs);
  }
  syncHomeClockFromTimeService();
  if (state_ == UiState::Home &&
      (!batteryRefreshStarted_ || nowMs - lastBatteryRefreshMs_ >= kBatteryRefreshIntervalMs)) {
    refreshHomeBattery(nowMs);
  }
  if (timeService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  bluetoothService_.tick(nowMs);
  if (bluetoothService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  otaService_.tick(nowMs);
  if (otaService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  wifiProvisionService_.tick(nowMs, timeService_);
  if (wifiProvisionService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  if (readerService_.consumeChanged()) {
    needsRedraw_ = true;
  }
  if (focusClockPage_.update(nowMs)) {
    needsRedraw_ = true;
  }
  if (answersBookPage_.update(nowMs)) {
    needsRedraw_ = true;
  }
  musicService_.tick(nowMs);
  if (musicService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  iicScanService_.tick(nowMs);
  if (iicScanService_.consumeChanged()) {
    needsRedraw_ = true;
  }
  const InputEdges edges = pollInputEdges();
  if (state_ == UiState::Home) syncHomeSubmenu();
  updateState(edges, nowMs);
  syncUiFont();
  syncContentFonts();
  if (state_ == UiState::Detail &&
      readerPage_.isStaticWordDetail(homePage_.focusIndex(), sectionFocusIndex_) &&
      readerPage_.updateMeaning(readerService_, sdBitmapFont_)) needsRedraw_ = true;

  {
    SdUiText::FixedScope fixedText(display_, uiFont_);
    if (systemUpdatePage_.update(SystemUpdatePage::viewOf(otaService_),
                                 state_ == UiState::Detail && settingsPage_.isOtaSelection(
                                     homePage_.focusIndex(), sectionFocusIndex_),
                                 PopupView::isVisible(confirmState_.anim), nowMs,
                                 display_.text(), &sdBitmapFont_)) {
      needsRedraw_ = true;
    }
  }

  // 弹窗退场动画播完后再执行动作，避免"还没看清就执行了"。
  const uint32_t popupNowMs = millis();
  const PopupView::Result popupResult = PopupView::update(confirmState_, popupNowMs);
  if (popupResult != PopupView::Result::None) {
    if (popupResult == PopupView::Result::Confirmed) {
      performConfirmAction(popupNowMs);
    } else if (state_ == UiState::Popup) {
      state_ = UiState::Home;
    }
    needsRedraw_ = true;
  }

  if (shouldRedraw(nowMs)) {
    render(nowMs);
    lastRenderMs_ = nowMs;
    needsRedraw_ = false;
  }

  updateCpuClock(millis());

  // 空闲时让出 CPU，避免主循环全速空转。
  const uint32_t idleDelayMs = nextIdleDelayMs(millis());
  if (idleDelayMs > 0) {
    delay(idleDelayMs);
  }
}

UiManager::InputEdges UiManager::pollInputEdges() {
  InputEdges edges;
  const uint32_t nowMs = millis();

  auto updateDebounced = [&](ButtonEdge& button, bool& changed) -> bool {
    const bool rawPressed = isPressed(button.pin);
    if (rawPressed != button.lastRawPressed) {
      button.lastRawPressed = rawPressed;
      button.lastRawChangeMs = nowMs;
    }

    changed = false;
    const bool stableEnough = (nowMs - button.lastRawChangeMs) >= kButtonDebounceMs;
    if (stableEnough && button.stablePressed != button.lastRawPressed) {
      const bool prevStable = button.stablePressed;
      button.stablePressed = button.lastRawPressed;
      changed = true;
      return button.stablePressed && !prevStable;
    }

    return false;
  };

  bool ignoredChanged = false;
  (void)updateDebounced(buttons_[0], ignoredChanged);
  if (ignoredChanged && !buttons_[0].stablePressed) {
    edges.left = !leftLongReported_;
    leftLongReported_ = false;
  }
  if (buttons_[0].stablePressed && !leftLongReported_ &&
      (nowMs - buttons_[0].lastRawChangeMs) >= kButtonLongPressMs) {
    edges.leftLong = true;
    leftLongReported_ = true;
  }
  edges.right = updateDebounced(buttons_[1], ignoredChanged);
  edges.up = updateDebounced(buttons_[2], ignoredChanged);
  edges.down = updateDebounced(buttons_[3], ignoredChanged);
  edges.ok = updateDebounced(buttons_[4], edges.okChanged);
  edges.okPressed = buttons_[4].stablePressed;

  return edges;
}

void UiManager::updateState(const InputEdges& edges, uint32_t nowMs) {
  if (edges.left || edges.leftLong || edges.right || edges.up || edges.down || edges.ok) {
    needsRedraw_ = true;
    lastActivityMs_ = nowMs;
  }

  switch (state_) {
    case UiState::Home: {
      syncHomeSubmenu();
      const bool enterDetail =
          homePage_.handleInput(edges.left, edges.right, edges.up, edges.down, edges.ok, nowMs);
      homePage_.update(nowMs);
      syncHomeSubmenu();
      if (enterDetail) {
        sectionFocusIndex_ = homePage_.submenuIndex();
        const SettingsPage::ConfirmKind kind =
            settingsPage_.popupForSelection(homePage_.focusIndex(), sectionFocusIndex_);
        if (kind != SettingsPage::ConfirmKind::None) {
          openConfirm(settingsPage_.confirmTitle(kind),
                      settingsPage_.confirmPrimaryLabel(kind),
                      settingsPage_.confirmDangerLabel(kind),
                      kind == SettingsPage::ConfirmKind::FactoryReset
                          ? ConfirmAction::FactoryReset : ConfirmAction::Restart,
                      nowMs);
          state_ = UiState::Popup;
        } else {
          detailPageIndex_ = 0;
          musicPage_.resetState(homePage_.focusIndex(), sectionFocusIndex_);
          if (settingsPage_.isOtaSelection(homePage_.focusIndex(), sectionFocusIndex_))
            systemUpdatePage_.prepareEnter();
          if (homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex)
            computerStatusPage_.prepareEnter();
          state_ = UiState::ToDetailTransition;
          transitionStartMs_ = nowMs;
        }
        needsRedraw_ = true;
      }
      if (isSleepAllowed() && (nowMs - lastActivityMs_) >= kAutoSleepTimeoutMs) {
        enterSleep(nowMs);
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToDetailTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        focusClockPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_);
        answersBookPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_);
        if (settingsPage_.isAboutDeviceSelection(homePage_.focusIndex(), sectionFocusIndex_))
          refreshSdStatus();
        if (settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_))
          deviceSelfTestPage_.reset();
        musicPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_, musicService_,
                                     sdCardService_);
        readerPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_, sdBitmapFont_);
        setRemotePagePortrait(remotePage_.isPortraitSelection(homePage_.focusIndex(),
                                                              sectionFocusIndex_));
        if (homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex) {
          computerStatusFrameVisible_ = false;
          computerStatusPage_.begin(nowMs);
          computerStatusProtocol_.beginSession();
        }
        if (homePage_.focusIndex() == HomePage::kSdUsbMenuIndex) {
          closeContentFonts();  // Close all font handles/caches before SD changes owner.
          readerService_.reset();
          // Stops audio/scans, closes their files and unmounts SD_MMC before handoff.
          musicService_.exitMusic(sdCardService_);
          sdUsbSwitch_.setEnabled(true);
        }
        state_ = UiState::Detail;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToHomeTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        state_ = UiState::Home;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Popup: {
      if (PopupView::handleInput(confirmState_, edges.left, edges.right, edges.up, edges.down,
                                 edges.ok, nowMs)) {
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Detail: {
      // 详情页上的确认弹窗优先接收输入。
      if (PopupView::isVisible(confirmState_.anim)) {
        if (PopupView::handleInput(confirmState_, edges.left, edges.right, edges.up, edges.down,
                                   edges.ok, nowMs)) {
          needsRedraw_ = true;
        }
        break;
      }

      // 「应用固件」「开始配网」这类动作先弹确认，再由 performConfirmAction 执行。
      const SettingsPage::ConfirmKind detailConfirm = settingsPage_.detailConfirmFor(
          homePage_.focusIndex(), sectionFocusIndex_, otaService_, wifiProvisionService_);
      if (edges.ok && detailConfirm != SettingsPage::ConfirmKind::None) {
        openConfirm(settingsPage_.confirmTitle(detailConfirm),
                    settingsPage_.confirmPrimaryLabel(detailConfirm),
                    settingsPage_.confirmDangerLabel(detailConfirm),
                    detailConfirm == SettingsPage::ConfirmKind::OtaApply
                        ? ConfirmAction::OtaApply
                        : ConfirmAction::WifiStart,
                    nowMs);
        needsRedraw_ = true;
        break;
      }

      const bool isSelfTest = settingsPage_.isDeviceSelfTestSelection(
          homePage_.focusIndex(), sectionFocusIndex_);
      const DeviceSelfTestPage::ButtonState selfTestButtons = {
          buttons_[0].stablePressed, buttons_[1].stablePressed, buttons_[2].stablePressed,
          buttons_[3].stablePressed, buttons_[4].stablePressed};
      if (isSelfTest &&
          deviceSelfTestPage_.handleInput(edges.left, edges.leftLong, edges.up, edges.down,
                                          edges.ok, selfTestButtons, nowMs, iicScanService_,
                                          rtcTestService_, sdCardService_, imuTestService_,
                                          powerDiagnosticService_, peripheralPower_, sdBitmapFont_)) {
        needsRedraw_ = true;
      } else if (edges.leftLong) {
        if (musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_)) closeContentFonts();
        if (musicPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                        musicService_, sdCardService_)) {
          needsRedraw_ = true;
        } else if (gamesPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_)) {
          needsRedraw_ = true;
        } else if (remotePage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                bluetoothService_)) {
          needsRedraw_ = true;
        } else if (timeCalibrationPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                               timeService_)) {
          needsRedraw_ = true;
        } else if (readerPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                readerService_, sdBitmapFont_)) {
          needsRedraw_ = true;
        } else if (answersBookPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_)) {
          needsRedraw_ = true;
        } else if (focusClockPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                   nowMs)) {
          needsRedraw_ = true;
        } else {
          if (homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex) {
            computerStatusFrameVisible_ = false;
            computerStatusProtocol_.endSession();
          }
          sdUsbSwitch_.setEnabled(false);
          settingsPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                          wifiProvisionService_, otaService_, timeService_);
          setRemotePagePortrait(false);
          state_ = UiState::ToHomeTransition;
          transitionStartMs_ = nowMs;
          needsRedraw_ = true;
        }
      } else if (musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
          musicPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                       edges.left, edges.right, edges.up, edges.down, edges.ok, nowMs,
                                       musicService_)) {
        needsRedraw_ = true;
      } else if (edges.left) {
        if (musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_)) closeContentFonts();
        if (musicPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                        musicService_, sdCardService_)) {
          needsRedraw_ = true;
        } else if (gamesPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_)) {
          needsRedraw_ = true;
        } else if (remotePage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                bluetoothService_)) {
          needsRedraw_ = true;
        } else if (timeCalibrationPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                               timeService_)) {
          needsRedraw_ = true;
        } else if (readerPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                                true, edges.right, edges.up, edges.down,
                                                edges.ok, readerService_, sdCardService_, sdBitmapFont_, &sdTitleFont_)) {
          needsRedraw_ = true;
        } else if (readerPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                readerService_, sdBitmapFont_)) {
          needsRedraw_ = true;
        } else if (answersBookPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_)) {
          needsRedraw_ = true;
        } else if (focusClockPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                   nowMs)) {
          // 长按 Left 同样不能绕过"放弃专注"确认。
          needsRedraw_ = true;
        } else {
          if (homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex) {
            computerStatusFrameVisible_ = false;
            computerStatusProtocol_.endSession();
          }
          sdUsbSwitch_.setEnabled(false);
          settingsPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                          wifiProvisionService_, otaService_, timeService_);
          setRemotePagePortrait(false);
          state_ = UiState::ToHomeTransition;
          transitionStartMs_ = nowMs;
          needsRedraw_ = true;
        }
      } else if (settingsPage_.isOtaSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
                 (edges.up || edges.down)) {
        if (systemUpdatePage_.handleLogInput(edges.up, edges.down, nowMs)) needsRedraw_ = true;
      } else if (musicPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              false, edges.right, edges.up, edges.down, edges.ok, nowMs,
                                              musicService_)) {
        needsRedraw_ = true;
      } else if (focusClockPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                                   false, edges.right, edges.up, edges.down,
                                                   edges.ok, nowMs)) {
        needsRedraw_ = true;
      } else if (answersBookPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                                    edges.up, edges.down, edges.ok, nowMs)) {
        needsRedraw_ = true;
      } else if (gamesPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              edges.ok, edges.okPressed, edges.okChanged,
                                              nowMs)) {
        needsRedraw_ = true;
        } else if (remotePage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                               edges.up, edges.down, edges.ok, nowMs,
                                               bluetoothService_,
                                               remoteService_)) {
          needsRedraw_ = true;
      } else if (timeCalibrationPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              edges.up, edges.down, edges.ok, nowMs, timeService_,
                                              wifiProvisionService_)) {
        needsRedraw_ = true;
      } else if (readerPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                               false, edges.right, edges.up, edges.down,
                                               edges.ok, readerService_, sdCardService_, sdBitmapFont_, &sdTitleFont_)) {
        needsRedraw_ = true;
      } else {
        if (settingsPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                             detailPageIndex_,
                                             edges.ok, nowMs, wifiProvisionService_, otaService_)) {
          needsRedraw_ = true;
        } else if (edges.up || edges.down || edges.right || edges.ok) {
          uint8_t pageCount = timeCalibrationPage_.detailPageCount(homePage_.focusIndex(), sectionFocusIndex_);
          if (pageCount == 0U) {
            pageCount = musicPage_.detailPageCount(homePage_.focusIndex(), sectionFocusIndex_);
          }
          if (pageCount == 0U) {
            pageCount = remotePage_.detailPageCount(homePage_.focusIndex(), sectionFocusIndex_);
          }
          if (pageCount == 0U) {
            pageCount = settingsPage_.detailPageCount(homePage_.focusIndex(), sectionFocusIndex_);
          }
          if (pageCount == 0U) {
            pageCount = 1U;
          }
          if (pageCount > 1U) {
            detailPageIndex_ = static_cast<uint8_t>((detailPageIndex_ + 1U) % pageCount);
            needsRedraw_ = true;
          }
        }
      }
      break;
    }
  }
}

bool UiManager::shouldRedraw(uint32_t nowMs) const {
  // 弹窗进场/退场动画期间必须持续出帧。
  if (PopupView::isAnimating(confirmState_.anim)) {
    return true;
  }
  const uint32_t frameIntervalMs = targetFrameIntervalMs(nowMs);
  if (frameIntervalMs > 0 && !needsRedraw_ && (nowMs - lastRenderMs_) < frameIntervalMs) {
    return false;
  }

  if (needsRedraw_) {
    return true;
  }
  if (systemUpdatePage_.needsAnimationFrame()) return true;

  if (state_ == UiState::Home) {
    if (homePage_.isSliding()) {
      return true;
    }
    if (homePage_.hasAnimationTick(nowMs)) {
      return true;
    }
  }

  if (state_ == UiState::ToHomeTransition) {
    return true;
  }
  if (state_ == UiState::ToDetailTransition) {
    return true;
  }
  if (state_ == UiState::Detail &&
      musicPage_.needsAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs)) {
    return true;
  }

  if (state_ == UiState::Detail &&
      focusClockPage_.needsAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs)) {
    return true;
  }

  if (state_ == UiState::Detail &&
      answersBookPage_.needsAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_)) {
    return true;
  }

  if (isComputerStatusDetail() && !computerStatusFrameVisible_ &&
      computerStatusPage_.needsAnimationFrame(nowMs, HomePage::computerStatusIcon())) {
    return true;
  }

  if (state_ == UiState::Detail &&
      settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
      deviceSelfTestPage_.needsAnimationFrame()) {
    return true;
  }

  return false;
}

uint32_t UiManager::targetFrameIntervalMs(uint32_t nowMs) const {
  if (PopupView::isAnimating(confirmState_.anim)) {
    return kHighFrameIntervalMs;
  }
  if (systemUpdatePage_.needsAnimationFrame()) return kTransitionFrameIntervalMs;
  if (state_ == UiState::ToHomeTransition || state_ == UiState::ToDetailTransition) {
    return kTransitionFrameIntervalMs;
  }

  if (state_ == UiState::Home && homePage_.isSliding()) {
    return kHighFrameIntervalMs;
  }


  if (state_ == UiState::Detail &&
      musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_)) {
    return musicPage_.detailFrameIntervalMs(homePage_.focusIndex(), sectionFocusIndex_);
  }

  if (state_ == UiState::Detail &&
      focusClockPage_.isAnimating(homePage_.focusIndex(), sectionFocusIndex_, nowMs)) {
    return kHighFrameIntervalMs;
  }

  if (state_ == UiState::Detail &&
      answersBookPage_.isAnimating(homePage_.focusIndex(), sectionFocusIndex_)) {
    return kHighFrameIntervalMs;
  }

  if (isComputerStatusDetail() && !computerStatusFrameVisible_ &&
      computerStatusPage_.needsAnimationFrame(nowMs, HomePage::computerStatusIcon())) {
    return ComputerStatusPage::kAnimationFrameIntervalMs;
  }

  if (state_ == UiState::Detail &&
      settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
      deviceSelfTestPage_.needsAnimationFrame()) {
    return 0;
  }

  (void)nowMs;
  return 0;
}

uint32_t UiManager::nextIdleDelayMs(uint32_t nowMs) const {
  if (needsRedraw_) {
    return 0;
  }

  // 动画期间按剩余帧时间让出 CPU，保证帧节奏不被拉长。
  const uint32_t frameIntervalMs = targetFrameIntervalMs(nowMs);
  if (frameIntervalMs > 0) {
    const uint32_t elapsed = nowMs - lastRenderMs_;
    if (elapsed >= frameIntervalMs) {
      return 0;
    }
    const uint32_t remaining = frameIntervalMs - elapsed;
    return remaining < kActivePollMs ? remaining : kActivePollMs;
  }

  // 音频播放依赖 loop() 持续喂数据，保持原有不受限的调用节奏。
  if (musicService_.needsRealtimeAudio()) {
    return 0;
  }

  // 配网、OTA、蓝牙和硬件自检需要高频协作式服务，最多让出 1ms。
  if (wifiProvisionService_.isRadioActive() ||
      otaService_.isBusy() ||
      bluetoothService_.isRadioActive() ||
      iicScanService_.isBusy() || rtcTestService_.isBusy() || imuTestService_.isBusy() ||
      sdBitmapFont_.state() == SdBitmapFont::State::Checking ||
      sdTitleFont_.state() == SdBitmapFont::State::Checking ||
      uiFont_.state() == UiFontCache::State::Loading ||
      systemUpdatePage_.isPreparingLogs() || readerPage_.isPreparingMeaning()) {
    return kActivePollMs;
  }

  return BoardConfig::kInputTickMs;
}

void UiManager::updateCpuClock(uint32_t nowMs) {
  const bool audioActive = musicService_.needsRealtimeAudio();
  const bool otaBusy = otaService_.isBusy();
  const bool bluetoothActive = bluetoothService_.isRadioActive();
  // NTP 校时会自行打开 WiFi，不经过配网服务，需要单独判断。
  const bool radioActive = wifiProvisionService_.isRadioActive() ||
                           otaBusy || bluetoothActive ||
                           timeService_.snapshot().syncState ==
                               TimeService::SyncState::Syncing;
  const bool selfTestActive =
      iicScanService_.isBusy() || rtcTestService_.isBusy() || imuTestService_.isBusy() ||
      sdBitmapFont_.state() == SdBitmapFont::State::Checking ||
      sdTitleFont_.state() == SdBitmapFont::State::Checking ||
      uiFont_.state() == UiFontCache::State::Loading ||
      systemUpdatePage_.isPreparingLogs() || readerPage_.isPreparingMeaning();
  // 满屏渲染后保持一段满速窗口；期间只画脏区域的局部帧可以降频执行。
  const bool uiActive = (nowMs - cpuBoostStartMs_) < kCpuBoostHoldMs;

  const bool musicBusy = musicService_.isBusy();
  const uint8_t workMask = (otaBusy ? 1U : 0U) | (audioActive ? 2U : 0U) |
                           (musicBusy ? 4U : 0U) | (bluetoothActive ? 8U : 0U);
  static uint8_t lastWorkMask = 0xFF;
  if (workMask != lastWorkMask) {
    lastWorkMask = workMask;
    LogD("POWER", "ota_busy=%u audio_realtime=%u music_busy=%u bt_active=%u",
         otaBusy ? 1U : 0U, audioActive ? 1U : 0U,
         musicBusy ? 1U : 0U, bluetoothActive ? 1U : 0U);
  }

  CpuClock::setBoost(CpuClock::Boost::Audio, audioActive);
  CpuClock::setBoost(CpuClock::Boost::Radio, radioActive);
  CpuClock::setBoost(CpuClock::Boost::SelfTest, selfTestActive);
  CpuClock::setBoost(CpuClock::Boost::Ui, uiActive);
  CpuClock::apply();
}

void UiManager::render(uint32_t nowMs) {
  SdUiText::FixedScope fixedText(display_, uiFont_);
  // Keep a verified host frame on screen until the next verified frame.
  if (isComputerStatusDetail() && computerStatusFrameVisible_ &&
      !computerStatusProtocol_.hasCompletedFrame()) {
    return;
  }
  // Unrelated clock/service events must not repaint a static waiting page.
  if (isComputerStatusDetail() && !computerStatusProtocol_.hasCompletedFrame() &&
      !computerStatusPage_.needsRedraw(nowMs, HomePage::computerStatusIcon(),
                                      computerStatusProtocol_.isHandshakeComplete())) {
    return;
  }
  const bool navOnlyMusicFrame =
      state_ == UiState::Detail && !needsRedraw_ &&
      musicPage_.needsNavAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs);
  const bool listOnlyMusicFrame =
      state_ == UiState::Detail && !needsRedraw_ && !navOnlyMusicFrame &&
      musicPage_.needsListAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs);
  const bool homeBreadOnlyFrame = state_ == UiState::Home && !needsRedraw_ &&
                                  !navOnlyMusicFrame && !listOnlyMusicFrame &&
                                  homePage_.isBreadOnlyAnimationTick(nowMs);
  const bool homeMenuOnlyFrame =
      state_ == UiState::Home && !needsRedraw_ && !navOnlyMusicFrame && !listOnlyMusicFrame &&
      !homeBreadOnlyFrame && homePage_.isMenuIconsOnlyAnimationTick(nowMs);
  // 滑动中轮盘整体位移，局部刷新不适用；但静态图层可整帧回填，只需推送菜单列。
  const bool homeSlideFrame =
      state_ == UiState::Home && !needsRedraw_ && homePage_.isSliding();

  renderer_.beginFrame();
  if (navOnlyMusicFrame) {
    renderer_.markDirty(0, 112, static_cast<int16_t>(display_.width() - 1),
                        static_cast<int16_t>(display_.height() - 1));
  } else if (listOnlyMusicFrame) {
    renderer_.markDirty(0, 0, static_cast<int16_t>(display_.width() - 1), 111);
  } else if (homeBreadOnlyFrame) {
    const HomePage::Rect card = homePage_.timeCardBounds();
    renderer_.markDirty(card.x1, card.y1, card.x2, card.y2);
  } else if (homeMenuOnlyFrame) {
    const HomePage::Rect icons = homePage_.menuIconBounds(display_);
    renderer_.markDirty(icons.x1, icons.y1, icons.x2, icons.y2);
  } else {
    renderer_.markDirty(0, 0, static_cast<int16_t>(display_.width() - 1),
                        static_cast<int16_t>(display_.height() - 1));
  }

  if (navOnlyMusicFrame) {
    if (musicPage_.renderDetailNavOnly(homePage_.focusIndex(), sectionFocusIndex_, 0, display_,
                                       nowMs)) {
      if (renderer_.hasDirty()) {
        // 音乐导航动画的局部刷新窗口是「部分列 + 全行」，在 ST7305 上窗口映射会错位，
        // 实机表现为导航栏动画被画到屏幕顶部；这里退回整屏推送，代价仅为动画期间的少量 SPI。
        display_.present();
      }
      return;
    }
  }

  if (listOnlyMusicFrame) {
    if (musicPage_.renderDetailListOnly(homePage_.focusIndex(), sectionFocusIndex_, display_,
                                        musicService_, nowMs, &sdBitmapFont_)) {
      if (renderer_.hasDirty()) {
        // 与导航动画同理，列表焦点动画同样退回整屏推送。
        display_.present();
      }
      return;
    }
  }

  if (homeBreadOnlyFrame) {
    homePage_.renderTimeCardOnly(display_, nowMs);
    if (renderer_.hasDirty()) {
#if OB_PARTIAL_REFRESH_ENABLED
      const Render1bpp::Rect region = renderer_.dirty();
      display_.presentRegion(region.x1, region.y1, region.x2, region.y2);
#else
      display_.present();
#endif
    }
    return;
  }

  if (homeMenuOnlyFrame) {
    homePage_.renderMenuIconsOnly(display_, nowMs);
    if (renderer_.hasDirty()) {
#if OB_PARTIAL_REFRESH_ENABLED
      const Render1bpp::Rect region = renderer_.dirty();
      display_.presentRegion(region.x1, region.y1, region.x2, region.y2);
#else
      display_.present();
#endif
    }
    return;
  }

  // 满屏渲染前先恢复满速，避免整帧在降频状态下执行。
  cpuBoostStartMs_ = nowMs;
  CpuClock::setBoost(CpuClock::Boost::Ui, true);
  CpuClock::apply();

  display_.clear();

  if (isComputerStatusDetail() && computerStatusProtocol_.hasCompletedFrame()) {
    renderComputerStatusFrame();
    computerStatusProtocol_.markFrameDisplayed();
    computerStatusFrameVisible_ = true;
  }

  if (isComputerStatusDetail() && computerStatusFrameVisible_) {
    // The host frame was rendered above.
  } else if (state_ == UiState::Home) {
    homePage_.render(display_, 0, nowMs);
  } else if (state_ == UiState::ToDetailTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t height = static_cast<int16_t>(display_.height());
    const float t = static_cast<float>(elapsed) / kDetailTransitionMs;
    const int16_t detailOffsetY = easeOutCubic(height, 0, t);
    homePage_.render(display_, 0, nowMs);
    renderDetail(detailOffsetY);
  } else if (state_ == UiState::ToHomeTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t height = static_cast<int16_t>(display_.height());
    const float t = static_cast<float>(elapsed) / kDetailTransitionMs;
    const int16_t detailOffsetY = easeInCubic(0, height, t);
    homePage_.render(display_, 0, nowMs);
    renderDetail(detailOffsetY);
  } else if (state_ == UiState::Popup) {
    homePage_.render(display_, 0, nowMs);
    renderPopup(nowMs);
  } else {
    renderDetail();
  }

  if (renderer_.hasDirty()) {
    const bool transitionFrame =
        state_ == UiState::ToHomeTransition || state_ == UiState::ToDetailTransition;
#if OB_PARTIAL_REFRESH_ENABLED
    // 滑动中只有菜单列在变；静态图层是整帧回填的，其余区域与屏幕内容一致，
    // 因此只需把菜单列推给面板。
    if (homeSlideFrame && homePage_.frameUsedStaticLayer()) {
      const HomePage::Rect region = homePage_.menuColumnBounds(display_);
      display_.presentRegion(region.x1, region.y1, region.x2, region.y2);
    } else if (transitionFrame) {
      display_.presentSynced();
    } else {
      display_.present();
    }
#else
    if (transitionFrame) {
      display_.presentSynced();
    } else {
      display_.present();
    }
#endif
  }
}

bool UiManager::isComputerStatusDetail() const {
  return state_ == UiState::Detail &&
         homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex;
}

void UiManager::renderComputerStatusFrame() {
  const uint8_t* frame = computerStatusProtocol_.completedFrame();
  if (frame == nullptr || computerStatusProtocol_.completedFrameBytes() == 0U) return;

  auto& canvas = display_.canvas();
  for (uint16_t y = 0; y < ComputerStatusProtocol::kCanvasHeight; ++y) {
    uint16_t x = 0;
    while (x < ComputerStatusProtocol::kCanvasWidth) {
      const uint32_t bit = y * ComputerStatusProtocol::kCanvasWidth + x;
      if ((frame[bit / 8U] & (0x80U >> (bit % 8U))) == 0U) {
        ++x;
        continue;
      }
      const uint16_t start = x++;
      while (x < ComputerStatusProtocol::kCanvasWidth) {
        const uint32_t nextBit = y * ComputerStatusProtocol::kCanvasWidth + x;
        if ((frame[nextBit / 8U] & (0x80U >> (nextBit % 8U))) == 0U) break;
        ++x;
      }
      canvas.drawFastHLine(start, y, x - start, ST7305_COLOR_BLACK);
    }
  }
}

void UiManager::openConfirm(const char* title, const char* primaryLabel, const char* dangerLabel,
                            ConfirmAction action, uint32_t nowMs) {
  confirmTitle_ = title;
  confirmPrimaryLabel_ = primaryLabel;
  confirmDangerLabel_ = dangerLabel;
  confirmAction_ = action;
  PopupView::open(confirmState_, nowMs);
}

void UiManager::performConfirmAction(uint32_t nowMs) {
  switch (confirmAction_) {
    case ConfirmAction::Restart:
      LogI("SLEEP", "user confirmed restart");
      ESP.restart();
      break;
    case ConfirmAction::FactoryReset:
      performFactoryReset();
      break;
    case ConfirmAction::OtaApply:
      closeContentFonts(); // Release SD resources before flash activation/restart.
      [[fallthrough]];
    case ConfirmAction::WifiStart:
      // 复用详情页原有的 OK 处理：确认后带一次 OK 边沿执行动作。
      (void)settingsPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                            detailPageIndex_, true, nowMs,
                                            wifiProvisionService_, otaService_);
      break;
    case ConfirmAction::None:
    default:
      break;
  }
  confirmAction_ = ConfirmAction::None;
}

void UiManager::performFactoryReset() {
  LogI("SLEEP", "user confirmed factory reset: erasing NVS");
  (void)nvs_flash_deinit();
  const esp_err_t err = nvs_flash_erase();
  LogI("SLEEP", "nvs erase err=%d; restarting", static_cast<int>(err));
  ESP.restart();
}

void UiManager::renderPopup(uint32_t nowMs) {
  PopupView::drawConfirm(display_, 0, confirmState_, confirmTitle_, confirmPrimaryLabel_,
                         confirmDangerLabel_, nowMs, true);
}

void UiManager::setRemotePagePortrait(bool enabled) {
  // Default is 90 degrees clockwise; rotation 2 is the next clockwise quarter-turn.
  const uint8_t desiredRotation = enabled ? 2U : BoardConfig::kDisplayRotation;
  if (display_.rotation() == desiredRotation) {
    return;
  }
  display_.setRotation(desiredRotation);
  LogI("DISPLAY", "UI rotation=%u (%s)", static_cast<unsigned>(desiredRotation),
       enabled ? "portrait remote" : "default landscape");
  needsRedraw_ = true;
}

void UiManager::renderDetail(int16_t yOffset) {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());

  if (yOffset < height) {
    canvas.drawFilledRectangle(0, yOffset, width - 1, height - 1, ST7305_COLOR_WHITE);
  }

  bool handled = false;
  if (homePage_.focusIndex() == HomePage::kComputerStatusMenuIndex) {
    computerStatusPage_.render(display_, yOffset, HomePage::computerStatusIcon(),
                                computerStatusProtocol_.isHandshakeComplete(), millis());
    handled = true;
  } else if (homePage_.focusIndex() == HomePage::kSdUsbMenuIndex) {
    sdUsbPage_.render(display_, yOffset, HomePage::sdUsbIcon(), sdUsbSwitch_.enabled());
    handled = true;
  } else if (settingsPage_.isOtaSelection(homePage_.focusIndex(), sectionFocusIndex_)) {
    systemUpdatePage_.render(display_, yOffset, SystemUpdatePage::viewOf(otaService_), millis(), &sdBitmapFont_);
    handled = true;
  } else if (settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_)) {
    deviceSelfTestPage_.render(display_, yOffset, millis(), iicScanService_, rtcTestService_,
                               sdCardService_, imuTestService_, powerDiagnosticService_, sdBitmapFont_);
    handled = true;
  } else if (gamesPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_)) {
    handled = true;
  } else if (focusClockPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset,
                                          display_, millis(), timeService_)) {
    handled = true;
  } else if (answersBookPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset,
                                           display_, millis(), &sdBitmapFont_, &sdTitleFont_)) {
    handled = true;
  } else if (musicPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                                     musicService_, millis(), &sdBitmapFont_)) {
    handled = true;
  } else if (readerPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                                      readerService_, sdBitmapFont_, &sdTitleFont_)) {
    handled = true;
  } else if (remotePage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                                      bluetoothService_, remoteService_)) {
    handled = true;
  } else if (timeCalibrationPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset,
                                               display_, timeService_)) {
    handled = true;
  } else if (settingsPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_,
                                        detailPageIndex_, yOffset, display_, deviceIdText_,
                                        flashTotalText_, sdStatusText_, wifiProvisionService_,
                                        timeService_, &sdBitmapFont_)) {
    handled = true;
  }

  if (!handled) {
    canvas.drawFilledRectangle(0, yOffset, width - 1,
                               static_cast<int16_t>(yOffset + ThemeMono::kHeaderHeight),
                               ST7305_COLOR_BLACK);
    text.setFont(u8g2_font_7x14_tf);
    text.setForegroundColor(ST7305_COLOR_WHITE);
    text.drawUTF8(6, static_cast<int16_t>(yOffset + 24), "详情页");

    text.setForegroundColor(ST7305_COLOR_BLACK);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 62), homePage_.focusName());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 92), "详情内容开发中");
    text.drawUTF8(width - 92, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回");

    canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                         static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
  }

  // 详情页的确认弹窗（应用固件 / 开始配网）覆盖在页面之上。
  PopupView::drawConfirm(display_, yOffset, confirmState_, confirmTitle_, confirmPrimaryLabel_,
                         confirmDangerLabel_, millis());
}

void UiManager::closeContentFonts() {
  uiFont_.cancelLoad();  // Ready UI glyphs remain usable after SD ownership changes.
  sdTitleFont_.end();
  sdBitmapFont_.end();
  contentFontUse_ = ContentFontUse::None;
}

void UiManager::syncUiFont() {
  if (state_ != UiState::Home) {
    uiFont_.cancelLoad();
    uiFontAttempted_ = false;
    return;
  }
  // First paint and navigation never depend on a font file. Attempt once per
  // home visit; no repeated mount/CRC loop on absent, corrupt or removed cards.
  if (!uiFontAttempted_ && lastRenderMs_ != 0 && !musicService_.isBusy()) {
    uiFontAttempted_ = true;
    if (uiFont_.state() != UiFontCache::State::Ready) {
      if (sdCardService_.status() != SdCardService::Status::Ready) sdCardService_.refresh();
      (void)uiFont_.begin();
    }
  }
  uiFont_.tick();
  const auto fontState = uiFont_.state();
  const auto status = fontState == UiFontCache::State::Ready ? HomePage::FontStatus::Ready
      : (fontState == UiFontCache::State::Error ? HomePage::FontStatus::Unavailable
                                               : HomePage::FontStatus::Loading);
  if (homePage_.setFontStatus(status)) needsRedraw_ = true;
  if (uiFont_.consumeChanged()) {
    homePage_.invalidateTextLayer();
    needsRedraw_ = true;
    // Show the result before starting a fresh idle window, even if extraction
    // already took longer than the normal home timeout.
    lastActivityMs_ = millis();
  }
}

void UiManager::syncContentFonts() {
  // Keep outgoing typography through its exit animation. Start I/O only after entry.
  if (state_ != UiState::Detail && state_ != UiState::Home) return;
  ContentFontUse next = ContentFontUse::None;
  if (state_ == UiState::Detail) {
    const uint8_t home = homePage_.focusIndex();
    if (answersBookPage_.isSelection(home, sectionFocusIndex_)) next = ContentFontUse::Answers;
    else if (readerPage_.isStaticWordDetail(home, sectionFocusIndex_)) next = ContentFontUse::Reader;
    else if (settingsPage_.isOtaSelection(home, sectionFocusIndex_)) {
      const auto state = otaService_.state();
      if (state == OtaService::State::Idle || state == OtaService::State::UpToDate ||
          state == OtaService::State::UpdateAvailable || state == OtaService::State::ReadyToApply)
        next = ContentFontUse::Update;
    } else if (musicPage_.isMusicListSelection(home, sectionFocusIndex_) ||
               settingsPage_.isWifiProvisionSelection(home, sectionFocusIndex_))
      next = ContentFontUse::External;
  }
  if (next == contentFontUse_) {
    if (next == ContentFontUse::Reader && sdTitleFont_.state() == SdBitmapFont::State::Closed &&
        sdBitmapFont_.state() == SdBitmapFont::State::Checking) (void)sdTitleFont_.open(24, false);
    return;
  }
  if (contentFontUse_ != ContentFontUse::None && contentFontUse_ != ContentFontUse::Reader)
    sdBitmapFont_.end();
  sdTitleFont_.end();
  contentFontUse_ = next;
  if (next == ContentFontUse::None) return;
  if (next != ContentFontUse::Reader) {
    // Music already owns/mounts SD. Never refresh it while its files are open.
    const bool music = musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_);
    if (sdCardService_.status() != SdCardService::Status::Ready && !music) sdCardService_.refresh();
    (void)sdBitmapFont_.open(14);
  }
  if (next == ContentFontUse::Answers || next == ContentFontUse::Reader)
    (void)sdTitleFont_.open(next == ContentFontUse::Answers ? 20 : 24, false);
  needsRedraw_ = true;
}

void UiManager::refreshSdStatus() {
  const SdCardService::Status status = sdCardService_.refresh();
  if (status == SdCardService::Status::NotInserted) {
    snprintf(sdStatusText_, sizeof(sdStatusText_), "SD卡：未插入");
    return;
  }

  if (status == SdCardService::Status::ReinsertNeeded) {
    snprintf(sdStatusText_, sizeof(sdStatusText_), "SD卡：重新插拔SD卡");
    return;
  }

  const double freeGb =
      static_cast<double>(sdCardService_.freeBytes()) / (1024.0 * 1024.0 * 1024.0);
  const double totalGb =
      static_cast<double>(sdCardService_.totalBytes()) / (1024.0 * 1024.0 * 1024.0);
  snprintf(sdStatusText_, sizeof(sdStatusText_), "SD卡：%.1fG可用/%.1fG总容量", freeGb, totalGb);
}

void UiManager::syncHomeClockFromTimeService() {
  const TimeService::Snapshot& snapshot = timeService_.snapshot();
  HomePage::ClockData clock;
  if (snapshot.valid) {
    clock.second = snapshot.now.second;
    clock.minute = snapshot.now.minute;
    clock.hour = snapshot.now.hour;
    clock.day = snapshot.now.day;
    clock.weekday = snapshot.now.weekday;
    clock.month = snapshot.now.month;
    clock.year = snapshot.now.year;
    clock.valid = true;
  } else {
    clock.valid = false;
  }
  homePage_.setClockData(clock);
}

void UiManager::syncHomeSubmenu() {
  if (homePage_.focusIndex() == SettingsPage::kHomeIndex) {
    const uint8_t count = settingsPage_.menuItemCount();
    const uint8_t boundedCount = count > 16U ? 16U : count;
    homePage_.setSubmenuLabels(settingsPage_.menuLabels(), boundedCount);
    return;
  }
  const SubmenuContent content = submenuContentFor(homePage_.focusIndex());
  homePage_.setSubmenuLabels(content.labels, content.count);
}

void UiManager::refreshHomeBattery(uint32_t nowMs) {
  if (!powerDiagnosticService_.refresh()) {
    const bool displayChanged = homePage_.setBatteryData({0, false});
    lastBatteryRefreshMs_ = nowMs;
    batteryRefreshStarted_ = true;
    if (state_ == UiState::Home && displayChanged) {
      needsRedraw_ = true;
    }
    return;
  }

  const uint16_t socX100 = powerDiagnosticService_.reading().stateOfChargeX100;
  const uint8_t percent = socX100 >= 10000U
                              ? 100U
                              : static_cast<uint8_t>((socX100 + 50U) / 100U);
  const bool displayChanged = homePage_.setBatteryData({percent, true});
  lastBatteryRefreshMs_ = nowMs;
  batteryRefreshStarted_ = true;
  if (state_ == UiState::Home && displayChanged) {
    needsRedraw_ = true;
  }
}

bool UiManager::isSleepAllowed() const {
  if (state_ != UiState::Home) {
    return false;
  }
  // Slow SD extraction may outlast the 20s idle timer. Sleeping here would
  // cancel startup on every attempt and leave all fixed Chinese unavailable.
  if (uiFont_.state() == UiFontCache::State::Loading) return false;
  if (iicScanService_.isBusy() || rtcTestService_.isBusy() || imuTestService_.isBusy()) {
    return false;
  }
  if (bluetoothService_.isRadioActive()) {
    return false;
  }
  if (wifiProvisionService_.isRadioActive()) {
    return false;
  }
  if (otaService_.isBusy()) {
    return false;
  }
  if (musicService_.isBusy()) {
    return false;
  }
  if (timeService_.snapshot().syncState == TimeService::SyncState::Syncing) {
    return false;
  }
  return true;
}

void UiManager::resetButtonDebounceState() {
  const uint32_t nowMs = millis();
  for (auto& button : buttons_) {
    const bool pressed = isPressed(button.pin);
    button.lastRawPressed = pressed;
    button.stablePressed = pressed;
    button.lastRawChangeMs = nowMs;
  }
  leftLongReported_ = buttons_[0].stablePressed;
}

void UiManager::enterSleep(uint32_t nowMs) {
  const uint32_t idleMs = nowMs - lastActivityMs_;
  LogI("SLEEP", "enter home auto-sleep idle=%lums", static_cast<unsigned long>(idleMs));
  SdUiText::FixedScope fixedText(display_, uiFont_);

  const size_t frameBytes = display_.frameBufferBytes();
  auto* homeFrame = static_cast<uint8_t*>(
      heap_caps_malloc(frameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (homeFrame != nullptr) {
    display_.captureFrame(homeFrame);
  } else {
    LogW("SLEEP", "XMB transition snapshot unavailable bytes=%u; use direct clock switch",
         static_cast<unsigned>(frameBytes));
  }
  const auto animateClock = [&](bool entering, uint32_t clockNowMs) {
    if (homeFrame == nullptr) {
      display_.clear();
      if (entering) homePage_.renderSleepClock(display_, clockNowMs);
      else homePage_.render(display_, 0, clockNowMs);
      display_.presentSynced();
      return;
    }
    auto* sleepFrame = static_cast<uint8_t*>(
        heap_caps_malloc(frameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (sleepFrame != nullptr) {
      display_.clear();
      homePage_.renderSleepClock(display_, clockNowMs);
      display_.captureFrame(sleepFrame);
    } else {
      LogW("SLEEP", "clock frame cache unavailable bytes=%u; render clock every frame",
           static_cast<unsigned>(frameBytes));
    }
    const uint32_t startedMs = millis();
    uint16_t lastProgress = 0xFFFFU;
    uint32_t frameCount = 0;
    uint32_t renderTotalUs = 0;
    uint32_t renderMaxUs = 0;
    uint32_t presentTotalUs = 0;
    uint32_t presentMaxUs = 0;
    uint32_t teWaitTotalUs = 0;
    uint32_t teWaitMaxUs = 0;
    for (;;) {
      const uint32_t elapsedMs = millis() - startedMs;
      const uint16_t eased = AnimMath::fixedEaseInOut(
          AnimMath::fixedClampProgress(elapsedMs, kSleepTransitionMs));
      const uint16_t progress = entering
          ? eased
          : static_cast<uint16_t>(AnimMath::kFixedScale - eased);
      if (progress != lastProgress) {
        const uint32_t renderStartedUs = micros();
        homePage_.renderSleepTransition(display_, clockNowMs, progress, homeFrame, sleepFrame);
        const uint32_t renderUs = micros() - renderStartedUs;
        display_.presentSynced();
        const uint32_t presentUs = display_.lastPresentUs();
        const uint32_t teWaitUs = display_.lastTeWaitUs();
        ++frameCount;
        renderTotalUs += renderUs;
        renderMaxUs = renderUs > renderMaxUs ? renderUs : renderMaxUs;
        presentTotalUs += presentUs;
        presentMaxUs = presentUs > presentMaxUs ? presentUs : presentMaxUs;
        teWaitTotalUs += teWaitUs;
        teWaitMaxUs = teWaitUs > teWaitMaxUs ? teWaitUs : teWaitMaxUs;
        lastProgress = progress;
      }
      if (elapsedMs >= kSleepTransitionMs) break;
      const uint32_t nextElapsed =
          (elapsedMs / kSleepTransitionFrameMs + 1U) * kSleepTransitionFrameMs;
      const int32_t waitMs = static_cast<int32_t>(startedMs + nextElapsed - millis());
      if (waitMs > 0) delay(static_cast<uint32_t>(waitMs));
    }
    const uint32_t elapsedUs = (millis() - startedMs) * 1000U;
    LogI("SLEEP", "XMB transition=%s frames=%lu elapsed_ms=%lu render_avg_us=%lu render_max_us=%lu "
                  "te_wait_avg_us=%lu te_wait_max_us=%lu present_avg_us=%lu present_max_us=%lu te=%s",
         entering ? "enter" : "wake", static_cast<unsigned long>(frameCount),
         static_cast<unsigned long>(elapsedUs / 1000U),
         static_cast<unsigned long>(frameCount == 0U ? 0U : renderTotalUs / frameCount),
         static_cast<unsigned long>(renderMaxUs),
         static_cast<unsigned long>(frameCount == 0U ? 0U : teWaitTotalUs / frameCount),
         static_cast<unsigned long>(teWaitMaxUs),
         static_cast<unsigned long>(frameCount == 0U ? 0U : presentTotalUs / frameCount),
         static_cast<unsigned long>(presentMaxUs),
         display_.teSyncAvailable() ? "available" : "unavailable");
    if (sleepFrame != nullptr) heap_caps_free(sleepFrame);
  };

  const bool rtcWakeReady = rtcDriver_.setMinuteInterruptEnabled(true);
  if (rtcWakeReady) {
    pinMode(BoardConfig::kPinPcfInt, INPUT_PULLUP);
    gpio_wakeup_enable(static_cast<gpio_num_t>(BoardConfig::kPinPcfInt),
                       GPIO_INTR_LOW_LEVEL);
    LogI("SLEEP", "rtc minute interrupt wake enabled int_gpio=%d",
         static_cast<int>(BoardConfig::kPinPcfInt));
  } else {
    LogI("SLEEP", "rtc minute interrupt unavailable; time refresh wake disabled");
  }

  animateClock(true, nowMs);
  if (homeFrame != nullptr) {
    heap_caps_free(homeFrame);
    homeFrame = nullptr;
  }

  display_.prepareForSleepKeepDisplay();
  peripheralPower_.setSensorEnabled(false);
  uiFontAttempted_ = false;
  closeContentFonts();
  sdCardService_.end();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_OFF);
  LogI("SLEEP", "power domains off: rtc_periph modem");

  bool keepSleeping = true;
  while (keepSleeping) {
    for (const auto& button : buttons_) {
      gpio_wakeup_enable(static_cast<gpio_num_t>(button.pin), GPIO_INTR_LOW_LEVEL);
    }
    if (rtcWakeReady) {
      gpio_wakeup_enable(static_cast<gpio_num_t>(BoardConfig::kPinPcfInt),
                         GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_gpio_wakeup();

    const esp_err_t sleepErr = esp_light_sleep_start();
    const uint32_t wakeNowMs = millis();

    if (sleepErr != ESP_OK) {
      LogW("SLEEP", "light sleep rejected err=%d", static_cast<int>(sleepErr));
      keepSleeping = false;
      break;
    }

    bool buttonPressed = false;
    for (const auto& button : buttons_) {
      if (isPressed(button.pin)) {
        buttonPressed = true;
        break;
      }
    }

    const bool rtcLineLow =
        rtcWakeReady && (digitalRead(BoardConfig::kPinPcfInt) == LOW);

    if (!buttonPressed && rtcLineLow) {
      display_.restoreAfterSleep();
      peripheralPower_.setSensorEnabled(true);
      timeService_.tick(wakeNowMs);
      syncHomeClockFromTimeService();
      needsRedraw_ = true;
      display_.clear();
      homePage_.renderSleepClock(display_, wakeNowMs);
      display_.present();
      lastRenderMs_ = wakeNowMs;
      needsRedraw_ = false;
      if (!rtcDriver_.clearTimerFlag()) {
        LogW("SLEEP", "rtc timer flag clear failed after minute wake; exit light sleep");
        keepSleeping = false;
        break;
      }
      display_.prepareForSleepKeepDisplay();
      peripheralPower_.setSensorEnabled(false);
      LogI("SLEEP", "rtc minute wake at %lums; time refreshed, continue sleep",
           static_cast<unsigned long>(wakeNowMs));
      continue;
    }

    if (buttonPressed) {
      LogI("SLEEP", "button wake; exit sleep");
    } else {
      LogI("SLEEP", "unknown wake cause=%d; exit sleep",
           static_cast<int>(esp_sleep_get_wakeup_cause()));
    }
    keepSleeping = false;
  }

  display_.restoreAfterSleep();
  peripheralPower_.setSensorEnabled(true);
  if (rtcWakeReady) {
    (void)rtcDriver_.setMinuteInterruptEnabled(false);
  }
  const uint32_t exitNowMs = millis();
  resetButtonDebounceState();
  timeService_.tick(exitNowMs);
  syncHomeClockFromTimeService();
  homePage_.resumeAnimation(exitNowMs);
  homeFrame = static_cast<uint8_t*>(
      heap_caps_malloc(frameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (homeFrame != nullptr) {
    display_.clear();
    homePage_.render(display_, 0, exitNowMs);
    display_.captureFrame(homeFrame);
  } else {
    LogW("SLEEP", "XMB wake transition snapshot unavailable bytes=%u; restore home directly",
         static_cast<unsigned>(frameBytes));
  }
  animateClock(false, exitNowMs);
  if (homeFrame != nullptr) heap_caps_free(homeFrame);
  const uint32_t wakeFinishedMs = millis();
  homePage_.resumeAnimation(wakeFinishedMs);
  lastActivityMs_ = wakeFinishedMs;
  lastRenderMs_ = wakeFinishedMs;
  needsRedraw_ = true;
  LogI("SLEEP", "exit home auto-sleep at %lums", static_cast<unsigned long>(wakeFinishedMs));
}

bool UiManager::isPressed(uint8_t pin) const { return digitalRead(pin) == LOW; }

int16_t UiManager::easeInCubic(int16_t from, int16_t to, float t) const {
  return AnimMath::easeInCubicInt16(from, to, t);
}

int16_t UiManager::easeOutCubic(int16_t from, int16_t to, float t) const {
  return AnimMath::easeOutCubicInt16(from, to, t);
}
