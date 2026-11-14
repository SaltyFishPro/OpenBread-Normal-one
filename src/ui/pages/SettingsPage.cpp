#include "SettingsPage.h"
#include "../SdUiText.h"
#include "../TextUtils.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../../services/OtaService.h"
#include "../../services/TimeService.h"
#include "../../bsp/UsbSerial.h"
#include "../../services/WifiProvisionService.h"

#include <cstring>

namespace {
const char* const kSettingsLabels[] = {
    "重启设备", "系统更新", "WiFi配网", "时间校准", "设备自检",
    "USB串口", "恢复默认设置", "关于设备", "关于作者",
};

const char* infoValue(const char* text) {
  if (!text) return "";
  const char* separator = strstr(text, "：");
  return separator ? separator + strlen("：") : text;
}

void drawAboutCard(DisplayMonoTft& display, int16_t x, int16_t y, int16_t width,
                   const char* title, const char* value) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  DrawUtils::drawRoundRect(canvas, x, y, width, 44, 10,
                           ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
  text.drawUTF8(x + 10, y + 17, title);
  // Fit whole UTF-8 characters into the card without crossing its border.
  char fitted[96] = {};
  size_t used = 0;
  const char* cursor = value;
  while (*cursor && used + 5 < sizeof(fitted)) {
    const uint8_t bytes = TextUtils::utf8CharLen(*cursor);
    if (strlen(cursor) < bytes) break;
    memcpy(fitted + used, cursor, bytes);
    fitted[used + bytes] = '\0';
    if (text.getUTF8Width(fitted) > width - 20) { fitted[used] = '\0'; break; }
    used += bytes;
    cursor += bytes;
  }
  if (*cursor && used) {
    while (used && text.getUTF8Width(fitted) + text.getUTF8Width("...") > width - 20) {
      --used;
      while (used && TextUtils::utf8IsContinuation(fitted[used])) --used;
      fitted[used] = '\0';
    }
    memcpy(fitted + used, "...", 4);
  }
  text.drawUTF8(x + 10, y + 36, fitted);
}

void renderAboutDeviceDetail(DisplayMonoTft& display, uint8_t detailPageIndex, int16_t yOffset,
                             const char* deviceIdText, const char* flashTotalText,
                             const char* sdStatusText, SdBitmapFont* ui20) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t cardWidth = (width - 38) / 2;
  const int16_t secondX = 22 + cardWidth;
  const bool secondPage = detailPageIndex != 0;
  DrawUtils::drawClippedFilledRect(canvas, 0, yOffset, width - 1,
                                   yOffset + display.height() - 1, ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  DrawUtils::drawRoundRect(canvas, 19, yOffset + 10, 10, 14, 2,
                           ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
  DrawUtils::drawClippedFilledRect(canvas, 22, yOffset + 21, 25, yOffset + 21,
                                   ST7305_COLOR_WHITE);
  text.drawUTF8(39, yOffset + 23, "关于设备");
  text.drawUTF8(width - 52, yOffset + 23, secondPage ? "2 / 2" : "1 / 2");
  if (!secondPage) {
    DrawUtils::drawRoundRect(canvas, 16, yOffset + 34, width - 32, 53, 12,
                             ST7305_COLOR_WHITE, ST7305_COLOR_WHITE);
    DrawUtils::drawRoundRect(canvas, 28, yOffset + 44, 32, 32, 7,
                             ST7305_COLOR_WHITE, ST7305_COLOR_BLACK);
    DrawUtils::drawClippedRect(canvas, 33, yOffset + 49, 54, yOffset + 61,
                               ST7305_COLOR_BLACK);
    DrawUtils::drawClippedFilledRect(canvas, 37, yOffset + 69, 51, yOffset + 70,
                                    ST7305_COLOR_BLACK);
    const SdBitmapText::Clip clip = {72, static_cast<int16_t>(yOffset + 35),
                                     static_cast<int16_t>(width - 18),
                                     static_cast<int16_t>(yOffset + 65)};
    SdUiText::draw(display, ui20, 72, yOffset + 59, "OpenBread Normal One",
                   ST7305_COLOR_BLACK, clip);
    text.setForegroundColor(ST7305_COLOR_BLACK);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.drawUTF8(72, yOffset + 77, "设备名称");
    text.setForegroundColor(ST7305_COLOR_WHITE);
    text.setBackgroundColor(ST7305_COLOR_BLACK);
    char version[48];
    snprintf(version, sizeof(version), "V%s", FirmwareInfo::kVersion);
    drawAboutCard(display, 16, yOffset + 93, cardWidth, "固件版本", version);
    drawAboutCard(display, secondX, yOffset + 93, cardWidth, "设备ID", infoValue(deviceIdText));
  } else {
    char sdValue[64];
    snprintf(sdValue, sizeof(sdValue), "%s", infoValue(sdStatusText));
    char* suffix = strstr(sdValue, "总容量");
    if (suffix) *suffix = '\0';
    drawAboutCard(display, 16, yOffset + 34, cardWidth, "屏幕尺寸", "2.9寸黑白反射屏");
    drawAboutCard(display, secondX, yOffset + 34, cardWidth, "处理器", "ESP32-S3");
    drawAboutCard(display, 16, yOffset + 84, cardWidth, "SD卡", sdValue);
    drawAboutCard(display, secondX, yOffset + 84, cardWidth, "Flash", infoValue(flashTotalText));
  }
  text.drawUTF8(16, yOffset + 157, "< 返回");
  const char* footer = secondPage ? "上一页" : "下一页";
  const int16_t footerX = width - 16 - text.getUTF8Width(footer);
  text.drawUTF8(footerX, yOffset + 157, footer);
  // Draw the direction symbol independently of font availability.
  const int16_t arrowX = footerX - 10;
  const int16_t arrowY = yOffset + 151;
  DrawUtils::drawClippedFilledRect(canvas, arrowX, arrowY - 4, arrowX, arrowY + 4,
                                   ST7305_COLOR_WHITE);
  const int16_t tipY = secondPage ? arrowY - 4 : arrowY + 4;
  for (int16_t step = 1; step <= 3; ++step) {
    const int16_t y = secondPage ? tipY + step : tipY - step;
    DrawUtils::drawClippedFilledRect(canvas, arrowX - step, y, arrowX - step, y,
                                     ST7305_COLOR_WHITE);
    DrawUtils::drawClippedFilledRect(canvas, arrowX + step, y, arrowX + step, y,
                                     ST7305_COLOR_WHITE);
  }
}

void renderUsbSerialDetail(DisplayMonoTft& display, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const bool enabled = UsbSerial::isEnabled();

  DetailHeader::render(display, "USB 串口", yOffset);

  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.drawUTF8(10, static_cast<int16_t>(yOffset + 58),
                enabled ? "当前状态：已开启" : "当前状态：已关闭");
  text.drawUTF8(10, static_cast<int16_t>(yOffset + 86),
                enabled ? "主机可枚举为USB串口" : "主机侧已断开");
  text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回  OK: 切换");

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
}
}  // namespace

SettingsPage::ConfirmKind SettingsPage::popupForSelection(uint8_t homeFocus,
                                                          uint8_t sectionFocus) const {
  if (homeFocus != kHomeIndex) {
    return ConfirmKind::None;
  }

  if (sectionFocus == kRestartItemIndex) {
    return ConfirmKind::Restart;
  }
  if (sectionFocus == kFactoryResetItemIndex) {
    return ConfirmKind::FactoryReset;
  }
  return ConfirmKind::None;
}

SettingsPage::ConfirmKind SettingsPage::detailConfirmFor(uint8_t homeFocus, uint8_t sectionFocus,
                                                         const OtaService& ota,
                                                         const WifiProvisionService& wifi) const {
  if (homeFocus != kHomeIndex) {
    return ConfirmKind::None;
  }
  if (sectionFocus == kOtaItemIndex && ota.state() == OtaService::State::ReadyToApply) {
    return ConfirmKind::OtaApply;
  }
  (void)wifi;
  return ConfirmKind::None;
}

bool SettingsPage::isAboutDeviceSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kAboutDeviceItemIndex;
}

