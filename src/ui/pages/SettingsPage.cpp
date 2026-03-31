#include "SettingsPage.h"

#include "../../bsp/DisplayMonoTft.h"
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
    snprintf(ipText, sizeof(ipText), "IP地址：%s", wifi.staIp());
  } else if (language == HomePage::Language::Zh) {
    snprintf(ipText, sizeof(ipText), "IP地址：未连接，无法获取IP");
  } else {
    snprintf(ipText, sizeof(ipText), "IP: Not connected");
  }

  const char* const kPage0Lines[4] = {"设备名称：My device", "固件版本：V0.01",
                                      deviceIdText, "屏幕尺寸：2.9黑白反射屏"};
  const char* const kPage1Lines[4] = {"IP地址：", "处理器：ESP32 S3", "SD卡容量：",
                                      flashTotalText};
  const char* const kPage1LinesWithIp[4] = {ipText, "处理器：ESP32 S3", sdStatusText,
                                            flashTotalText};
  const int16_t kLineY[4] = {55, 87, 119, 151};
  const char* const* lines = (detailPageIndex == 0) ? kPage0Lines : kPage1LinesWithIp;

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
      return "连接失败";
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
      return "Connected";
    case WifiProvisionService::State::PortalTimeout:
      return "Portal Timeout";
    default:
      return "Failed";
  }
}

const char* wifiErrorTextZh(WifiProvisionService::Error err) {
  switch (err) {
    case WifiProvisionService::Error::InvalidInput:
      return "输入无效";
    case WifiProvisionService::Error::AuthFailed:
      return "密码错误";
    case WifiProvisionService::Error::ApNotFound:
      return "找不到热点";
    case WifiProvisionService::Error::Timeout:
      return "连接超时";
    case WifiProvisionService::Error::Unknown:
      return "未知错误";
    default:
      return "无";
  }
}

void renderWifiProvisionDetail(DisplayMonoTft& display, HomePage::Language language,
                               const WifiProvisionService& wifi, int16_t yOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const WifiProvisionService::State state = wifi.state();

  const char* title = (language == HomePage::Language::Zh) ? wifiStateTitleZh(state)
                                                            : wifiStateTitleEn(state);
  renderDetailHeader(display, title, yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (state == WifiProvisionService::State::Idle) {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58),
                  language == HomePage::Language::Zh ? "按OK开启配网热点"
                                                     : "Press OK to start portal");
  } else if (state == WifiProvisionService::State::PortalReady) {
    char line1[64];
    snprintf(line1, sizeof(line1), "AP: %s", wifi.apSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  language == HomePage::Language::Zh ? "连接后打开: 192.168.4.1"
                                                     : "Open: 192.168.4.1");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114),
                  language == HomePage::Language::Zh ? "网页输入WiFi名和密码"
                                                     : "Submit SSID and password");
  } else if (state == WifiProvisionService::State::Connecting) {
    char line1[64];
    snprintf(line1, sizeof(line1), "%s: %s",
             language == HomePage::Language::Zh ? "目标网络" : "Target", wifi.targetSsid());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  language == HomePage::Language::Zh ? "正在连接，请稍候"
                                                     : "Connecting, please wait");
  } else if (state == WifiProvisionService::State::Connected) {
    char line1[64];
    char line2[64];
    snprintf(line1, sizeof(line1), "SSID: %s", wifi.targetSsid());
    snprintf(line2, sizeof(line2), "IP: %s", wifi.staIp());
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);
  } else {
    char line1[64];
    snprintf(line1, sizeof(line1), "%s: %s",
             language == HomePage::Language::Zh ? "失败原因" : "Reason",
             language == HomePage::Language::Zh ? wifiErrorTextZh(wifi.error()) : "Check AP/pass");
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86),
                  language == HomePage::Language::Zh ? "按OK重新开启配网"
                                                     : "Press OK to retry");
  }

  text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                language == HomePage::Language::Zh ? "LEFT: 返回  OK: 操作"
                                                   : "LEFT: Back  OK: Action");

  if (state == WifiProvisionService::State::Connected) {
    canvas.drawFilledRectangle(8, static_cast<int16_t>(yOffset + height - 22),
                               static_cast<int16_t>(width - 8),
                               static_cast<int16_t>(yOffset + height - 2),
                               ST7305_COLOR_WHITE);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 10),
                  language == HomePage::Language::Zh ? "LEFT: 返回" : "LEFT: Back");
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

bool SettingsPage::isWifiProvisionSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kWifiProvisionItemIndex;
}

uint8_t SettingsPage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  if (isWifiProvisionSelection(homeFocus, sectionFocus)) {
    return 1U;
  }
  return isAboutDeviceSelection(homeFocus, sectionFocus) ? 2U : 1U;
}

bool SettingsPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                                     uint32_t nowMs, WifiProvisionService& wifi) const {
  if (!okEdge || !isWifiProvisionSelection(homeFocus, sectionFocus) || !wifi.canStartPortal()) {
    return false;
  }
  wifi.startPortal(nowMs);
  return true;
}

bool SettingsPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                    WifiProvisionService& wifi) const {
  if (!isWifiProvisionSelection(homeFocus, sectionFocus) || !wifi.isPortalActive()) {
    return false;
  }

  wifi.cancelProvision();
  return true;
}

const SettingsPage::MenuItem* SettingsPage::menuItems() const { return kSettingsItems; }

uint8_t SettingsPage::menuItemCount() const {
  return static_cast<uint8_t>(sizeof(kSettingsItems) / sizeof(kSettingsItems[0]));
}

bool SettingsPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                                int16_t yOffset, DisplayMonoTft& display,
                                HomePage::Language language, const char* deviceIdText,
                                const char* flashTotalText, const char* sdStatusText,
                                const WifiProvisionService& wifi) const {
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
    return "中";
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
