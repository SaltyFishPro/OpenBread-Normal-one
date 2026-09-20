#include "UiManager.h"

#include <cstdio>

#include "../bsp/BoardConfig.h"
#include <cstdarg>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include "IconBitmap.h"
#include "ThemeMono.h"
#include "../services/SdCardService.h"
#include "assets/games/icons8-airplanewars.h"
#include "assets/games/icons8-tapthewoodenfish.h"
#include "assets/submenu/bluetoothconnet.h"
#include "assets/submenu/likemusic.h"
#include "assets/submenu/musiclist.h"
#include "assets/submenu/remote.h"
#include "assets/submenu/teleprompter.h"
#include "assets/submenu/vocabularybook.h"
#include "assets/main_menu/icons8-clock.h"
#include "assets/ui/pop_up_window.h"

namespace {
#ifndef OB_SLEEP_LOG_ENABLED
#define OB_SLEEP_LOG_ENABLED 1
#endif

constexpr uint32_t kAutoSleepTimeoutMs = 20000;

void sleepLog(const char* fmt, ...) {
#if OB_SLEEP_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[SLEEP] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

constexpr uint32_t kSectionTransitionMs = 600;
constexpr uint32_t kForwardBgEndMs = 90;
constexpr uint32_t kForwardMenuStartMs = 70;
constexpr uint32_t kForwardMenuEndMs = 500;
constexpr uint32_t kForwardSectionStartMs = 540;

constexpr uint32_t kBackSectionEndMs = 200;
constexpr uint32_t kBackMenuStartMs = 180;
constexpr uint32_t kBackMenuEndMs = 340;
constexpr uint32_t kBackBgStartMs = 300;
constexpr uint32_t kDetailTransitionMs = 420;
constexpr uint32_t kDetailForwardIconEndMs = 120;
constexpr uint32_t kDetailForwardRowsStartMs = 80;
constexpr uint32_t kDetailForwardRowsEndMs = 320;
constexpr uint32_t kDetailForwardDetailStartMs = 220;
constexpr uint32_t kDetailBackDetailEndMs = 200;
constexpr uint32_t kDetailBackRowsStartMs = 180;
constexpr uint32_t kDetailBackRowsEndMs = 340;
constexpr uint32_t kDetailBackIconStartMs = 300;
constexpr uint32_t kButtonDebounceMs = 25;
constexpr uint32_t kButtonLongPressMs = 650;
constexpr uint32_t kSectionFocusSlideMs = 170;
constexpr uint8_t kSectionPageSize = 5;
constexpr uint8_t kMaxSectionItemsForAnim = 8;
constexpr int16_t kRestartPopupOptionWidth = 44;
constexpr int16_t kRestartPopupOptionGap = 16;
constexpr int16_t kRestartPopupTitleTopOffset = 35;
constexpr int16_t kRestartPopupOptionBottomMargin = 8;
constexpr int16_t kRestartPopupOptionPadY = 4;
constexpr uint32_t kHighFrameIntervalMs = 16;
constexpr HomePage::Language kUiLanguage = HomePage::Language::Zh;

const IconBitmap::Anim kRestartPopupWindow = {
    reinterpret_cast<const uint8_t*>(&pop_up_window_frames[0][0]),
    POP_UP_WINDOW_FRAME_BYTES,
    POP_UP_WINDOW_FRAME_WIDTH,
    POP_UP_WINDOW_FRAME_HEIGHT,
    POP_UP_WINDOW_FRAME_DELAY,
    POP_UP_WINDOW_FRAME_COUNT};

using SectionItem = SettingsPage::MenuItem;

struct SectionContent {
  const SectionItem* items;
  uint8_t itemCount;
};

const SectionItem kMusicItems[] = {
    {"音乐列表", "Music List",
     {reinterpret_cast<const uint8_t*>(&musiclist_frames[0][0]), MUSICLIST_FRAME_BYTES,
      MUSICLIST_FRAME_WIDTH, MUSICLIST_FRAME_HEIGHT, MUSICLIST_FRAME_DELAY, MUSICLIST_FRAME_COUNT}},
    {"收藏歌曲", "Favorites",
     {reinterpret_cast<const uint8_t*>(&likemusic_frames[0][0]), LIKEMUSIC_FRAME_BYTES,
      LIKEMUSIC_FRAME_WIDTH, LIKEMUSIC_FRAME_HEIGHT, LIKEMUSIC_FRAME_DELAY, LIKEMUSIC_FRAME_COUNT}},
};

const SectionItem kReaderItems[] = {
    {"单词阅读", "Vocabulary",
     {reinterpret_cast<const uint8_t*>(&vocabularybook_frames[0][0]),
      VOCABULARYBOOK_FRAME_BYTES, VOCABULARYBOOK_FRAME_WIDTH, VOCABULARYBOOK_FRAME_HEIGHT,
      VOCABULARYBOOK_FRAME_DELAY, VOCABULARYBOOK_FRAME_COUNT}},
    {"提词器", "Teleprompter",
     {reinterpret_cast<const uint8_t*>(&teleprompter_frames[0][0]),
      TELEPROMPTER_FRAME_BYTES, TELEPROMPTER_FRAME_WIDTH, TELEPROMPTER_FRAME_HEIGHT,
      TELEPROMPTER_FRAME_DELAY, TELEPROMPTER_FRAME_COUNT}},
};

const SectionItem kFocusClockItems[] = {
    {"专注时钟", "Focus Clock",
     {reinterpret_cast<const uint8_t*>(&clock_frames[0][0]), CLOCK_FRAME_BYTES,
      CLOCK_FRAME_WIDTH, CLOCK_FRAME_HEIGHT, CLOCK_FRAME_DELAY, CLOCK_FRAME_COUNT}},
};

const SectionItem kWirelessItems[] = {
    {"蓝牙连接", "Bluetooth",
     {reinterpret_cast<const uint8_t*>(&bluetoothconnet_frames[0][0]),
      BLUETOOTHCONNET_FRAME_BYTES, BLUETOOTHCONNET_FRAME_WIDTH, BLUETOOTHCONNET_FRAME_HEIGHT,
      BLUETOOTHCONNET_FRAME_DELAY, BLUETOOTHCONNET_FRAME_COUNT}},
    {"蓝牙远程拍照", "BT Remote Cam",
     {reinterpret_cast<const uint8_t*>(&remote_frames[0][0]), REMOTE_FRAME_BYTES,
      REMOTE_FRAME_WIDTH, REMOTE_FRAME_HEIGHT, REMOTE_FRAME_DELAY, REMOTE_FRAME_COUNT}},
};

const SectionItem kGamesItems[] = {
    {"敲木鱼", "Tap Wooden Fish",
     {reinterpret_cast<const uint8_t*>(&icons8_tapthewoodenfish_frames[0][0]),
      ICONS8_TAPTHEWOODENFISH_FRAME_BYTES, ICONS8_TAPTHEWOODENFISH_FRAME_WIDTH,
      ICONS8_TAPTHEWOODENFISH_FRAME_HEIGHT, ICONS8_TAPTHEWOODENFISH_FRAME_DELAY,
      ICONS8_TAPTHEWOODENFISH_FRAME_COUNT}},
    {"飞机世界大战", "Airplane World War",
     {reinterpret_cast<const uint8_t*>(&icons8_airplanewars_frames[0][0]),
      ICONS8_AIRPLANEWARS_FRAME_BYTES, ICONS8_AIRPLANEWARS_FRAME_WIDTH,
      ICONS8_AIRPLANEWARS_FRAME_HEIGHT, ICONS8_AIRPLANEWARS_FRAME_DELAY,
      ICONS8_AIRPLANEWARS_FRAME_COUNT}},
};

const SectionContent kSectionContents[] = {
    {kMusicItems, static_cast<uint8_t>(sizeof(kMusicItems) / sizeof(kMusicItems[0]))},
    {kReaderItems, static_cast<uint8_t>(sizeof(kReaderItems) / sizeof(kReaderItems[0]))},
    {kFocusClockItems, static_cast<uint8_t>(sizeof(kFocusClockItems) / sizeof(kFocusClockItems[0]))},
    {kWirelessItems, static_cast<uint8_t>(sizeof(kWirelessItems) / sizeof(kWirelessItems[0]))},
    {kGamesItems, static_cast<uint8_t>(sizeof(kGamesItems) / sizeof(kGamesItems[0]))},
};

SectionContent sectionContentFor(uint8_t homeFocus, const SettingsPage& settingsPage) {
  if (homeFocus == SettingsPage::kHomeIndex) {
    return {settingsPage.menuItems(), settingsPage.menuItemCount()};
  }

  const size_t count = sizeof(kSectionContents) / sizeof(kSectionContents[0]);
  const size_t index = (static_cast<size_t>(homeFocus) + count - 1U) % count;
  return kSectionContents[index];
}

uint8_t sectionPageCount(uint8_t itemCount) {
  if (itemCount == 0) {
    return 1;
  }
  return static_cast<uint8_t>((itemCount + kSectionPageSize - 1) / kSectionPageSize);
}

uint8_t sectionPageStart(uint8_t itemCount, uint8_t selectedIndex) {
  if (itemCount == 0) {
    return 0;
  }
  const uint8_t selected = static_cast<uint8_t>(selectedIndex % itemCount);
  return static_cast<uint8_t>((selected / kSectionPageSize) * kSectionPageSize);
}

uint8_t sectionVisibleCount(uint8_t itemCount, uint8_t pageStart) {
  if (itemCount == 0 || pageStart >= itemCount) {
    return 0;
  }
  const uint8_t left = static_cast<uint8_t>(itemCount - pageStart);
  return left > kSectionPageSize ? kSectionPageSize : left;
}

struct SectionListLayout {
  int16_t listX;
  int16_t listStartBaseline;
  int16_t rowStep;
  int16_t textHeight;
  int16_t focusPadX;
  int16_t focusPadY;
  int16_t focusRadius;
  int16_t boxMaxRight;
  int16_t iconX;
  int16_t iconY;
  int16_t iconSize;
};

struct SectionRowLayout {
  int16_t baselineY;
  int16_t boxLeft;
  int16_t boxTop;
  int16_t boxRight;
  int16_t boxBottom;
};

SectionListLayout makeSectionListLayout(int16_t xOffset, int16_t yOffset, int16_t width,
                                        int16_t height) {
  SectionListLayout layout;
  layout.iconSize = static_cast<int16_t>(height - 18);
  if (layout.iconSize > 150) {
    layout.iconSize = 150;
  }
  if (layout.iconSize < 96) {
    layout.iconSize = static_cast<int16_t>(height - 12);
  }
  layout.iconX = static_cast<int16_t>(xOffset + width - layout.iconSize - 8);
  layout.iconY = static_cast<int16_t>(yOffset + (height - layout.iconSize) / 2);

  layout.listX = static_cast<int16_t>(xOffset + 14);
  layout.listStartBaseline = static_cast<int16_t>(yOffset + 34);
  layout.rowStep = 26;
  layout.textHeight = 14;
  layout.focusPadX = 10;
  layout.focusPadY = 5;
  layout.focusRadius = 8;
  layout.boxMaxRight = static_cast<int16_t>(layout.iconX - 10);
  return layout;
}

SectionRowLayout makeSectionRowLayout(const SectionListLayout& layout, int16_t labelW,
                                      uint8_t rowIndex) {
  SectionRowLayout row;
  row.baselineY = static_cast<int16_t>(layout.listStartBaseline + layout.rowStep * rowIndex);
  row.boxTop = static_cast<int16_t>(row.baselineY - layout.textHeight - layout.focusPadY);
  row.boxBottom = static_cast<int16_t>(row.baselineY + layout.focusPadY);
  row.boxLeft = static_cast<int16_t>(layout.listX - layout.focusPadX);
  row.boxRight = static_cast<int16_t>(layout.listX + labelW + layout.focusPadX - 1);
  if (row.boxRight > layout.boxMaxRight) {
    row.boxRight = layout.boxMaxRight;
  }
  return row;
}

void applySectionItemTextStyle(U8G2_FOR_ST73XX& text, bool selected) {
  if (selected) {
    text.setBackgroundColor(ST7305_COLOR_BLACK);
    text.setFontMode(0);
    text.setForegroundColor(ST7305_COLOR_WHITE);
    return;
  }

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
}

void drawFilledRoundRect(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, int16_t radius, uint16_t color) {
  if (x1 > x2) {
    const int16_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    const int16_t t = y1;
    y1 = y2;
    y2 = t;
  }

  const int16_t w = static_cast<int16_t>(x2 - x1 + 1);
  const int16_t h = static_cast<int16_t>(y2 - y1 + 1);
  if (w <= 0 || h <= 0) {
    return;
  }

  int16_t r = radius;
  if (r < 0) {
    r = 0;
  }
  if (r > w / 2) {
    r = static_cast<int16_t>(w / 2);
  }
  if (r > h / 2) {
    r = static_cast<int16_t>(h / 2);
  }

  if (r == 0) {
    canvas.drawFilledRectangle(x1, y1, x2, y2, color);
    return;
  }

  canvas.drawFilledRectangle(static_cast<uint>(x1 + r), static_cast<uint>(y1),
                             static_cast<uint>(x2 - r), static_cast<uint>(y2), color);
  canvas.drawFilledRectangle(static_cast<uint>(x1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x1 + r - 1), static_cast<uint>(y2 - r), color);
  canvas.drawFilledRectangle(static_cast<uint>(x2 - r + 1), static_cast<uint>(y1 + r),
                             static_cast<uint>(x2), static_cast<uint>(y2 - r), color);

  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y1 + r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x1 + r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
  canvas.drawFilledCircle(static_cast<int>(x2 - r), static_cast<int>(y2 - r),
                          static_cast<uint>(r), color);
}
}  // namespace

bool UiManager::begin() {
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
  state_ = UiState::Home;
  lastActivityMs_ = millis();
  leftLongReported_ = false;
  transitionStartMs_ = millis();
  sectionFocusIndex_ = 0;
  sectionAnimFromIndex_ = 0;
  sectionAnimToIndex_ = 0;
  sectionAnimStartMs_ = transitionStartMs_;
  lastSectionIconFrame_ = 0;
  lastSectionInteractionMs_ = transitionStartMs_;
  sectionAnimationTimeMs_ = transitionStartMs_;
  popupKind_ = SettingsPage::PopupKind::RestartConfirm;
  popupSelectPrimary_ = false;
  lastPopupFrame_ = 0;
  lastRenderMs_ = 0;
  sectionAnimActive_ = false;
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
  musicService_.tick(nowMs);
  if (musicService_.consumeChanged() && !staticReaderDetail) {
    needsRedraw_ = true;
  }
  iicScanService_.tick(nowMs);
  if (iicScanService_.consumeChanged()) {
    needsRedraw_ = true;
  }
  const InputEdges edges = pollInputEdges();
  updateState(edges, nowMs);

  if (!shouldRedraw(nowMs)) {
    return;
  }

  render(nowMs);
  lastRenderMs_ = nowMs;
  needsRedraw_ = false;
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
      const bool enterSection =
          homePage_.handleInput(edges.up, edges.down, edges.ok, nowMs);
      homePage_.update(nowMs);
      if (enterSection) {
        sectionFocusIndex_ = 0;
        sectionAnimFromIndex_ = 0;
        sectionAnimToIndex_ = 0;
        sectionAnimStartMs_ = nowMs;
        sectionAnimActive_ = false;
        state_ = homePage_.focusIndex() == FocusClockPage::kHomeIndex
                     ? UiState::ToDirectDetailTransition
                     : UiState::ToSectionTransition;
        transitionStartMs_ = nowMs;
        needsRedraw_ = true;
      }
      if (isSleepAllowed() && (nowMs - lastActivityMs_) >= kAutoSleepTimeoutMs) {
        enterSleep(nowMs);
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToSectionTransition: {
      if (nowMs - transitionStartMs_ >= kSectionTransitionMs) {
        state_ = UiState::Section;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToDirectDetailTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        state_ = UiState::Detail;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToHomeTransition: {
      if (nowMs - transitionStartMs_ >= kSectionTransitionMs) {
        state_ = UiState::Home;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToHomeFromDirectDetailTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        state_ = UiState::Home;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToDetailTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        if (settingsPage_.isAboutDeviceSelection(homePage_.focusIndex(), sectionFocusIndex_)) {
          refreshSdStatus();
        }
        if (settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(),
                                                    sectionFocusIndex_)) {
          deviceSelfTestPage_.reset();
        }
        musicPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_, musicService_,
                                     sdCardService_);
        readerPage_.handleDetailEnter(homePage_.focusIndex(), sectionFocusIndex_);
        state_ = UiState::Detail;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::ToSectionFromDetailTransition: {
      if (nowMs - transitionStartMs_ >= kDetailTransitionMs) {
        state_ = UiState::Section;
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Section: {
      const SectionContent content = sectionContentFor(homePage_.focusIndex(), settingsPage_);
      if (sectionAnimActive_ || isSectionAnimationActive(nowMs)) {
        sectionAnimationTimeMs_ = nowMs;
      }
      if (sectionAnimActive_ && nowMs - sectionAnimStartMs_ >= kSectionFocusSlideMs) {
        sectionAnimActive_ = false;
        needsRedraw_ = true;
      }

      if (content.itemCount > 0 && edges.up) {
        lastSectionInteractionMs_ = nowMs;
        const uint8_t next = (sectionFocusIndex_ == 0)
                                 ? static_cast<uint8_t>(content.itemCount - 1)
                                 : static_cast<uint8_t>(sectionFocusIndex_ - 1);
        startSectionFocusAnimation(next, nowMs);
        needsRedraw_ = true;
      } else if (content.itemCount > 0 && edges.down) {
        lastSectionInteractionMs_ = nowMs;
        const uint8_t next =
            static_cast<uint8_t>((sectionFocusIndex_ + 1) % content.itemCount);
        startSectionFocusAnimation(next, nowMs);
        needsRedraw_ = true;
      } else if (edges.left) {
        lastSectionInteractionMs_ = nowMs;
        sectionAnimActive_ = false;
        remotePage_.handleSectionExit(homePage_.focusIndex(), bluetoothService_);
        state_ = UiState::ToHomeTransition;
        transitionStartMs_ = nowMs;
        needsRedraw_ = true;
      } else if (edges.ok) {
        lastSectionInteractionMs_ = nowMs;
        sectionAnimActive_ = false;
        popupKind_ = settingsPage_.popupForSelection(homePage_.focusIndex(), sectionFocusIndex_);
        if (popupKind_ != SettingsPage::PopupKind::None) {
          state_ = UiState::Popup;
          popupSelectPrimary_ = false;  // default "否"
        } else {
          detailPageIndex_ = 0;
          musicPage_.resetState(homePage_.focusIndex(), sectionFocusIndex_);
          state_ = UiState::ToDetailTransition;
          transitionStartMs_ = nowMs;
        }
        needsRedraw_ = true;
      }
      break;
    }

    case UiState::Popup: {
      if (edges.left || edges.up) {
        popupSelectPrimary_ = true;
        needsRedraw_ = true;
      } else if (edges.right || edges.down) {
        popupSelectPrimary_ = false;
        needsRedraw_ = true;
      } else if (edges.ok) {
        if (popupKind_ == SettingsPage::PopupKind::RestartConfirm) {
          if (popupSelectPrimary_) {
            ESP.restart();
          } else {
            state_ = UiState::Section;
            needsRedraw_ = true;
          }
        }
      }
      break;
    }

    case UiState::Detail: {
      const bool isSelfTest = settingsPage_.isDeviceSelfTestSelection(
          homePage_.focusIndex(), sectionFocusIndex_);
      const DeviceSelfTestPage::ButtonState selfTestButtons = {
          buttons_[0].stablePressed, buttons_[1].stablePressed, buttons_[2].stablePressed,
          buttons_[3].stablePressed, buttons_[4].stablePressed};
      if (isSelfTest &&
          deviceSelfTestPage_.handleInput(edges.left, edges.leftLong, edges.up, edges.down,
                                          edges.ok, selfTestButtons, nowMs, iicScanService_,
                                          rtcTestService_, sdCardService_, imuTestService_,
                                          powerDiagnosticService_, peripheralPower_)) {
        needsRedraw_ = true;
      } else if (edges.leftLong) {
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
                                                readerService_)) {
          needsRedraw_ = true;
        } else {
          settingsPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                          wifiProvisionService_, otaService_, timeService_);
          state_ = homePage_.focusIndex() == FocusClockPage::kHomeIndex
                       ? UiState::ToHomeFromDirectDetailTransition
                       : UiState::ToSectionFromDetailTransition;
          transitionStartMs_ = nowMs;
          needsRedraw_ = true;
        }
      } else if (musicPage_.isMusicListSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
          musicPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                       edges.left, edges.right, edges.up, edges.down, edges.ok, nowMs,
                                       musicService_)) {
        needsRedraw_ = true;
      } else if (edges.left) {
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
                                                edges.ok, readerService_, sdCardService_)) {
          needsRedraw_ = true;
        } else if (readerPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                                readerService_)) {
          needsRedraw_ = true;
        } else {
          settingsPage_.handleDetailBack(homePage_.focusIndex(), sectionFocusIndex_,
                                         wifiProvisionService_, otaService_, timeService_);
          state_ = homePage_.focusIndex() == FocusClockPage::kHomeIndex
                       ? UiState::ToHomeFromDirectDetailTransition
                       : UiState::ToSectionFromDetailTransition;
          transitionStartMs_ = nowMs;
          needsRedraw_ = true;
        }
      } else if (musicPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              false, edges.right, edges.up, edges.down, edges.ok, nowMs,
                                              musicService_)) {
        needsRedraw_ = true;
      } else if (focusClockPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                                   false, edges.right, edges.up, edges.down,
                                                   edges.ok, nowMs)) {
        needsRedraw_ = true;
      } else if (gamesPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              edges.ok, edges.okPressed, edges.okChanged,
                                              nowMs)) {
        needsRedraw_ = true;
        } else if (remotePage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                               edges.ok, nowMs, bluetoothService_,
                                               remoteService_)) {
          needsRedraw_ = true;
      } else if (timeCalibrationPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                              edges.up, edges.down, edges.ok, nowMs, timeService_,
                                              wifiProvisionService_)) {
        needsRedraw_ = true;
      } else if (readerPage_.handleDetailInput(homePage_.focusIndex(), sectionFocusIndex_,
                                               false, edges.right, edges.up, edges.down,
                                               edges.ok, readerService_, sdCardService_)) {
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
  const uint32_t frameIntervalMs = targetFrameIntervalMs(nowMs);
  if (frameIntervalMs > 0 && !needsRedraw_ && (nowMs - lastRenderMs_) < frameIntervalMs) {
    return false;
  }

  if (needsRedraw_) {
    return true;
  }

  if (state_ == UiState::Home) {
    if (homePage_.isSliding()) {
      return true;
    }
    if (homePage_.hasAnimationTick(nowMs)) {
      return true;
    }
  }

  if (state_ == UiState::ToSectionTransition) {
    return true;
  }
  if (state_ == UiState::ToDirectDetailTransition) {
    return true;
  }
  if (state_ == UiState::ToHomeTransition) {
    return true;
  }
  if (state_ == UiState::ToHomeFromDirectDetailTransition) {
    return true;
  }
  if (state_ == UiState::ToDetailTransition) {
    return true;
  }
  if (state_ == UiState::ToSectionFromDetailTransition) {
    return true;
  }
  if (state_ == UiState::Section) {
    if (sectionAnimActive_) {
      return true;
    }

    const SectionContent content = sectionContentFor(homePage_.focusIndex(), settingsPage_);
    if (content.itemCount > 0 && isSectionAnimationActive(nowMs)) {
      const uint8_t selected = static_cast<uint8_t>(sectionFocusIndex_ % content.itemCount);
      const uint16_t frame =
          IconBitmap::frameAt(content.items[selected].icon, sectionAnimationRenderTime(nowMs));
      if (frame != lastSectionIconFrame_) {
        return true;
      }
    }
  }
  if (state_ == UiState::Popup) {
    const uint16_t frame = IconBitmap::frameAt(kRestartPopupWindow, nowMs);
    if (frame != lastPopupFrame_) {
      return true;
    }
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
      settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
      deviceSelfTestPage_.needsAnimationFrame()) {
    return true;
  }

  return false;
}

