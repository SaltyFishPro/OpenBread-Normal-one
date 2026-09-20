#include "SettingsPage.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../services/OtaService.h"
#include "../../services/TimeService.h"
#include "../../bsp/UsbSerial.h"
#include "../assets/submenu/desktopclock.h"
#include "../assets/submenu/language.h"
#include "../../services/WifiProvisionService.h"
#include "../assets/submenu/about.h"
#include "../assets/submenu/author.h"
#include "../assets/submenu/reset.h"
#include "../assets/submenu/restart.h"
#include "../assets/submenu/wificonnet.h"
#if __has_include("../assets/submenu/ota.h")
#include "../assets/submenu/ota.h"
#define OB_HAS_OTA_ICON 1
#else
#define OB_HAS_OTA_ICON 0
#endif

namespace {
#if OB_HAS_OTA_ICON
const IconBitmap::Anim kSettingsOtaIcon = {
    reinterpret_cast<const uint8_t*>(&ota_frames[0][0]),
    OTA_FRAME_BYTES,
    OTA_FRAME_WIDTH,
    OTA_FRAME_HEIGHT,
    OTA_FRAME_DELAY,
    OTA_FRAME_COUNT};
#else
const IconBitmap::Anim kSettingsOtaIcon = {
    reinterpret_cast<const uint8_t*>(&reset_frames[0][0]),
    RESET_FRAME_BYTES,
    RESET_FRAME_WIDTH,
    RESET_FRAME_HEIGHT,
    RESET_FRAME_DELAY,
    RESET_FRAME_COUNT};
#endif

const SettingsPage::MenuItem kSettingsItems[] = {
    {"重启设备",
     {reinterpret_cast<const uint8_t*>(&restart_frames[0][0]), RESTART_FRAME_BYTES,
      RESTART_FRAME_WIDTH, RESTART_FRAME_HEIGHT, RESTART_FRAME_DELAY, RESTART_FRAME_COUNT}},
    {"OTA更新", kSettingsOtaIcon},
    {"WiFi配网",
     {reinterpret_cast<const uint8_t*>(&wificonnet_frames[0][0]), WIFICONNET_FRAME_BYTES,
      WIFICONNET_FRAME_WIDTH, WIFICONNET_FRAME_HEIGHT, WIFICONNET_FRAME_DELAY,
      WIFICONNET_FRAME_COUNT}},
    {"时间校准",
     {reinterpret_cast<const uint8_t*>(&desktopclock_frames[0][0]), DESKTOPCLOCK_FRAME_BYTES,
      DESKTOPCLOCK_FRAME_WIDTH, DESKTOPCLOCK_FRAME_HEIGHT, DESKTOPCLOCK_FRAME_DELAY,
      DESKTOPCLOCK_FRAME_COUNT}},
    {"设备自检",
     {reinterpret_cast<const uint8_t*>(&reset_frames[0][0]), RESET_FRAME_BYTES,
      RESET_FRAME_WIDTH, RESET_FRAME_HEIGHT, RESET_FRAME_DELAY, RESET_FRAME_COUNT}},
    {"USB串口",
     {reinterpret_cast<const uint8_t*>(&language_frames[0][0]), LANGUAGE_FRAME_BYTES,
      LANGUAGE_FRAME_WIDTH, LANGUAGE_FRAME_HEIGHT, LANGUAGE_FRAME_DELAY,
      LANGUAGE_FRAME_COUNT}},
    {"恢复默认设置",
     {reinterpret_cast<const uint8_t*>(&reset_frames[0][0]), RESET_FRAME_BYTES,
      RESET_FRAME_WIDTH, RESET_FRAME_HEIGHT, RESET_FRAME_DELAY, RESET_FRAME_COUNT}},
    {"关于设备",
     {reinterpret_cast<const uint8_t*>(&about_frames[0][0]), ABOUT_FRAME_BYTES,
      ABOUT_FRAME_WIDTH, ABOUT_FRAME_HEIGHT, ABOUT_FRAME_DELAY, ABOUT_FRAME_COUNT}},
    {"关于作者",
     {reinterpret_cast<const uint8_t*>(&author_frames[0][0]), AUTHOR_FRAME_BYTES,
      AUTHOR_FRAME_WIDTH, AUTHOR_FRAME_HEIGHT, AUTHOR_FRAME_DELAY, AUTHOR_FRAME_COUNT}},
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

  text.setFont(chinese_font_all);
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

void renderWifiProvisionDetail(DisplayMonoTft& display, const WifiProvisionService& wifi,
                               const TimeService& time,
                               int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const WifiProvisionService::State state = wifi.state();

  DetailHeader::render(display, wifiStateTitleZh(state), yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (state == WifiProvisionService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), "按OK开启配网热点");
  } else if (state == WifiProvisionService::State::PortalReady) {
    char line1[64];
    snprintf(line1, sizeof(line1), "AP: %s", wifi.apSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), "连接后打开: 192.168.4.1");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "网页输入WiFi名和密码");
  } else if (state == WifiProvisionService::State::Connecting) {
    char line1[64];
    snprintf(line1, sizeof(line1), "目标网络: %s", wifi.targetSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), "正在连接，请稍候");
  } else if (state == WifiProvisionService::State::Connected ||
             state == WifiProvisionService::State::SyncingTime) {
    char line1[64];
    snprintf(line1, sizeof(line1), "SSID: %s", wifi.targetSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
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

const char* otaStateTitleZh(OtaService::State state) {
  switch (state) {
    case OtaService::State::Idle:
      return "OTA更新";
    case OtaService::State::Checking:
      return "正在检查";
    case OtaService::State::UpToDate:
      return "已是最新";
    case OtaService::State::UpdateAvailable:
      return "有可用更新";
    case OtaService::State::Downloading:
      return "正在下载";
    case OtaService::State::Verifying:
      return "正在校验";
    case OtaService::State::ReadyToApply:
      return "等待重启生效";
    case OtaService::State::Applying:
      return "正在重启应用";
    default:
      return "检查失败";
  }
}

const char* otaErrorTextZh(OtaService::Error err) {
  switch (err) {
    case OtaService::Error::None:
      return "无";
    case OtaService::Error::Busy:
      return "忙碌中";
    case OtaService::Error::NoCredentials:
      return "缺少WiFi凭据";
    case OtaService::Error::ConnectTimeout:
      return "WiFi连接超时";
    case OtaService::Error::HttpRequestFailed:
      return "HTTP请求失败";
    case OtaService::Error::HttpStatusInvalid:
      return "Manifest状态码异常";
    case OtaService::Error::ManifestInvalid:
      return "Manifest无效";
    case OtaService::Error::ProductMismatch:
      return "产品型号不匹配";
    case OtaService::Error::ChannelMismatch:
      return "更新通道不匹配";
    case OtaService::Error::VersionNotNew:
      return "版本不是更新";
    case OtaService::Error::UrlInvalid:
      return "固件地址无效";
    case OtaService::Error::SizeInvalid:
      return "固件大小无效";
    case OtaService::Error::ShaInvalid:
      return "SHA256无效";
    case OtaService::Error::DownloadFailed:
      return "下载失败";
    case OtaService::Error::UpdateBeginFailed:
      return "升级区初始化失败";
    case OtaService::Error::UpdateWriteFailed:
      return "升级写入失败";
    case OtaService::Error::UpdateFinalizeFailed:
      return "升级封包失败";
    case OtaService::Error::ShaMismatch:
      return "SHA256不匹配";
    case OtaService::Error::ApplyNotReady:
      return "当前状态不可应用";
    case OtaService::Error::PostApplyVerifyFailed:
      return "重启后版本核验失败";
    default:
      return "未知";
  }
}

void renderOtaDetail(DisplayMonoTft& display, const OtaService& ota, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  DetailHeader::render(display, otaStateTitleZh(ota.state()), yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line1[96];
  char line2[96];
  const OtaService::ManifestInfo& manifest = ota.manifest();
  snprintf(line1, sizeof(line1), "本地版本: %s (%lu)",
           FirmwareInfo::kVersion, static_cast<unsigned long>(FirmwareInfo::kBuild));

  if (ota.state() == OtaService::State::UpdateAvailable ||
      ota.state() == OtaService::State::UpToDate) {
    snprintf(line2, sizeof(line2), "远端版本: %s (%lu)", manifest.version,
             static_cast<unsigned long>(manifest.build));
  } else {
    snprintf(line2, sizeof(line2), "远端版本: --");
  }

  text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);

  if (ota.state() == OtaService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "按OK检查更新");
  } else if (ota.state() == OtaService::State::Checking) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "正在通过WiFi检查manifest");
  } else if (ota.state() == OtaService::State::UpToDate) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "当前已是最新版本");
  } else if (ota.state() == OtaService::State::UpdateAvailable) {
    char sizeLine[96];
    snprintf(sizeLine, sizeof(sizeLine), "包大小: %lu bytes",
             static_cast<unsigned long>(manifest.size));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), sizeLine);
  } else if (ota.state() == OtaService::State::Downloading) {
    char progressLine[96];
    snprintf(progressLine, sizeof(progressLine),
             "下载进度: %u%% (%lu/%lu)",
             static_cast<unsigned>(ota.progressPercent()),
             static_cast<unsigned long>(ota.downloadedBytes()),
             static_cast<unsigned long>(ota.totalBytes()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), progressLine);
  } else if (ota.state() == OtaService::State::Verifying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "正在进行SHA256校验");
  } else if (ota.state() == OtaService::State::ReadyToApply) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "固件已写入，重启后生效");
  } else if (ota.state() == OtaService::State::Applying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), "正在重启设备...");
  } else {
    char errLine[96];
    snprintf(errLine, sizeof(errLine), "错误: %s", otaErrorTextZh(ota.error()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), errLine);
  }

  const OtaService::State otaState = ota.state();
  if (otaState == OtaService::State::UpdateAvailable) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回  OK: 下载");
  } else if (otaState == OtaService::State::ReadyToApply) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回  OK: 立即应用");
  } else if (otaState == OtaService::State::Downloading ||
             otaState == OtaService::State::Verifying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回/取消  OK: 等待");
  } else {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), "LEFT: 返回  OK: 检查");
  }

  if (ota.hasPostApplyResult()) {
    char verifyLine[96];
    snprintf(verifyLine, sizeof(verifyLine), "上次应用结果: %s",
             ota.postApplySucceeded() ? "成功" : "失败");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 142), verifyLine);
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

  text.setFont(chinese_font_all);
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

