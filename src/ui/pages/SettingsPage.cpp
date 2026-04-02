#include "SettingsPage.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../../services/OtaService.h"
#include "../../services/WifiProvisionService.h"
#include "../assets/submenu_icon/about.h"
#include "../assets/submenu_icon/author.h"
#include "../assets/submenu_icon/language.h"
#include "../assets/submenu_icon/reset.h"
#include "../assets/submenu_icon/restart.h"
#include "../assets/submenu_icon/wificonnet.h"
#if __has_include("../assets/submenu_icon/ota.h")
#include "../assets/submenu_icon/ota.h"
#define OB_HAS_OTA_ICON 1
#else
#define OB_HAS_OTA_ICON 0
#endif

namespace {
constexpr int16_t kDetailHeaderHeight = 28;

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
    {"重启设备", "Restart Device",
     {reinterpret_cast<const uint8_t*>(&restart_frames[0][0]), RESTART_FRAME_BYTES,
      RESTART_FRAME_WIDTH, RESTART_FRAME_HEIGHT, RESTART_FRAME_DELAY, RESTART_FRAME_COUNT}},
    {"语言", "Language",
     {reinterpret_cast<const uint8_t*>(&language_frames[0][0]), LANGUAGE_FRAME_BYTES,
      LANGUAGE_FRAME_WIDTH, LANGUAGE_FRAME_HEIGHT, LANGUAGE_FRAME_DELAY, LANGUAGE_FRAME_COUNT}},
    {"OTA更新", "OTA Update", kSettingsOtaIcon},
    {"WiFi配网", "WiFi Provision",
     {reinterpret_cast<const uint8_t*>(&wificonnet_frames[0][0]), WIFICONNET_FRAME_BYTES,
      WIFICONNET_FRAME_WIDTH, WIFICONNET_FRAME_HEIGHT, WIFICONNET_FRAME_DELAY,
      WIFICONNET_FRAME_COUNT}},
    {"恢复默认设置", "Reset to Defaults",
     {reinterpret_cast<const uint8_t*>(&reset_frames[0][0]), RESET_FRAME_BYTES,
      RESET_FRAME_WIDTH, RESET_FRAME_HEIGHT, RESET_FRAME_DELAY, RESET_FRAME_COUNT}},
    {"关于设备", "About Device",
     {reinterpret_cast<const uint8_t*>(&about_frames[0][0]), ABOUT_FRAME_BYTES,
      ABOUT_FRAME_WIDTH, ABOUT_FRAME_HEIGHT, ABOUT_FRAME_DELAY, ABOUT_FRAME_COUNT}},
    {"关于作者", "About Author",
     {reinterpret_cast<const uint8_t*>(&author_frames[0][0]), AUTHOR_FRAME_BYTES,
      AUTHOR_FRAME_WIDTH, AUTHOR_FRAME_HEIGHT, AUTHOR_FRAME_DELAY, AUTHOR_FRAME_COUNT}},
};

void renderDetailHeader(DisplayMonoTft& display, const char* title, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());

  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kDetailHeaderHeight - 1),
                             ST7305_COLOR_BLACK);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  const int16_t titleW = text.getUTF8Width(title);
  const int16_t titleX = static_cast<int16_t>((width - titleW) / 2);
  text.drawUTF8(titleX, static_cast<int16_t>(yOffset + 22), title);

  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
}

void renderAboutDeviceDetail(DisplayMonoTft& display, HomePage::Language language,
                             uint8_t detailPageIndex, int16_t yOffset,
                             const char* deviceIdText, const char* flashTotalText,
                             const char* sdStatusText, const WifiProvisionService& wifi) {
  auto& text = display.text();
  renderDetailHeader(display, "OpenBread Normal One", yOffset);

  char ipText[64];
  if (wifi.state() == WifiProvisionService::State::Connected && wifi.staIp()[0] != '\0') {
    snprintf(ipText, sizeof(ipText), language == HomePage::Language::Zh ? "IP地址：%s" : "IP: %s",
             wifi.staIp());
  } else {
    snprintf(ipText, sizeof(ipText), language == HomePage::Language::Zh ? "IP地址：未连接" : "IP: Not connected");
  }

  const char* const kPage0LinesZh[4] = {"设备名称：My device", "固件版本：V0.01", deviceIdText,
                                        "屏幕尺寸：2.9寸黑白反射屏"};
  const char* const kPage0LinesEn[4] = {"Device: My device", "FW: V0.01", deviceIdText,
                                        "Display: 2.9 BW Reflective"};
  const char* const kPage1LinesZh[4] = {ipText, "处理器：ESP32-S3", sdStatusText, flashTotalText};
  const char* const kPage1LinesEn[4] = {ipText, "MCU: ESP32-S3", sdStatusText, flashTotalText};
  const int16_t kLineY[4] = {55, 87, 119, 151};
  const bool zh = language == HomePage::Language::Zh;
  const char* const* lines = (detailPageIndex == 0)
                                 ? (zh ? kPage0LinesZh : kPage0LinesEn)
                                 : (zh ? kPage1LinesZh : kPage1LinesEn);

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
    case WifiProvisionService::State::Connected:
      return "WiFi连接成功";
    case WifiProvisionService::State::PortalTimeout:
      return "配网超时";
    default:
      return "WiFi连接失败";
  }
}