uint32_t UiManager::targetFrameIntervalMs(uint32_t nowMs) const {
  if (state_ == UiState::ToSectionTransition ||
      state_ == UiState::ToDirectDetailTransition || state_ == UiState::ToHomeTransition ||
      state_ == UiState::ToHomeFromDirectDetailTransition ||
      state_ == UiState::ToDetailTransition ||
      state_ == UiState::ToSectionFromDetailTransition) {
    return kHighFrameIntervalMs;
  }

  if (state_ == UiState::Home && homePage_.isSliding()) {
    return kHighFrameIntervalMs;
  }

  if (state_ == UiState::Section && sectionAnimActive_) {
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
      settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_) &&
      deviceSelfTestPage_.needsAnimationFrame()) {
    return 0;
  }

  (void)nowMs;
  return 0;
}

void UiManager::render(uint32_t nowMs) {
  const bool navOnlyMusicFrame =
      state_ == UiState::Detail && !needsRedraw_ &&
      musicPage_.needsNavAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs);
  const bool listOnlyMusicFrame =
      state_ == UiState::Detail && !needsRedraw_ && !navOnlyMusicFrame &&
      musicPage_.needsListAnimationFrame(homePage_.focusIndex(), sectionFocusIndex_, nowMs);

  renderer_.beginFrame();
  if (navOnlyMusicFrame) {
    renderer_.markDirty(0, 112, static_cast<int16_t>(display_.width() - 1),
                        static_cast<int16_t>(display_.height() - 1));
  } else if (listOnlyMusicFrame) {
    renderer_.markDirty(0, 0, static_cast<int16_t>(display_.width() - 1), 111);
  } else {
    renderer_.markDirty(0, 0, static_cast<int16_t>(display_.width() - 1),
                        static_cast<int16_t>(display_.height() - 1));
  }

  if (navOnlyMusicFrame) {
    if (musicPage_.renderDetailNavOnly(homePage_.focusIndex(), sectionFocusIndex_, 0, display_,
                                       kUiLanguage, nowMs)) {
      if (renderer_.hasDirty()) {
        display_.present();
      }
      return;
    }
  }

  if (listOnlyMusicFrame) {
    if (musicPage_.renderDetailListOnly(homePage_.focusIndex(), sectionFocusIndex_, display_,
                                        kUiLanguage, musicService_, nowMs)) {
      if (renderer_.hasDirty()) {
        display_.present();
      }
      return;
    }
  }

  display_.clear();

  if (state_ == UiState::Home) {
    homePage_.render(display_, 0, nowMs);
  } else if (state_ == UiState::ToSectionTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kSectionTransitionMs ? kSectionTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());
    int16_t backgroundOffsetX = static_cast<int16_t>(-width);
    if (elapsed < kForwardBgEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kForwardBgEndMs == 0 ? 1 : kForwardBgEndMs);
      backgroundOffsetX = easeInCubic(0, static_cast<int16_t>(-width), t);
    }

    const uint32_t menuWindow = kForwardMenuEndMs - kForwardMenuStartMs;
    const uint32_t menuStep = menuWindow / 3U;
    const uint32_t menuStartUp = kForwardMenuStartMs;
    const uint32_t menuStartFocus = kForwardMenuStartMs + menuStep;
    const uint32_t menuStartDown = kForwardMenuStartMs + (menuStep * 2U);
    const uint32_t menuDur = menuStep;

    int16_t menuUpOffsetX = static_cast<int16_t>(-width);
    int16_t menuFocusOffsetX = static_cast<int16_t>(-width);
    int16_t menuDownOffsetX = static_cast<int16_t>(-width);

    auto forwardMenuOffset = [&](uint32_t startMs) -> int16_t {
      if (elapsed <= startMs) {
        return 0;
      }
      const uint32_t endMs = startMs + menuDur;
      if (elapsed >= endMs) {
        return static_cast<int16_t>(-width);
      }
      const float t = static_cast<float>(elapsed - startMs) /
                      static_cast<float>(menuDur == 0 ? 1 : menuDur);
      return easeInCubic(0, static_cast<int16_t>(-width), t);
    };

    menuUpOffsetX = forwardMenuOffset(menuStartUp);
    menuFocusOffsetX = forwardMenuOffset(menuStartFocus);
    menuDownOffsetX = forwardMenuOffset(menuStartDown);

    int16_t sectionOffsetY = height;
    if (elapsed >= kForwardSectionStartMs) {
      const uint32_t moveDuration = kSectionTransitionMs - kForwardSectionStartMs;
      const float t = static_cast<float>(elapsed - kForwardSectionStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      sectionOffsetY = easeOutCubic(height, 0, t);
    }

    homePage_.renderTransition(display_, backgroundOffsetX, 0, menuUpOffsetX,
                               menuFocusOffsetX, menuDownOffsetX, nowMs);
    renderSection(0, sectionOffsetY, nowMs);
  } else if (state_ == UiState::ToDirectDetailTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());

    int16_t homeOffsetX = static_cast<int16_t>(-width);
    if (elapsed < kDetailForwardIconEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kDetailForwardIconEndMs == 0 ? 1
                                                                      : kDetailForwardIconEndMs);
      homeOffsetX = easeInCubic(0, static_cast<int16_t>(-width), t);
    }

    int16_t detailOffsetY = height;
    if (elapsed >= kDetailForwardDetailStartMs) {
      const uint32_t moveDuration = kDetailTransitionMs - kDetailForwardDetailStartMs;
      const float t = static_cast<float>(elapsed - kDetailForwardDetailStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      detailOffsetY = easeOutCubic(height, 0, t);
    }

    homePage_.renderTransition(display_, homeOffsetX, homeOffsetX, 0, 0, 0, nowMs);
    renderDetail(detailOffsetY);
  } else if (state_ == UiState::ToHomeTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kSectionTransitionMs ? kSectionTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());
    int16_t sectionOffsetY = height;
    if (elapsed < kBackSectionEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kBackSectionEndMs == 0 ? 1 : kBackSectionEndMs);
      sectionOffsetY = easeInCubic(0, height, t);
    }

    const uint32_t menuWindow = kBackMenuEndMs - kBackMenuStartMs;
    const uint32_t menuStep = menuWindow / 3U;
    const uint32_t menuStartDown = kBackMenuStartMs;
    const uint32_t menuStartFocus = kBackMenuStartMs + menuStep;
    const uint32_t menuStartUp = kBackMenuStartMs + (menuStep * 2U);
    const uint32_t menuDur = menuStep;

    int16_t menuUpOffsetX = static_cast<int16_t>(-width);
    int16_t menuFocusOffsetX = static_cast<int16_t>(-width);
    int16_t menuDownOffsetX = static_cast<int16_t>(-width);

    auto backwardMenuOffset = [&](uint32_t startMs) -> int16_t {
      if (elapsed <= startMs) {
        return static_cast<int16_t>(-width);
      }
      const uint32_t endMs = startMs + menuDur;
      if (elapsed >= endMs) {
        return 0;
      }
      const float t = static_cast<float>(elapsed - startMs) /
                      static_cast<float>(menuDur == 0 ? 1 : menuDur);
      return easeOutCubic(static_cast<int16_t>(-width), 0, t);
    };

    menuDownOffsetX = backwardMenuOffset(menuStartDown);
    menuFocusOffsetX = backwardMenuOffset(menuStartFocus);
    menuUpOffsetX = backwardMenuOffset(menuStartUp);

    int16_t backgroundOffsetX = static_cast<int16_t>(-width);
    if (elapsed >= kBackBgStartMs) {
      const uint32_t moveDuration = kSectionTransitionMs - kBackBgStartMs;
      const float t = static_cast<float>(elapsed - kBackBgStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      backgroundOffsetX = easeOutCubic(static_cast<int16_t>(-width), 0, t);
    }

    homePage_.renderTransition(display_, backgroundOffsetX, 0, menuUpOffsetX,
                               menuFocusOffsetX, menuDownOffsetX, nowMs);
    renderSection(0, sectionOffsetY, nowMs);
  } else if (state_ == UiState::ToHomeFromDirectDetailTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());

    int16_t detailOffsetY = height;
    if (elapsed < kDetailBackDetailEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kDetailBackDetailEndMs == 0 ? 1
                                                                      : kDetailBackDetailEndMs);
      detailOffsetY = easeInCubic(0, height, t);
    }

    int16_t homeOffsetX = static_cast<int16_t>(-width);
    if (elapsed >= kDetailBackIconStartMs) {
      const uint32_t moveDuration = kDetailTransitionMs - kDetailBackIconStartMs;
      const float t = static_cast<float>(elapsed - kDetailBackIconStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      homeOffsetX = easeOutCubic(static_cast<int16_t>(-width), 0, t);
    }

    renderDetail(detailOffsetY);
    homePage_.renderTransition(display_, homeOffsetX, homeOffsetX, 0, 0, 0, nowMs);
  } else if (state_ == UiState::ToDetailTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());

    int16_t iconOffsetX = width;
    if (elapsed < kDetailForwardIconEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kDetailForwardIconEndMs == 0 ? 1
                                                                      : kDetailForwardIconEndMs);
      iconOffsetX = easeInCubic(0, width, t);
    }

    const SectionContent content = sectionContentFor(homePage_.focusIndex(), settingsPage_);
    const uint8_t selected =
        content.itemCount == 0 ? 0 : static_cast<uint8_t>(sectionFocusIndex_ % content.itemCount);
    const uint8_t pageStart = sectionPageStart(content.itemCount, selected);
    const uint8_t rowCount = sectionVisibleCount(content.itemCount, pageStart);
    int16_t rowOffsets[kMaxSectionItemsForAnim] = {0};
    if (rowCount > 0) {
      const uint32_t rowWindow = kDetailForwardRowsEndMs - kDetailForwardRowsStartMs;
      uint32_t rowStep = rowWindow / rowCount;
      if (rowStep == 0) {
        rowStep = 1;
      }
      const uint32_t rowDur = rowStep;

      for (uint8_t i = 0; i < rowCount; ++i) {
        const uint32_t startMs = kDetailForwardRowsStartMs + rowStep * i;
        if (elapsed <= startMs) {
          rowOffsets[i] = 0;
          continue;
        }
        const uint32_t endMs = startMs + rowDur;
        if (elapsed >= endMs) {
          rowOffsets[i] = width;
          continue;
        }
        const float t = static_cast<float>(elapsed - startMs) /
                        static_cast<float>(rowDur == 0 ? 1 : rowDur);
        rowOffsets[i] = easeInCubic(0, width, t);
      }
    }
    const uint8_t selectedRow = static_cast<uint8_t>(selected - pageStart);
    const int16_t focusOffsetX = (selectedRow < rowCount) ? rowOffsets[selectedRow] : 0;

    int16_t detailOffsetY = height;
    if (elapsed >= kDetailForwardDetailStartMs) {
      const uint32_t moveDuration = kDetailTransitionMs - kDetailForwardDetailStartMs;
      const float t = static_cast<float>(elapsed - kDetailForwardDetailStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      detailOffsetY = easeOutCubic(height, 0, t);
    }

    renderSection(0, 0, nowMs, iconOffsetX, rowOffsets, rowCount, focusOffsetX);
    renderDetail(detailOffsetY);
  } else if (state_ == UiState::ToSectionFromDetailTransition) {
    const uint32_t elapsedRaw = nowMs - transitionStartMs_;
    const uint32_t elapsed =
        elapsedRaw > kDetailTransitionMs ? kDetailTransitionMs : elapsedRaw;
    const int16_t width = static_cast<int16_t>(display_.width());
    const int16_t height = static_cast<int16_t>(display_.height());

    int16_t detailOffsetY = height;
    if (elapsed < kDetailBackDetailEndMs) {
      const float t = static_cast<float>(elapsed) /
                      static_cast<float>(kDetailBackDetailEndMs == 0 ? 1 : kDetailBackDetailEndMs);
      detailOffsetY = easeInCubic(0, height, t);
    }

    const SectionContent content = sectionContentFor(homePage_.focusIndex(), settingsPage_);
    const uint8_t selected =
        content.itemCount == 0 ? 0 : static_cast<uint8_t>(sectionFocusIndex_ % content.itemCount);
    const uint8_t pageStart = sectionPageStart(content.itemCount, selected);
    const uint8_t rowCount = sectionVisibleCount(content.itemCount, pageStart);
    int16_t rowOffsets[kMaxSectionItemsForAnim] = {0};
    if (rowCount > 0) {
      const uint32_t rowWindow = kDetailBackRowsEndMs - kDetailBackRowsStartMs;
      uint32_t rowStep = rowWindow / rowCount;
      if (rowStep == 0) {
        rowStep = 1;
      }
      const uint32_t rowDur = rowStep;

      for (uint8_t i = 0; i < rowCount; ++i) {
        const uint32_t order = static_cast<uint32_t>(rowCount - 1U - i);
        const uint32_t startMs = kDetailBackRowsStartMs + rowStep * order;
        if (elapsed <= startMs) {
          rowOffsets[i] = width;
          continue;
        }
        const uint32_t endMs = startMs + rowDur;
        if (elapsed >= endMs) {
          rowOffsets[i] = 0;
          continue;
        }
        const float t = static_cast<float>(elapsed - startMs) /
                        static_cast<float>(rowDur == 0 ? 1 : rowDur);
        rowOffsets[i] = easeOutCubic(width, 0, t);
      }
    }
    const uint8_t selectedRow = static_cast<uint8_t>(selected - pageStart);
    const int16_t focusOffsetX = (selectedRow < rowCount) ? rowOffsets[selectedRow] : 0;

    int16_t iconOffsetX = width;
    if (elapsed >= kDetailBackIconStartMs) {
      const uint32_t moveDuration = kDetailTransitionMs - kDetailBackIconStartMs;
      const float t = static_cast<float>(elapsed - kDetailBackIconStartMs) /
                      static_cast<float>(moveDuration == 0 ? 1 : moveDuration);
      iconOffsetX = easeOutCubic(width, 0, t);
    }

    renderSection(0, 0, nowMs, iconOffsetX, rowOffsets, rowCount, focusOffsetX);
    renderDetail(detailOffsetY);
  } else if (state_ == UiState::Section) {
    renderSection(0, 0, nowMs);
  } else if (state_ == UiState::Popup) {
    renderSection(0, 0, nowMs);
    renderPopup(nowMs);
  } else {
    renderDetail();
  }

  if (renderer_.hasDirty()) {
    display_.present();
  }
}

