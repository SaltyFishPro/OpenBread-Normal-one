#include "SettingsPage.h"
#include "../SdUiText.h"
#include "../TextUtils.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../services/OtaService.h"
#include "../../services/TimeService.h"
#include "../../bsp/UsbSerial.h"
#include "../../services/WifiProvisionService.h"
#include <qrcode.h>

#include <cstring>

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

constexpr int16_t kContentLeft = 16;
constexpr int16_t kWifiQrMaxModules = 29;
constexpr int16_t kWifiQrRowBytes = (kWifiQrMaxModules + 7) / 8;
constexpr int16_t kWifiQrQuietZone = 4;
constexpr int16_t kWifiQrScale = 3;
constexpr int16_t kWifiQrFrameSize =
    (kWifiQrMaxModules + 2 * kWifiQrQuietZone) * kWifiQrScale;
uint8_t gWifiQrRows[kWifiQrMaxModules][kWifiQrRowBytes] = {};
char gWifiQrSsid[33] = {};
int16_t gWifiQrSize = 0;

void captureWifiQr(esp_qrcode_handle_t qr) {
  const int size = esp_qrcode_get_size(qr);
  if (size <= 0 || size > kWifiQrMaxModules) {
    gWifiQrSize = 0;
    return;
  }
  memset(gWifiQrRows, 0, sizeof(gWifiQrRows));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (esp_qrcode_get_module(qr, x, y)) {
        gWifiQrRows[y][x >> 3] |= static_cast<uint8_t>(0x80U >> (x & 7));
      }
    }
  }
  gWifiQrSize = static_cast<int16_t>(size);
}

bool prepareWifiQr(const char* ssid) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  if (gWifiQrSize > 0 && strcmp(gWifiQrSsid, ssid) == 0) {
    return true;
  }

  gWifiQrSize = 0;
  gWifiQrSsid[0] = '\0';
  char payload[72];
  const int length = snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ssid);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload)) {
    return false;
  }

  esp_qrcode_config_t config = {};
  config.display_func = captureWifiQr;
  config.max_qrcode_version = 3;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  if (esp_qrcode_generate(&config, payload) != ESP_OK || gWifiQrSize == 0) {
    return false;
  }

  snprintf(gWifiQrSsid, sizeof(gWifiQrSsid), "%s", ssid);
  return true;
}

bool wifiQrModule(int16_t x, int16_t y) {
  return (gWifiQrRows[y][x >> 3] & static_cast<uint8_t>(0x80U >> (x & 7))) != 0;
}

void drawWifiQr(DisplayMonoTft& display, int16_t x, int16_t y) {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(x, y, static_cast<int16_t>(x + kWifiQrFrameSize - 1),
                             static_cast<int16_t>(y + kWifiQrFrameSize - 1),
                             ST7305_COLOR_WHITE);
  for (int16_t row = 0; row < gWifiQrSize; ++row) {
    for (int16_t col = 0; col < gWifiQrSize; ++col) {
      if (!wifiQrModule(col, row)) {
        continue;
      }
      const int16_t left = static_cast<int16_t>(x + (col + kWifiQrQuietZone) * kWifiQrScale);
      const int16_t top = static_cast<int16_t>(y + (row + kWifiQrQuietZone) * kWifiQrScale);
      canvas.drawFilledRectangle(left, top, static_cast<int16_t>(left + kWifiQrScale - 1),
                                 static_cast<int16_t>(top + kWifiQrScale - 1),
                                 ST7305_COLOR_BLACK);
    }
  }
}

void setFixedFont(DisplayMonoTft& display) {
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
}

void drawFixed(DisplayMonoTft& display, int16_t x, int16_t baseline, const char* value) {
  setFixedFont(display);
  display.text().drawUTF8(x, baseline, value);
}

void drawFixedCentered(DisplayMonoTft& display, int16_t centerX, int16_t baseline,
                       const char* value) {
  setFixedFont(display);
  display.text().drawUTF8(static_cast<int16_t>(centerX - display.text().getUTF8Width(value) / 2),
                          baseline, value);
}

// ASCII-only face for the setup AP name, portal URL and station IP. Values longer
// than the content box are truncated at the byte level, which is safe because
// these fields are never multi-byte UTF-8.
void drawAsciiCentered(DisplayMonoTft& display, const char* value, const uint8_t* font,
                       int16_t baseline, int16_t left, int16_t right) {
  if (value == nullptr || value[0] == '\0') {
    return;
  }
  auto& text = display.text();
  text.setFont(font);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line[48];
  size_t out = 0;
  const int16_t maxWidth = static_cast<int16_t>(right - left);
  for (size_t i = 0; value[i] != '\0' && out + 1U < sizeof(line); ++i) {
    line[out] = value[i];
    line[out + 1U] = '\0';
    if (text.getUTF8Width(line) > maxWidth) {
      break;
    }
    ++out;
  }
  line[out] = '\0';
  const int16_t lineWidth = text.getUTF8Width(line);
  text.drawUTF8(static_cast<int16_t>(left + (maxWidth - lineWidth) / 2), baseline, line);
}

