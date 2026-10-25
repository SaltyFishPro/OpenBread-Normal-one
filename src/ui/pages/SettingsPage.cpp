#include "SettingsPage.h"
#include "../SdUiText.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../services/OtaService.h"
#include "../../services/TimeService.h"
#include "../../bsp/UsbSerial.h"
#include "../../services/WifiProvisionService.h"

namespace {
const char* const kSettingsLabels[] = {
    "重启设备", "系统更新", "WiFi配网", "时间校准", "设备自检",
    "USB串口", "恢复默认设置", "关于设备", "关于作者",
};

void renderAboutDeviceDetail(DisplayMonoTft& display, uint8_t detailPageIndex, int16_t yOffset,
                             const char* deviceIdText, const char* flashTotalText,
                             const char* sdStatusText, const WifiProvisionService& wifi) {
  auto& text = display.text();
  DetailHeader::render(display, "OpenBread Normal One", yOffset);

  char ipText[64];
  if (wifi.state() == WifiProvisionService::State::Connected && wifi.staIp()[0] != '\0') {
    snprintf(ipText, sizeof(ipText), "IP地址：%s", wifi.staIp());
  } else {
    snprintf(ipText, sizeof(ipText), "IP地址：未连接");
  }

  const char* const kPage0Lines[4] = {"设备名称：My device", "固件版本：V0.01", deviceIdText,
                                      "屏幕尺寸：2.9寸黑白反射屏"};
  const char* const kPage1Lines[4] = {ipText, "处理器：ESP32-S3", sdStatusText, flashTotalText};
  const int16_t kLineY[4] = {55, 87, 119, 151};
  const char* const* lines = (detailPageIndex == 0) ? kPage0Lines : kPage1Lines;

  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  for (uint8_t i = 0; i < 4; ++i) {
    text.drawUTF8(9, static_cast<int16_t>(yOffset + kLineY[i]), lines[i]);
  }
}

const char* wifiStateTitleZh(WifiProvisionService::State state) {
  switch (state) {
    case WifiProvisionService::State::Idle:
      return "WiFi配网";
    case WifiProvisionService::State::PortalReady:
      return "配网热点已开启";
    case WifiProvisionService::State::Connecting:
      return "正在连接路由器";
    case WifiProvisionService::State::SyncingTime:
      return "配网成功，正在校时";
    case WifiProvisionService::State::Connected:
      return "WiFi配网完成";
    case WifiProvisionService::State::PortalTimeout:
      return "配网超时";
    default:
      return "WiFi连接失败";
  }
}

const char* wifiErrorTextZh(WifiProvisionService::Error err) {
  switch (err) {
    case WifiProvisionService::Error::InvalidInput:
      return "输入无效";
    case WifiProvisionService::Error::AuthFailed:
      return "密码错误";
    case WifiProvisionService::Error::ApNotFound:
      return "热点未找到";
    case WifiProvisionService::Error::Timeout:
      return "连接超时";
    case WifiProvisionService::Error::Unknown:
      return "未知错误";
    default:
      return "无";
  }
}

void drawNetworkName(DisplayMonoTft& display, SdBitmapFont* font, int16_t baseline, const char* line) {
  SdUiText::Scope externalText(display, font);
  display.text().drawUTF8(8, baseline, line);
}

void renderWifiProvisionDetail(DisplayMonoTft& display, const WifiProvisionService& wifi,
                               const TimeService& time,
                               int16_t yOffset, SdBitmapFont* font) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const WifiProvisionService::State state = wifi.state();

  DetailHeader::render(display, wifiStateTitleZh(state), yOffset);

  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (state == WifiProvisionService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), "按OK开启配网热点");
  } else if (state == WifiProvisionService::State::PortalReady) {
    char line1[64];
    snprintf(line1, sizeof(line1), "AP: %s", wifi.apSsid());
    drawNetworkName(display, font, yOffset + 58, line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), "连接后打开: 192.168.4.1");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "网页输入WiFi名和密码");
  } else if (state == WifiProvisionService::State::Connecting) {
    char line1[64];
    snprintf(line1, sizeof(line1), "目标网络: %s", wifi.targetSsid());
    drawNetworkName(display, font, yOffset + 58, line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), "正在连接，请稍候");
  } else if (state == WifiProvisionService::State::Connected ||
             state == WifiProvisionService::State::SyncingTime) {
    char line1[64];
    snprintf(line1, sizeof(line1), "SSID: %s", wifi.targetSsid());
    drawNetworkName(display, font, yOffset + 58, line1);
    const char* status = "校时已取消，可在时间校准中重试";
    if (state == WifiProvisionService::State::SyncingTime) {
      status = "正在自动校准时间，请稍候";
    } else if (time.snapshot().syncState == TimeService::SyncState::Success) {
      status = "时间校准成功";
    } else if (time.snapshot().syncState == TimeService::SyncState::Failed) {
      status = "校时失败，请在时间校准中重试";
    }
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), status);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  state == WifiProvisionService::State::SyncingTime
                      ? "校时结束后自动关闭WiFi"
                      : "WiFi已关闭，配网信息已保存");
  } else {
    char line1[64];
    snprintf(line1, sizeof(line1), "失败原因: %s", wifiErrorTextZh(wifi.error()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), "按OK重试配网");
  }

  if (state == WifiProvisionService::State::Connected) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  "LEFT: 返回  OK: 重新开启配网热点");
  } else if (state == WifiProvisionService::State::SyncingTime) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 取消校时并返回");
  } else {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回  OK: 操作");
  }

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
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
  if (sectionFocus == kWifiProvisionItemIndex && wifi.canStartPortal()) {
    return ConfirmKind::WifiStart;
  }
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

  if (!isWifiProvisionSelection(homeFocus, sectionFocus) || !wifi.canStartPortal()) {
    return false;
  }
  wifi.startPortal(nowMs);
  return true;
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
  if (isWifiProvisionSelection(homeFocus, sectionFocus)) {
    renderWifiProvisionDetail(display, wifi, time, yOffset, font);
    return true;
  }

  if (isUsbSerialSelection(homeFocus, sectionFocus)) {
    renderUsbSerialDetail(display, yOffset);
    return true;
  }

  if (!isAboutDeviceSelection(homeFocus, sectionFocus)) {
    return false;
  }

  renderAboutDeviceDetail(display, detailPageIndex, yOffset, deviceIdText, flashTotalText,
                          sdStatusText, wifi);
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
    case ConfirmKind::WifiStart:
      return "开始配网？";
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
    case ConfirmKind::WifiStart:
      return "开始";
    default:
      return "";
  }
}