SettingsPage::PopupKind SettingsPage::popupForSelection(uint8_t homeFocus,
                                                        uint8_t sectionFocus) const {
  if (homeFocus != kHomeIndex) {
    return PopupKind::None;
  }

  if (sectionFocus == kRestartItemIndex) {
    return PopupKind::RestartConfirm;
  }
  return PopupKind::None;
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

const SettingsPage::MenuItem* SettingsPage::menuItems() const { return kSettingsItems; }

uint8_t SettingsPage::menuItemCount() const {
  return static_cast<uint8_t>(sizeof(kSettingsItems) / sizeof(kSettingsItems[0]));
}

bool SettingsPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                                int16_t yOffset, DisplayMonoTft& display,
                                const char* deviceIdText, const char* flashTotalText,
                                const char* sdStatusText,
                                const WifiProvisionService& wifi,
                                const OtaService& ota, const TimeService& time) const {
  if (isOtaSelection(homeFocus, sectionFocus)) {
    renderOtaDetail(display, ota, yOffset);
    return true;
  }

  if (isWifiProvisionSelection(homeFocus, sectionFocus)) {
    renderWifiProvisionDetail(display, wifi, time, yOffset);
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

const char* SettingsPage::popupTitle(PopupKind kind) const {
  if (kind == PopupKind::RestartConfirm) {
    return "重启设备?";
  }
  return "";
}

const char* SettingsPage::popupPrimaryLabel(PopupKind kind) const {
  if (kind == PopupKind::RestartConfirm) {
    return "是";
  }
  return "";
}

const char* SettingsPage::popupSecondaryLabel(PopupKind kind) const {
  if (kind == PopupKind::RestartConfirm) {
    return "否";
  }
  return "";
}
