#include "RemotePage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../../services/BluetoothService.h"
#include "../../services/RemoteService.h"

namespace {
constexpr int16_t kDetailHeaderHeight = 28;

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

const char* btStateTextZh(BluetoothService::State state) {
  switch (state) {
    case BluetoothService::State::Advertising:
      return "等待手机连接";
    case BluetoothService::State::Connected:
      return "已连接";
    case BluetoothService::State::Error:
      return "连接异常";
    default:
      return "蓝牙关闭";
  }
}

const char* btStateTextEn(BluetoothService::State state) {
  switch (state) {
    case BluetoothService::State::Advertising:
      return "Waiting";
    case BluetoothService::State::Connected:
      return "Connected";
    case BluetoothService::State::Error:
      return "Error";
    default:
      return "Bluetooth Off";
  }
}

const char* btErrorTextZh(BluetoothService::Error err) {
  switch (err) {
    case BluetoothService::Error::InitFailed:
      return "初始化失败";
    case BluetoothService::Error::StartFailed:
      return "启动失败";
    case BluetoothService::Error::Timeout:
      return "等待超时";
    default:
      return "无";
  }
}

const char* btErrorTextEn(BluetoothService::Error err) {
  switch (err) {
    case BluetoothService::Error::InitFailed:
      return "init failed";
    case BluetoothService::Error::StartFailed:
      return "start failed";
    case BluetoothService::Error::Timeout:
      return "timeout";
    default:
      return "none";
  }
}

const char* remoteStateTextZh(RemoteService::State state) {
  switch (state) {
    case RemoteService::State::Sent:
      return "已发送快门";
    case RemoteService::State::NotConnected:
      return "请先连接蓝牙";
    case RemoteService::State::Busy:
      return "发送过快";
    case RemoteService::State::Ready:
      return "等待触发";
    case RemoteService::State::Error:
      return "发送异常";
    default:
      return "空闲";
  }
}

const char* remoteStateTextEn(RemoteService::State state) {
  switch (state) {
    case RemoteService::State::Sent:
      return "Shutter Sent";
    case RemoteService::State::NotConnected:
      return "Connect Bluetooth First";
    case RemoteService::State::Busy:
      return "Too Fast";
    case RemoteService::State::Ready:
      return "Ready";
    case RemoteService::State::Error:
      return "Send Error";
    default:
      return "Idle";
  }
}
}  // namespace

bool RemotePage::isBluetoothSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kBluetoothConnectItemIndex;
}

bool RemotePage::isRemoteCamSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kBluetoothRemoteCamItemIndex;
}

uint8_t RemotePage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  return (isBluetoothSelection(homeFocus, sectionFocus) ||
          isRemoteCamSelection(homeFocus, sectionFocus))
             ? 1U
             : 0U;
}

bool RemotePage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                                   uint32_t nowMs, BluetoothService& bluetooth,
                                   RemoteService& remote) const {
  if (!okEdge) {
    return false;
  }

  if (isBluetoothSelection(homeFocus, sectionFocus)) {
    if (bluetooth.state() == BluetoothService::State::Advertising ||
        bluetooth.state() == BluetoothService::State::Connected) {
      bluetooth.stop();
    } else {
      (void)bluetooth.start(nowMs);
    }
    return true;
  }

  if (isRemoteCamSelection(homeFocus, sectionFocus)) {
    (void)remote.triggerCameraShutter(nowMs, bluetooth);
    return true;
  }

  return false;
}

bool RemotePage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                                  BluetoothService& bluetooth) const {
  if (!isBluetoothSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (bluetooth.state() == BluetoothService::State::Advertising) {
    bluetooth.stop();
  }
  return false;
}

void RemotePage::handleSectionExit(uint8_t homeFocus, BluetoothService& bluetooth) const {
  if (homeFocus != kHomeIndex) {
    return;
  }
  bluetooth.stop();
}

bool RemotePage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                              DisplayMonoTft& display, HomePage::Language language,
                              const BluetoothService& bluetooth,
                              const RemoteService& remote) const {
  if (!isBluetoothSelection(homeFocus, sectionFocus) &&
      !isRemoteCamSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const bool zh = language == HomePage::Language::Zh;

  renderDetailHeader(display,
                     isBluetoothSelection(homeFocus, sectionFocus)
                         ? (zh ? "蓝牙连接" : "Bluetooth Connect")
                         : (zh ? "蓝牙远程拍照" : "BT Remote Cam"),
                     yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  char line1[64];
  char line2[64];
  char line3[64];
  char line4[64];

  if (isBluetoothSelection(homeFocus, sectionFocus)) {
    snprintf(line1, sizeof(line1), zh ? "设备名: %s" : "Device: %s", bluetooth.deviceName());
    snprintf(line2, sizeof(line2), zh ? "状态: %s" : "State: %s",
             zh ? btStateTextZh(bluetooth.state()) : btStateTextEn(bluetooth.state()));
    snprintf(line3, sizeof(line3), zh ? "错误: %s" : "Error: %s",
             zh ? btErrorTextZh(bluetooth.error()) : btErrorTextEn(bluetooth.error()));

    if (bluetooth.state() == BluetoothService::State::Connected) {
      snprintf(line4, sizeof(line4), "%s",
               zh ? "可返回后进入蓝牙远程拍照" : "Ready for BT Remote Cam");
    } else if (bluetooth.state() == BluetoothService::State::Advertising) {
      snprintf(line4, sizeof(line4), "%s",
               zh ? "请在手机蓝牙中配对本设备" : "Pair from your phone");
    } else {
      snprintf(line4, sizeof(line4), "%s",
               zh ? "按 OK 开始蓝牙广播" : "Press OK to start pairing");
    }
  } else {
    snprintf(line1, sizeof(line1), zh ? "蓝牙状态: %s" : "Bluetooth: %s",
             zh ? btStateTextZh(bluetooth.state()) : btStateTextEn(bluetooth.state()));
    snprintf(line2, sizeof(line2), zh ? "已发送: %lu" : "Sent: %lu",
             static_cast<unsigned long>(remote.triggerCount()));
    snprintf(line3, sizeof(line3), zh ? "结果: %s" : "Result: %s",
             zh ? remoteStateTextZh(remote.state()) : remoteStateTextEn(remote.state()));

    if (bluetooth.state() == BluetoothService::State::Connected) {
      snprintf(line4, sizeof(line4), "%s",
               zh ? "按 OK 发送一次快门事件" : "Press OK to send shutter");
    } else {
      snprintf(line4, sizeof(line4), "%s",
               zh ? "请先进入蓝牙连接完成配对" : "Pair in Bluetooth Connect first");
    }
  }

  text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), line3);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + 142), line4);

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
  return true;
}