// Network name may contain Chinese, so it is drawn with the SD full font while it
// is available and truncated to the content box by character.
void drawNetworkLine(DisplayMonoTft& display, SdBitmapFont* font, int16_t x, int16_t baseline,
                     const char* value, int16_t maxWidth) {
  if (value == nullptr || value[0] == '\0') {
    return;
  }
  SdUiText::Scope externalText(display, font);
  char line[64];
  size_t out = 0;
  for (size_t i = 0; value[i] != '\0' && out < sizeof(line) - 1U;) {
    const uint8_t charLen = TextUtils::utf8CharLen(value[i]);
    if (out + charLen >= sizeof(line)) {
      break;
    }
    for (uint8_t j = 0; j < charLen && value[i + j] != '\0'; ++j) {
      line[out++] = value[i + j];
    }
    line[out] = '\0';
    if (SdUiText::width(display, font, line) > maxWidth) {
      out = static_cast<size_t>(out - charLen);
      line[out] = '\0';
      break;
    }
    i += charLen;
  }
  setFixedFont(display);
  display.text().drawUTF8(x, baseline, line);
}

void renderWifiProvisionDetail(DisplayMonoTft& display, const WifiProvisionService& wifi,
                               const TimeService& time,
                               int16_t yOffset, SdBitmapFont* font) {
  auto& canvas = display.canvas();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t centerX = static_cast<int16_t>(width / 2);
  const int16_t right = static_cast<int16_t>(width - 16);
  const WifiProvisionService::State state = wifi.state();
  (void)time;

  DetailHeader::render(display, wifiStateTitleZh(state), yOffset);

  if (state == WifiProvisionService::State::Idle) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 50),
              "开启热点后，手机连接设备热点");
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 78),
              "浏览器打开 192.168.4.1");
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 106),
              "在网页输入 WiFi 名称和密码");
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 134),
              "设备会自动连接并校时");
  } else if (state == WifiProvisionService::State::PortalReady) {
    const int16_t qrX = static_cast<int16_t>(right - kWifiQrFrameSize + 1);
    const int16_t qrY = static_cast<int16_t>(yOffset + 38);
    const int16_t leftWidth = static_cast<int16_t>(qrX - kContentLeft - 12);
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 51), "扫码加入热点");
    drawAsciiCentered(display, wifi.apSsid(), u8g2_font_fur14_tf,
                      static_cast<int16_t>(yOffset + 78), kContentLeft,
                      static_cast<int16_t>(kContentLeft + leftWidth));
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 108),
              "连接后等待登录页面提示");
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 138),
              "未弹出？打开 192.168.4.1");
    if (prepareWifiQr(wifi.apSsid())) {
      drawWifiQr(display, qrX, qrY);
    }
  } else if (state == WifiProvisionService::State::Connecting) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 46), "目标网络");
    drawNetworkLine(display, font, kContentLeft, static_cast<int16_t>(yOffset + 72),
                    wifi.targetSsid(), static_cast<int16_t>(width - 2 * kContentLeft));
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 108),
              "正在连接路由器，请稍候");
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 134),
              "手机请保持连接设备热点");
  } else if (state == WifiProvisionService::State::Connected ||
             state == WifiProvisionService::State::SyncingTime) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 46), "网络");
    drawNetworkLine(display, font, kContentLeft, static_cast<int16_t>(yOffset + 72),
                    wifi.targetSsid(), static_cast<int16_t>(width - 2 * kContentLeft));
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 96), "设备 IP");
    drawAsciiCentered(display, wifi.staIp(), u8g2_font_fur25_tf,
                      static_cast<int16_t>(yOffset + 120), kContentLeft, right);
    if (state == WifiProvisionService::State::SyncingTime) {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 138),
                        "正在校时，完成后自动关闭 WiFi");
    } else if (wifi.isPortalActive() && wifi.holdRemainingSeconds(millis()) > 0) {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 138),
                        "热点将在数秒后关闭");
    } else {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 138),
                        "WiFi 已关闭，配网信息已保存");
    }
  } else if (state == WifiProvisionService::State::PortalTimeout) {
    drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 84),
                      "热点已关闭，以节省电量");
    drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 112),
                      "按 OK 重新开启配网热点");
  } else {
    char reason[64];
    snprintf(reason, sizeof(reason), "失败原因：%s", wifiErrorTextZh(wifi.error()));
    drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 46), reason);
    if (wifi.targetSsid()[0] != '\0') {
      drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + 70), "网络");
      drawNetworkLine(display, font, kContentLeft, static_cast<int16_t>(yOffset + 94),
                      wifi.targetSsid(), static_cast<int16_t>(width - 2 * kContentLeft));
    }
    if (wifi.canRetryConnect()) {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 124),
                        "请检查后，按 OK 重新连接");
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 140),
                        "热点仍然开启，可在网页重试");
    } else if (wifi.isPortalActive()) {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 124),
                        "请在网页中重试");
    } else {
      drawFixedCentered(display, centerX, static_cast<int16_t>(yOffset + 124),
                        "按 OK 重新开启热点");
    }
  }

  if (state == WifiProvisionService::State::Connecting) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 取消并关闭热点");
  } else if (state == WifiProvisionService::State::SyncingTime) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 取消校时并返回");
  } else if (state == WifiProvisionService::State::Connected) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              wifi.isPortalActive() && wifi.holdRemainingSeconds(millis()) > 0
                  ? "LEFT: 返回"
                  : "LEFT: 返回  OK: 重新配网");
  } else if (wifi.canRetryConnect()) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 返回  OK: 重新连接");
  } else if (state == WifiProvisionService::State::PortalReady) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 关闭热点");
  } else if (state == WifiProvisionService::State::PortalTimeout ||
             state == WifiProvisionService::State::Failed) {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 返回  OK: 重新开启");
  } else {
    drawFixed(display, kContentLeft, static_cast<int16_t>(yOffset + height - 10),
              "LEFT: 返回  OK: 开启热点");
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

  if (!isWifiProvisionSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (wifi.canRetryConnect()) {
    return wifi.retryConnect();
  }
  if (!wifi.canStartPortal()) {
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