void UiManager::startSectionFocusAnimation(uint8_t toIndex, uint32_t nowMs) {
  const SectionContent content = sectionContentFor(homePage_.focusIndex(), settingsPage_);
  if (content.itemCount == 0) {
    sectionAnimActive_ = false;
    sectionFocusIndex_ = 0;
    return;
  }

  if (sectionAnimActive_) {
    sectionFocusIndex_ = sectionAnimToIndex_;
  }

  const uint8_t normalizedFrom = static_cast<uint8_t>(sectionFocusIndex_ % content.itemCount);
  const uint8_t normalizedTo = static_cast<uint8_t>(toIndex % content.itemCount);
  const uint8_t fromPage = sectionPageStart(content.itemCount, normalizedFrom);
  const uint8_t toPage = sectionPageStart(content.itemCount, normalizedTo);

  sectionAnimFromIndex_ = normalizedFrom;
  sectionAnimToIndex_ = normalizedTo;
  sectionFocusIndex_ = normalizedTo;
  sectionAnimStartMs_ = nowMs;
  sectionAnimActive_ =
      (sectionAnimFromIndex_ != sectionAnimToIndex_) && (fromPage == toPage);
}

void UiManager::renderSection(int16_t xOffset, int16_t yOffset, uint32_t nowMs,
                              int16_t iconExtraOffsetX, const int16_t* rowExtraOffsets,
                              uint8_t rowExtraCount, int16_t focusBoxExtraOffsetX) {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());
  const uint8_t focus = homePage_.focusIndex();
  const SectionContent content = sectionContentFor(focus, settingsPage_);
  const uint8_t itemCount = content.itemCount;
  const SectionListLayout layout = makeSectionListLayout(xOffset, yOffset, width, height);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  if (itemCount == 0) {
    return;
  }

  const uint8_t selected = static_cast<uint8_t>(sectionFocusIndex_ % itemCount);
  const uint8_t pageStart = sectionPageStart(itemCount, selected);
  const uint8_t visibleCount = sectionVisibleCount(itemCount, pageStart);
  const uint8_t selectedRow = static_cast<uint8_t>(selected - pageStart);
  const uint8_t fromIndex = static_cast<uint8_t>(sectionAnimFromIndex_ % itemCount);
  const uint8_t toIndex = static_cast<uint8_t>(sectionAnimToIndex_ % itemCount);
  const uint8_t fromRow = sectionAnimActive_ ? static_cast<uint8_t>(fromIndex - pageStart)
                                             : selectedRow;
  const uint8_t toRow = sectionAnimActive_ ? static_cast<uint8_t>(toIndex - pageStart)
                                           : selectedRow;

  const int16_t fromLabelW =
      text.getUTF8Width(content.items[fromIndex].labelZh);
  const int16_t toLabelW =
      text.getUTF8Width(content.items[toIndex].labelZh);
  const SectionRowLayout fromLayout = makeSectionRowLayout(layout, fromLabelW, fromRow);
  const SectionRowLayout toLayout = makeSectionRowLayout(layout, toLabelW, toRow);

  int16_t boxLeft = toLayout.boxLeft;
  int16_t boxTop = toLayout.boxTop;
  int16_t boxRight = toLayout.boxRight;
  int16_t boxBottom = toLayout.boxBottom;
  float sectionAnimT = 1.0f;

  if (sectionAnimActive_) {
    const uint32_t elapsed = nowMs - sectionAnimStartMs_;
    sectionAnimT = elapsed >= kSectionFocusSlideMs
                       ? 1.0f
                       : static_cast<float>(elapsed) /
                             static_cast<float>(kSectionFocusSlideMs == 0 ? 1
                                                                          : kSectionFocusSlideMs);
    boxLeft = easeOutCubic(fromLayout.boxLeft, toLayout.boxLeft, sectionAnimT);
    boxTop = easeOutCubic(fromLayout.boxTop, toLayout.boxTop, sectionAnimT);
    boxRight = easeOutCubic(fromLayout.boxRight, toLayout.boxRight, sectionAnimT);
    boxBottom = easeOutCubic(fromLayout.boxBottom, toLayout.boxBottom, sectionAnimT);
  }

  boxLeft = static_cast<int16_t>(boxLeft + focusBoxExtraOffsetX);
  boxRight = static_cast<int16_t>(boxRight + focusBoxExtraOffsetX);

  drawFilledRoundRect(canvas, boxLeft, boxTop, boxRight, boxBottom, layout.focusRadius,
                      ST7305_COLOR_BLACK);

  for (uint8_t row = 0; row < visibleCount; ++row) {
    const uint8_t itemIndex = static_cast<uint8_t>(pageStart + row);
    const char* label = content.items[itemIndex].labelZh;
    const int16_t labelW = text.getUTF8Width(label);
    const SectionRowLayout rowLayout = makeSectionRowLayout(layout, labelW, row);
    const bool isSelected = (itemIndex == selected);
    int16_t rowExtraOffsetX = 0;
    if (rowExtraOffsets != nullptr && row < rowExtraCount) {
      rowExtraOffsetX = rowExtraOffsets[row];
    }

    applySectionItemTextStyle(text, isSelected);
    text.drawUTF8(static_cast<int16_t>(layout.listX + rowExtraOffsetX), rowLayout.baselineY,
                  label);
  }

  const uint8_t pages = sectionPageCount(itemCount);
  const uint8_t currentPage = static_cast<uint8_t>(pageStart / kSectionPageSize);
  const int16_t progressX = static_cast<int16_t>(layout.iconX - 14);
  const int16_t progressY = static_cast<int16_t>(yOffset + 18);
  const int16_t progressW = 4;
  const int16_t progressH = static_cast<int16_t>(height - 36);
  canvas.drawRectangle(progressX, progressY, static_cast<int16_t>(progressX + progressW - 1),
                       static_cast<int16_t>(progressY + progressH - 1), ST7305_COLOR_BLACK);
  int16_t thumbH = progressH;
  if (itemCount > 0) {
    thumbH = static_cast<int16_t>(progressH / itemCount);
  }
  if (thumbH < 4) {
    thumbH = 4;
  }
  if (thumbH > progressH) {
    thumbH = progressH;
  }
  int16_t thumbY = progressY;
  if (itemCount > 0 && progressH > thumbH) {
    const int16_t travel = static_cast<int16_t>(progressH - thumbH);
    thumbY = static_cast<int16_t>(
        progressY + (static_cast<int32_t>(travel) * selected) / itemCount);
  }
  canvas.drawFilledRectangle(progressX, thumbY, static_cast<int16_t>(progressX + progressW - 1),
                             static_cast<int16_t>(thumbY + thumbH - 1), ST7305_COLOR_BLACK);

  char pageText[8];
  snprintf(pageText, sizeof(pageText), "%u/%u", static_cast<unsigned>(currentPage + 1),
           static_cast<unsigned>(pages));
  const int16_t pageTextW = text.getUTF8Width(pageText);
  const int16_t pageBaselineY = static_cast<int16_t>(yOffset + height - 8);
  const int16_t pageCenterX = static_cast<int16_t>((layout.listX + progressX - 1) / 2);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(static_cast<int16_t>(pageCenterX - pageTextW / 2), pageBaselineY, pageText);

  const SectionItem& selectedItem = content.items[selected];
  const bool sectionTransitionActive =
      (xOffset != 0) || (yOffset != 0) || (iconExtraOffsetX != 0) ||
      (rowExtraOffsets != nullptr) || (focusBoxExtraOffsetX != 0);
  const uint32_t iconNowMs =
      (sectionAnimActive_ || sectionTransitionActive) ? nowMs : sectionAnimationRenderTime(nowMs);
  const uint16_t iconFrame = IconBitmap::frameAt(selectedItem.icon, iconNowMs);
  lastSectionIconFrame_ = iconFrame;
  const int16_t iconBaseX = static_cast<int16_t>(layout.iconX + iconExtraOffsetX);
  int16_t iconDrawX = iconBaseX;
  if (sectionAnimActive_) {
    const int16_t iconStartX = static_cast<int16_t>(iconBaseX + layout.iconSize + 16);
    iconDrawX = easeOutCubic(iconStartX, iconBaseX, sectionAnimT);
  }

  const int16_t iconPad = 4;
  int16_t borderX1 = static_cast<int16_t>(iconDrawX - iconPad);
  int16_t borderY1 = static_cast<int16_t>(layout.iconY - iconPad);
  int16_t borderX2 =
      static_cast<int16_t>(iconDrawX + layout.iconSize + iconPad - 1);
  int16_t borderY2 =
      static_cast<int16_t>(layout.iconY + layout.iconSize + iconPad - 1);
  const int16_t areaX1 = xOffset;
  const int16_t areaY1 = yOffset;
  const int16_t areaX2 = static_cast<int16_t>(xOffset + width - 1);
  const int16_t areaY2 = static_cast<int16_t>(yOffset + height - 1);
  if (!(borderX2 < areaX1 || borderY2 < areaY1 || borderX1 > areaX2 || borderY1 > areaY2)) {
    if (borderX1 < areaX1) borderX1 = areaX1;
    if (borderY1 < areaY1) borderY1 = areaY1;
    if (borderX2 > areaX2) borderX2 = areaX2;
    if (borderY2 > areaY2) borderY2 = areaY2;
    canvas.drawRectangle(static_cast<uint>(borderX1), static_cast<uint>(borderY1),
                         static_cast<uint>(borderX2), static_cast<uint>(borderY2),
                         ST7305_COLOR_BLACK);
  }
  IconBitmap::drawFrame(
      canvas, selectedItem.icon, iconFrame, iconDrawX, layout.iconY, layout.iconSize,
      layout.iconSize, false, yOffset, static_cast<int16_t>(yOffset + height - 1));

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
}

