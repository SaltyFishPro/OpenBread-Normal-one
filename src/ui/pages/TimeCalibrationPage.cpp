#include "TimeCalibrationPage.h"
#include "SettingsPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../DrawUtils.h"
#include "../SdUiText.h"
#include "../../services/TimeService.h"
#include "../../services/WifiProvisionService.h"

namespace {
constexpr uint16_t kBlack = ST7305_COLOR_BLACK;
constexpr uint16_t kWhite = ST7305_COLOR_WHITE;

void clockIcon(DisplayMonoTft& display, int16_t x, int16_t y, int16_t radius) {
  auto& canvas = display.canvas();
  canvas.drawCircle(x, y, radius, kWhite);
  canvas.drawLine(x, y, x, y - radius / 2, kWhite);
  canvas.drawLine(x, y, x + radius / 2, y + radius / 3, kWhite);
}

void label(DisplayMonoTft& display, SdBitmapFont* font, int16_t x, int16_t baseline,
           const char* value, int16_t width, uint16_t color = kWhite) {
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(1);
  text.setForegroundColor(color);
  text.setBackgroundColor(kBlack);
  const SdBitmapText::Clip clip = {x, static_cast<int16_t>(baseline - 28),
      static_cast<int16_t>(x + width - 1), static_cast<int16_t>(baseline + 5)};
  SdUiText::draw(display, font, x, baseline, value, color, clip);
}

const char* syncSourceTextZh(TimeService::SyncSource source) {
  switch (source) {
    case TimeService::SyncSource::Ntp:
      return "网络校时";
    default:
      return "未校准";
  }
}

const char* syncStateTextZh(TimeService::SyncState state) {
  switch (state) {
    case TimeService::SyncState::Syncing:
      return "正在校时";
    case TimeService::SyncState::Success:
      return "校时成功";
    case TimeService::SyncState::Failed:
      return "校时失败";
    default:
      return "空闲";
  }
}

const char* errorTextZh(TimeService::Error err) {
  switch (err) {
    case TimeService::Error::NoRtc:
      return "RTC不可用";
    case TimeService::Error::InvalidRtc:
      return "RTC时间无效";
    case TimeService::Error::NoWifiCredentials:
      return "缺少WiFi凭据";
    case TimeService::Error::WifiConnectFailed:
      return "WiFi连接失败";
    case TimeService::Error::NtpTimeout:
      return "NTP超时";
    case TimeService::Error::RtcWriteFailed:
      return "RTC写入失败";
    case TimeService::Error::Busy:
      return "服务忙";
    default:
      return "无";
  }
}

}  // namespace

bool TimeCalibrationPage::isTimeCalibrationSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == SettingsPage::kHomeIndex &&
         sectionFocus == SettingsPage::kTimeCalibrationItemIndex;
}

uint8_t TimeCalibrationPage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isTimeCalibrationSelection(homeFocus, sectionFocus) ? 1U : 0U;
}

bool TimeCalibrationPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool upEdge,
                                            bool downEdge, bool okEdge, uint32_t nowMs,
                                            TimeService& timeService,
                                            const WifiProvisionService& wifi) {
  if (!isTimeCalibrationSelection(homeFocus, sectionFocus)) {
    return false;
  }

  const bool ntpSyncing = timeService.snapshot().syncState == TimeService::SyncState::Syncing;
  if (upEdge || downEdge) {
    return true;
  }

  if (!okEdge) {
    return false;
  }

  if (ntpSyncing) {
    timeService.cancelNtpSync();
    return true;
  }
  char ssid[33] = {}, pass[65] = {};
  (void)wifi.networks().credentials(0, ssid, sizeof(ssid), pass, sizeof(pass));
  (void)timeService.requestNtpSync(nowMs, ssid, pass);
  return true;
}

bool TimeCalibrationPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                           TimeService& timeService) {
  if (!isTimeCalibrationSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (timeService.snapshot().syncState == TimeService::SyncState::Syncing) {
    timeService.cancelNtpSync();
  }

  // Let UiManager return to Settings with the same Left press.
  return false;
}

bool TimeCalibrationPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                                       DisplayMonoTft& display,
                                       const TimeService& timeService, SdBitmapFont* titleFont) const {
  if (!isTimeCalibrationSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  const int16_t width = static_cast<int16_t>(display.width());
  const TimeService::Snapshot& snapshot = timeService.snapshot();
  const bool syncing = snapshot.syncState == TimeService::SyncState::Syncing;
  const bool hasError = snapshot.error != TimeService::Error::None;
  SdBitmapFont* large = SdUiText::ready(titleFont) ? titleFont : nullptr;

  DrawUtils::drawClippedFilledRect(canvas, 0, yOffset, width - 1,
                                   yOffset + display.height() - 1, kBlack);
  clockIcon(display, 24, yOffset + 18, 8);
  label(display, nullptr, 39, yOffset + 23, "时间校准", 170);
  const char* status = hasError ? "校时失败" : syncStateTextZh(snapshot.syncState);
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  const int16_t statusWidth = SdUiText::width(display, nullptr, status);
  label(display, nullptr, width - 16 - statusWidth, yOffset + 23, status, statusWidth + 1);

  clockIcon(display, 55, yOffset + 83, 23);
  char clock[24];
  if (snapshot.valid) {
    snprintf(clock, sizeof(clock), "%02u:%02u", snapshot.now.hour, snapshot.now.minute);
  } else {
    snprintf(clock, sizeof(clock), "--:--");
  }
  label(display, large, 112, yOffset + 72, clock, 250);
  char date[36];
  if (snapshot.valid) {
    snprintf(date, sizeof(date), "%04u-%02u-%02u", snapshot.now.year,
             snapshot.now.month, snapshot.now.day);
  } else {
    snprintf(date, sizeof(date), "当前时间未校准");
  }
  label(display, nullptr, 112, yOffset + 97, date, 250);
  const char* detail = hasError ? errorTextZh(snapshot.error) :
      syncing ? "正在校时" : syncSourceTextZh(snapshot.lastSource);
  label(display, nullptr, 112, yOffset + 120, detail, 250);

  label(display, nullptr, 16, yOffset + 157, "< 返回", 120);
  const char* action = syncing ? "停止网络校时" : "网络校时";
  const int16_t actionWidth = SdUiText::width(display, nullptr, action);
  const int16_t actionX = width - 16 - actionWidth - 37;
  DrawUtils::drawRoundRect(canvas, actionX, yOffset + 140, 31, 18, 4, kBlack, kWhite);
  label(display, nullptr, actionX + 5, yOffset + 154, "OK", 24);
  label(display, nullptr, actionX + 37, yOffset + 157, action, actionWidth + 1);
  return true;
}