const char* wifiStateTitleEn(WifiProvisionService::State state) {
  switch (state) {
    case WifiProvisionService::State::Idle:
      return "WiFi Provision";
    case WifiProvisionService::State::PortalReady:
      return "Portal Ready";
    case WifiProvisionService::State::Connecting:
      return "Connecting";
    case WifiProvisionService::State::Connected:
      return "WiFi Connected";
    case WifiProvisionService::State::PortalTimeout:
      return "Portal Timeout";
    default:
      return "WiFi Failed";
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

const char* wifiErrorTextEn(WifiProvisionService::Error err) {
  switch (err) {
    case WifiProvisionService::Error::InvalidInput:
      return "invalid input";
    case WifiProvisionService::Error::AuthFailed:
      return "auth failed";
    case WifiProvisionService::Error::ApNotFound:
      return "ap not found";
    case WifiProvisionService::Error::Timeout:
      return "timeout";
    case WifiProvisionService::Error::Unknown:
      return "unknown";
    default:
      return "none";
  }
}

void renderWifiProvisionDetail(DisplayMonoTft& display, HomePage::Language language,
                               const WifiProvisionService& wifi, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const WifiProvisionService::State state = wifi.state();
  const bool zh = language == HomePage::Language::Zh;

  renderDetailHeader(display, zh ? wifiStateTitleZh(state) : wifiStateTitleEn(state), yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (state == WifiProvisionService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58),
                  zh ? "按OK开启配网热点" : "Press OK to start portal");
  } else if (state == WifiProvisionService::State::PortalReady) {
    char line1[64];
    snprintf(line1, sizeof(line1), "AP: %s", wifi.apSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  zh ? "连接后打开: 192.168.4.1" : "Open: 192.168.4.1");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "网页输入WiFi名和密码" : "Submit SSID and password");
  } else if (state == WifiProvisionService::State::Connecting) {
    char line1[64];
    snprintf(line1, sizeof(line1), zh ? "目标网络: %s" : "Target: %s", wifi.targetSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  zh ? "正在连接，请稍候" : "Connecting, please wait");
  } else if (state == WifiProvisionService::State::Connected) {
    char line1[64];
    char line2[64];
    char line3[64];
    snprintf(line1, sizeof(line1), "SSID: %s", wifi.targetSsid());
    snprintf(line2, sizeof(line2), "PASS: %s", wifi.targetPass());
    snprintf(line3, sizeof(line3), "IP: %s", wifi.staIp());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), line3);
  } else {
    char line1[64];
    snprintf(line1, sizeof(line1), zh ? "失败原因: %s" : "Reason: %s",
             zh ? wifiErrorTextZh(wifi.error()) : wifiErrorTextEn(wifi.error()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  zh ? "按OK重试配网" : "Press OK to retry");
  }

  if (state == WifiProvisionService::State::Connected) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10), zh ? "LEFT: 返回  OK: 重新开启配网热点"
                                                                      : "LEFT: Back  OK: Reopen Portal");
  } else {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  zh ? "LEFT: 返回  OK: 操作" : "LEFT: Back  OK: Action");
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