bool UiManager::isSectionAnimationActive(uint32_t nowMs) const {
  return (nowMs - lastSectionInteractionMs_) < kSectionIdleAnimationTimeoutMs;
}

uint32_t UiManager::sectionAnimationRenderTime(uint32_t nowMs) const {
  return isSectionAnimationActive(nowMs) ? nowMs : sectionAnimationTimeMs_;
}

void UiManager::renderTwoOptionPopup(const char* title, const char* primaryLabel,
                                     const char* secondaryLabel, uint32_t nowMs) {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());

  const int16_t popupW = static_cast<int16_t>(kRestartPopupWindow.frameWidth);
  const int16_t popupH = static_cast<int16_t>(kRestartPopupWindow.frameHeight);
  const int16_t popupX = static_cast<int16_t>((width - popupW) / 2);
  const int16_t popupY = static_cast<int16_t>((height - popupH) / 2);

  const uint16_t popupFrame = IconBitmap::frameAt(kRestartPopupWindow, nowMs);
  lastPopupFrame_ = popupFrame;
  IconBitmap::drawFrame(canvas, kRestartPopupWindow, popupFrame, popupX, popupY, popupW,
                        popupH, false, 0, static_cast<int16_t>(height - 1));

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  const int16_t titleW = text.getUTF8Width(title);
  const int16_t titleX = static_cast<int16_t>(popupX + (popupW - titleW) / 2);
  text.drawUTF8(titleX, static_cast<int16_t>(popupY + kRestartPopupTitleTopOffset), title);

  const int16_t optionGroupW =
      static_cast<int16_t>(kRestartPopupOptionWidth * 2 + kRestartPopupOptionGap);
  const int16_t optionStartX = static_cast<int16_t>(popupX + (popupW - optionGroupW) / 2);
  const int16_t primaryTextW = text.getUTF8Width(primaryLabel);
  const int16_t secondaryTextW = text.getUTF8Width(secondaryLabel);
  const int16_t textHeight = 12;          // chinese_font_all glyph height
  const int16_t textBaselineFromTop = 12; // chinese_font_all baseline
  const int16_t optionHeight =
      static_cast<int16_t>(textHeight + kRestartPopupOptionPadY * 2);
  const int16_t optionY =
      static_cast<int16_t>(popupY + popupH - optionHeight - kRestartPopupOptionBottomMargin);
  const int16_t optionTextBaselineY =
      static_cast<int16_t>(optionY + kRestartPopupOptionPadY + textBaselineFromTop);
  const int16_t yesX = optionStartX;
  const int16_t noX = static_cast<int16_t>(optionStartX + kRestartPopupOptionWidth +
                                           kRestartPopupOptionGap);

  drawFilledRoundRect(canvas, yesX, optionY,
                      static_cast<int16_t>(yesX + kRestartPopupOptionWidth - 1),
                      static_cast<int16_t>(optionY + optionHeight - 1), 5,
                      popupSelectPrimary_ ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  drawFilledRoundRect(canvas, noX, optionY,
                      static_cast<int16_t>(noX + kRestartPopupOptionWidth - 1),
                      static_cast<int16_t>(optionY + optionHeight - 1), 5,
                      popupSelectPrimary_ ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  canvas.drawRectangle(yesX, optionY,
                       static_cast<int16_t>(yesX + kRestartPopupOptionWidth - 1),
                       static_cast<int16_t>(optionY + optionHeight - 1),
                       ST7305_COLOR_BLACK);
  canvas.drawRectangle(noX, optionY,
                       static_cast<int16_t>(noX + kRestartPopupOptionWidth - 1),
                       static_cast<int16_t>(optionY + optionHeight - 1),
                       ST7305_COLOR_BLACK);

  applySectionItemTextStyle(text, popupSelectPrimary_);
  text.drawUTF8(static_cast<int16_t>(yesX + (kRestartPopupOptionWidth - primaryTextW) / 2),
                optionTextBaselineY, primaryLabel);

  applySectionItemTextStyle(text, !popupSelectPrimary_);
  text.drawUTF8(static_cast<int16_t>(noX + (kRestartPopupOptionWidth - secondaryTextW) / 2),
                optionTextBaselineY, secondaryLabel);

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

void UiManager::renderPopup(uint32_t nowMs) {
  if (popupKind_ == SettingsPage::PopupKind::None) {
    return;
  }

  renderTwoOptionPopup(settingsPage_.popupTitle(popupKind_),
                       settingsPage_.popupPrimaryLabel(popupKind_),
                       settingsPage_.popupSecondaryLabel(popupKind_), nowMs);
}

void UiManager::renderDetail(int16_t yOffset) {
  auto& canvas = display_.canvas();
  auto& text = display_.text();
  const int16_t width = static_cast<int16_t>(display_.width());
  const int16_t height = static_cast<int16_t>(display_.height());

  if (yOffset < height) {
    canvas.drawFilledRectangle(0, yOffset, width - 1, height - 1, ST7305_COLOR_WHITE);
  }

  if (settingsPage_.isDeviceSelfTestSelection(homePage_.focusIndex(), sectionFocusIndex_)) {
    deviceSelfTestPage_.render(display_, yOffset, millis(), iicScanService_, rtcTestService_,
                               sdCardService_, imuTestService_, powerDiagnosticService_);
    return;
  }

  if (gamesPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                              kUiLanguage)) {
    return;
  }

  if (focusClockPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                                   kUiLanguage, millis())) {
    return;
  }

  if (musicPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                              kUiLanguage, musicService_, millis())) {
    return;
  }

  if (readerPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                               kUiLanguage, readerService_)) {
    return;
  }

  if (remotePage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                               kUiLanguage, bluetoothService_, remoteService_)) {
    return;
  }

  if (timeCalibrationPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, yOffset, display_,
                              kUiLanguage, timeService_)) {
    return;
  }

  if (settingsPage_.renderDetail(homePage_.focusIndex(), sectionFocusIndex_, detailPageIndex_,
                                 yOffset, display_, kUiLanguage, deviceIdText_, flashTotalText_,
                                 sdStatusText_, wifiProvisionService_, otaService_, timeService_)) {
    return;
  }

  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + ThemeMono::kHeaderHeight),
                             ST7305_COLOR_BLACK);
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(6, static_cast<int16_t>(yOffset + 24), "详情页");

  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 62), homePage_.focusName());
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 92), "详情内容开发中");
  text.drawUTF8(width - 92, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回");

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
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

bool UiManager::isSleepAllowed() const {
  if (state_ != UiState::Home) {
    return false;
  }
  if (iicScanService_.isBusy() || rtcTestService_.isBusy() || imuTestService_.isBusy()) {
    return false;
  }
  if (bluetoothService_.state() != BluetoothService::State::Off) {
    return false;
  }
  if (wifiProvisionService_.isRadioActive()) {
    return false;
  }
  if (otaService_.state() != OtaService::State::Idle) {
    return false;
  }
  if (musicService_.playbackState() != MusicService::PlaybackState::Stopped) {
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
  sleepLog("enter home auto-sleep idle=%lums", static_cast<unsigned long>(idleMs));

  const bool rtcWakeReady = rtcDriver_.setMinuteInterruptEnabled(true);
  if (rtcWakeReady) {
    pinMode(BoardConfig::kPinPcfInt, INPUT_PULLUP);
    gpio_wakeup_enable(static_cast<gpio_num_t>(BoardConfig::kPinPcfInt),
                       GPIO_INTR_LOW_LEVEL);
    sleepLog("rtc minute interrupt wake enabled int_gpio=%d",
             static_cast<int>(BoardConfig::kPinPcfInt));
  } else {
    sleepLog("rtc minute interrupt unavailable; time refresh wake disabled");
  }

  display_.prepareForSleepKeepDisplay();
  peripheralPower_.setSensorEnabled(false);
  sdCardService_.end();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_OFF);
  sleepLog("power domains off: rtc_periph modem");

  bool keepSleeping = true;
  while (keepSleeping) {
    for (const auto& button : buttons_) {
      gpio_wakeup_enable(static_cast<gpio_num_t>(button.pin), GPIO_INTR_LOW_LEVEL);
    }
    if (rtcWakeReady) {
      gpio_wakeup_enable(static_cast<gpio_num_t>(BoardConfig::kPinPcfInt),
                         GPIO_INTR_LOW_LEVEL);
      (void)rtcDriver_.clearTimerFlag();
    }
    esp_sleep_enable_gpio_wakeup();

    const esp_err_t sleepErr = esp_light_sleep_start();
    const uint32_t wakeNowMs = millis();

    if (sleepErr != ESP_OK) {
      sleepLog("light sleep rejected err=%d", static_cast<int>(sleepErr));
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
      render(wakeNowMs);
      lastRenderMs_ = wakeNowMs;
      needsRedraw_ = false;
      (void)rtcDriver_.clearTimerFlag();
      display_.prepareForSleepKeepDisplay();
      peripheralPower_.setSensorEnabled(false);
      sleepLog("rtc minute wake at %lums; time refreshed, continue sleep",
               static_cast<unsigned long>(wakeNowMs));
      continue;
    }

    if (buttonPressed) {
      sleepLog("button wake; exit sleep");
    } else {
      sleepLog("unknown wake cause=%d; exit sleep",
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
  lastActivityMs_ = exitNowMs;
  lastRenderMs_ = exitNowMs;
  needsRedraw_ = true;
  sleepLog("exit home auto-sleep at %lums", static_cast<unsigned long>(exitNowMs));
}

bool UiManager::isPressed(uint8_t pin) const { return digitalRead(pin) == LOW; }

int16_t UiManager::easeInCubic(int16_t from, int16_t to, float t) const {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  const float eased = t * t * t;
  return static_cast<int16_t>(from + (to - from) * eased);
}

int16_t UiManager::easeOutCubic(int16_t from, int16_t to, float t) const {
  if (t <= 0.0f) {
    return from;
  }
  if (t >= 1.0f) {
    return to;
  }
  const float inv = 1.0f - t;
  const float eased = 1.0f - (inv * inv * inv);
  return static_cast<int16_t>(from + (to - from) * eased);
}