bool SettingsPage::isOtaSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kOtaItemIndex;
}

bool SettingsPage::isWifiProvisionSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kWifiProvisionItemIndex;
}

bool SettingsPage::isDeviceSelfTestSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kDeviceSelfTestItemIndex;
}

bool SettingsPage::isUsbSerialSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kUsbSerialItemIndex;
}

uint8_t SettingsPage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  if (isWifiProvisionSelection(homeFocus, sectionFocus) ||
      isOtaSelection(homeFocus, sectionFocus) ||
      isDeviceSelfTestSelection(homeFocus, sectionFocus)) {
    return 1U;
  }
  return isAboutDeviceSelection(homeFocus, sectionFocus) ? 2U : 1U;
}

bool SettingsPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                                     bool okEdge,
                                      uint32_t nowMs, WifiProvisionService& wifi,
                                      OtaService& ota) const {
  (void)detailPageIndex;
  if (!okEdge) {
    return false;
  }

  if (isUsbSerialSelection(homeFocus, sectionFocus)) {
    (void)UsbSerial::setEnabled(!UsbSerial::isEnabled());
    return true;
  }

  if (isOtaSelection(homeFocus, sectionFocus)) {
    if (ota.state() == OtaService::State::UpdateAvailable) {
      ota.requestDownload(nowMs);
    } else if (ota.state() == OtaService::State::ReadyToApply) {
      ota.requestApply();
    } else if (ota.state() == OtaService::State::Downloading ||
               ota.state() == OtaService::State::Verifying) {
      return true;
    } else {
      ota.requestCheck(nowMs);
    }
    return true;
  }

  (void)wifi;
  return false;
}