const char* otaStateTitleEn(OtaService::State state) {
  switch (state) {
    case OtaService::State::Idle:
      return "OTA Update";
    case OtaService::State::Checking:
      return "Checking";
    case OtaService::State::UpToDate:
      return "Up To Date";
    case OtaService::State::UpdateAvailable:
      return "Update Available";
    case OtaService::State::Downloading:
      return "Downloading";
    case OtaService::State::Verifying:
      return "Verifying";
    case OtaService::State::ReadyToApply:
      return "Ready To Apply";
    case OtaService::State::Applying:
      return "Applying";
    default:
      return "Check Failed";
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

const char* otaErrorTextEn(OtaService::Error err) {
  switch (err) {
    case OtaService::Error::None:
      return "none";
    case OtaService::Error::Busy:
      return "busy";
    case OtaService::Error::NoCredentials:
      return "wifi credentials missing";
    case OtaService::Error::ConnectTimeout:
      return "wifi connect timeout";
    case OtaService::Error::HttpRequestFailed:
      return "http request failed";
    case OtaService::Error::HttpStatusInvalid:
      return "manifest http status";
    case OtaService::Error::ManifestInvalid:
      return "manifest invalid";
    case OtaService::Error::ProductMismatch:
      return "product mismatch";
    case OtaService::Error::ChannelMismatch:
      return "channel mismatch";
    case OtaService::Error::VersionNotNew:
      return "version not newer";
    case OtaService::Error::UrlInvalid:
      return "firmware url invalid";
    case OtaService::Error::SizeInvalid:
      return "firmware size invalid";
    case OtaService::Error::ShaInvalid:
      return "sha256 invalid";
    case OtaService::Error::DownloadFailed:
      return "download failed";
    case OtaService::Error::UpdateBeginFailed:
      return "update begin failed";
    case OtaService::Error::UpdateWriteFailed:
      return "update write failed";
    case OtaService::Error::UpdateFinalizeFailed:
      return "update finalize failed";
    case OtaService::Error::ShaMismatch:
      return "sha256 mismatch";
    case OtaService::Error::ApplyNotReady:
      return "apply not ready";
    case OtaService::Error::PostApplyVerifyFailed:
      return "post-apply verify failed";
    default:
      return "unknown";
  }
}

void renderOtaDetail(DisplayMonoTft& display, HomePage::Language language, const OtaService& ota,
                     int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const bool zh = language == HomePage::Language::Zh;

  renderDetailHeader(display, zh ? otaStateTitleZh(ota.state()) : otaStateTitleEn(ota.state()),
                     yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line1[96];
  char line2[96];
  const OtaService::ManifestInfo& manifest = ota.manifest();
  snprintf(line1, sizeof(line1), zh ? "本地版本: %s (%lu)" : "Local: %s (%lu)",
           FirmwareInfo::kVersion, static_cast<unsigned long>(FirmwareInfo::kBuild));

  if (ota.state() == OtaService::State::UpdateAvailable ||
      ota.state() == OtaService::State::UpToDate) {
    snprintf(line2, sizeof(line2), zh ? "远端版本: %s (%lu)" : "Remote: %s (%lu)", manifest.version,
             static_cast<unsigned long>(manifest.build));
  } else {
    snprintf(line2, sizeof(line2), zh ? "远端版本: --" : "Remote: --");
  }

  text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);

  if (ota.state() == OtaService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "按OK检查更新" : "Press OK to check updates");
  } else if (ota.state() == OtaService::State::Checking) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "正在通过WiFi检查manifest" : "Checking manifest via WiFi");
  } else if (ota.state() == OtaService::State::UpToDate) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "当前已是最新版本" : "No update available");
  } else if (ota.state() == OtaService::State::UpdateAvailable) {
    char sizeLine[96];
    snprintf(sizeLine, sizeof(sizeLine), zh ? "包大小: %lu bytes" : "Size: %lu bytes",
             static_cast<unsigned long>(manifest.size));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), sizeLine);
  } else if (ota.state() == OtaService::State::Downloading) {
    char progressLine[96];
    snprintf(progressLine, sizeof(progressLine),
             zh ? "下载进度: %u%% (%lu/%lu)" : "Progress: %u%% (%lu/%lu)",
             static_cast<unsigned>(ota.progressPercent()),
             static_cast<unsigned long>(ota.downloadedBytes()),
             static_cast<unsigned long>(ota.totalBytes()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), progressLine);
  } else if (ota.state() == OtaService::State::Verifying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "正在进行SHA256校验" : "Verifying SHA256");
  } else if (ota.state() == OtaService::State::ReadyToApply) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "固件已写入，重启后生效" : "Firmware ready, reboot to apply");
  } else if (ota.state() == OtaService::State::Applying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  zh ? "正在重启设备..." : "Rebooting device...");
  } else {
    char errLine[96];
    snprintf(errLine, sizeof(errLine), zh ? "错误: %s" : "Error: %s",
             zh ? otaErrorTextZh(ota.error()) : otaErrorTextEn(ota.error()));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), errLine);
  }

  const OtaService::State otaState = ota.state();
  if (otaState == OtaService::State::UpdateAvailable) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  zh ? "LEFT: 返回  OK: 下载" : "LEFT: Back  OK: Download");
  } else if (otaState == OtaService::State::ReadyToApply) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  zh ? "LEFT: 返回  OK: 立即应用" : "LEFT: Back  OK: Apply");
  } else if (otaState == OtaService::State::Downloading ||
             otaState == OtaService::State::Verifying) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  zh ? "LEFT: 返回  OK: 忽略" : "LEFT: Back  OK: Ignore");
  } else {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  zh ? "LEFT: 返回  OK: 检查" : "LEFT: Back  OK: Check");
  }

  if (ota.hasPostApplyResult()) {
    char verifyLine[96];
    snprintf(verifyLine, sizeof(verifyLine), "%s: %s",
             zh ? "上次应用结果" : "Last apply",
             ota.postApplySucceeded() ? (zh ? "成功" : "OK") : (zh ? "失败" : "Failed"));
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 142), verifyLine);
  }
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
  if (sectionFocus == kLanguageItemIndex) {
    return PopupKind::LanguageSelect;
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

uint8_t SettingsPage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  if (isWifiProvisionSelection(homeFocus, sectionFocus) ||
      isOtaSelection(homeFocus, sectionFocus)) {
    return 1U;
  }
  return isAboutDeviceSelection(homeFocus, sectionFocus) ? 2U : 1U;
}

