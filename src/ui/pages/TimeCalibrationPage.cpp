#include "TimeCalibrationPage.h"
#include "SettingsPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../services/TimeService.h"
#include "../../services/WifiProvisionService.h"

namespace {
void drawSelectionButton(DisplayMonoTft& display, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                         bool selected, const char* label) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const uint16_t fill = selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  const uint16_t fg = selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;

  canvas.drawFilledRectangle(x1, y1, x2, y2, fill);
  canvas.drawRectangle(x1, y1, x2, y2, ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(fill);
  text.setForegroundColor(fg);
  text.setFontMode(selected ? 0 : 1);
  const int16_t labelW = text.getUTF8Width(label);
  const int16_t labelX = static_cast<int16_t>(x1 + ((x2 - x1 + 1) - labelW) / 2);
  const int16_t labelY = static_cast<int16_t>(y1 + ((y2 - y1 + 1) / 2) + 5);
  text.drawUTF8(labelX, labelY, label);
}

const char* syncSourceTextZh(TimeService::SyncSource source) {
  switch (source) {
    case TimeService::SyncSource::Bluetooth:
      return "蓝牙校时";
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
    case TimeService::Error::InvalidInput:
      return "输入无效";
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
  (void)timeService.requestNtpSync(nowMs, wifi.targetSsid(), wifi.targetPass());
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
                                       const TimeService& timeService) const {
  if (!isTimeCalibrationSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const TimeService::Snapshot& snapshot = timeService.snapshot();

  DetailHeader::render(display, "时间校准", yOffset);

  const int16_t infoX = 10;
  const int16_t infoY1 = static_cast<int16_t>(yOffset + 55);
  const int16_t infoY2 = static_cast<int16_t>(yOffset + 79);
  const int16_t infoY3 = static_cast<int16_t>(yOffset + 103);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line1[72];
  char line2[72];
  char line3[72];

  if (snapshot.valid) {
    snprintf(line1, sizeof(line1), "%04u-%02u-%02u  %02u:%02u",
             static_cast<unsigned>(snapshot.now.year), static_cast<unsigned>(snapshot.now.month),
             static_cast<unsigned>(snapshot.now.day), static_cast<unsigned>(snapshot.now.hour),
             static_cast<unsigned>(snapshot.now.minute));
  } else {
    snprintf(line1, sizeof(line1), "当前时间未校准");
  }

  snprintf(line2, sizeof(line2), "校时来源：%s", syncSourceTextZh(snapshot.lastSource));

  const bool hasError = snapshot.error != TimeService::Error::None;
  snprintf(line3, sizeof(line3), hasError ? "错误：%s" : "状态：%s",
           hasError ? errorTextZh(snapshot.error) : syncStateTextZh(snapshot.syncState));

  text.drawUTF8(infoX, infoY1, line1);
  text.drawUTF8(infoX, infoY2, line2);
  text.drawUTF8(infoX, infoY3, line3);
  const bool ntpSyncing = snapshot.syncState == TimeService::SyncState::Syncing;
  const int16_t buttonX1 = 18;
  const int16_t buttonX2 = static_cast<int16_t>(width - 19);
  const int16_t buttonY1 = static_cast<int16_t>(yOffset + 119);
  const int16_t buttonY2 = static_cast<int16_t>(yOffset + 151);
  drawSelectionButton(display, buttonX1, buttonY1, buttonX2, buttonY2, true,
                      ntpSyncing ? "停止网络校时" : "网络校时");

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
  return true;
}