bool SettingsPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                     WifiProvisionService& wifi, OtaService& ota,
                                     TimeService& time) const {
  if (isOtaSelection(homeFocus, sectionFocus)) {
    ota.cancel();
    return true;
  }

  if (isWifiProvisionSelection(homeFocus, sectionFocus) && wifi.isRadioActive()) {
    wifi.cancelProvision(time);
    return true;
  }

  return false;
}

const char* const* SettingsPage::menuLabels() const { return kSettingsLabels; }

uint8_t SettingsPage::menuItemCount() const {
  return static_cast<uint8_t>(sizeof(kSettingsLabels) / sizeof(kSettingsLabels[0]));
}

bool SettingsPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                                int16_t yOffset, DisplayMonoTft& display,
                                const char* deviceIdText, const char* flashTotalText,
                                const char* sdStatusText,
                                const WifiProvisionService& wifi,
                                const TimeService& time, SdBitmapFont* font) const {
  if (isUsbSerialSelection(homeFocus, sectionFocus)) {
    renderUsbSerialDetail(display, yOffset);
    return true;
  }

  if (!isAboutDeviceSelection(homeFocus, sectionFocus)) {
    return false;
  }

  renderAboutDeviceDetail(display, detailPageIndex, yOffset, deviceIdText, flashTotalText,
                          sdStatusText, font);
  return true;
}

const char* SettingsPage::confirmTitle(ConfirmKind kind) const {
  switch (kind) {
    case ConfirmKind::Restart:
      return "重启设备？";
    case ConfirmKind::FactoryReset:
      return "恢复默认设置？";
    case ConfirmKind::OtaApply:
      return "应用更新并重启？";
    default:
      return "";
  }
}

const char* SettingsPage::confirmPrimaryLabel(ConfirmKind kind) const {
  return kind == ConfirmKind::None ? "" : "取消";
}

const char* SettingsPage::confirmDangerLabel(ConfirmKind kind) const {
  switch (kind) {
    case ConfirmKind::Restart:
      return "重启";
    case ConfirmKind::FactoryReset:
      return "恢复";
    case ConfirmKind::OtaApply:
      return "应用";
    default:
      return "";
  }
}