bool SettingsPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                                     uint32_t nowMs, WifiProvisionService& wifi,
                                     OtaService& ota) const {
  if (!okEdge) {
    return false;
  }

  if (isOtaSelection(homeFocus, sectionFocus)) {
    if (ota.state() == OtaService::State::UpdateAvailable) {
      ota.requestDownload(nowMs);
    } else if (ota.state() == OtaService::State::ReadyToApply) {
      ota.requestApply();
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
                                    WifiProvisionService& wifi, OtaService& ota) const {
  if (isOtaSelection(homeFocus, sectionFocus)) {
    ota.cancel();
    return true;
  }

  if (isWifiProvisionSelection(homeFocus, sectionFocus) && wifi.isPortalActive()) {
    wifi.cancelProvision();
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
                                HomePage::Language language, const char* deviceIdText,
                                const char* flashTotalText, const char* sdStatusText,
                                const WifiProvisionService& wifi,
                                const OtaService& ota) const {
  if (isOtaSelection(homeFocus, sectionFocus)) {
    renderOtaDetail(display, language, ota, yOffset);
    return true;
  }

  if (isWifiProvisionSelection(homeFocus, sectionFocus)) {
    renderWifiProvisionDetail(display, language, wifi, yOffset);
    return true;
  }

  if (!isAboutDeviceSelection(homeFocus, sectionFocus)) {
    return false;
  }

  renderAboutDeviceDetail(display, language, detailPageIndex, yOffset, deviceIdText,
                          flashTotalText, sdStatusText, wifi);
  return true;
}

const char* SettingsPage::popupTitle(PopupKind kind, HomePage::Language language) const {
  if (kind == PopupKind::RestartConfirm) {
    return (language == HomePage::Language::Zh) ? "重启设备?" : "Restart Device?";
  }
  if (kind == PopupKind::LanguageSelect) {
    return (language == HomePage::Language::Zh) ? "语言" : "Language";
  }
  return "";
}

const char* SettingsPage::popupPrimaryLabel(PopupKind kind,
                                            HomePage::Language language) const {
  if (kind == PopupKind::RestartConfirm) {
    return (language == HomePage::Language::Zh) ? "是" : "Yes";
  }
  if (kind == PopupKind::LanguageSelect) {
    return "Zh";
  }
  return "";
}

const char* SettingsPage::popupSecondaryLabel(PopupKind kind,
                                              HomePage::Language language) const {
  if (kind == PopupKind::RestartConfirm) {
    return (language == HomePage::Language::Zh) ? "否" : "No";
  }
  if (kind == PopupKind::LanguageSelect) {
    return "En";
  }
  return "";
}
