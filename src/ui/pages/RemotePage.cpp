#include "RemotePage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../../services/BluetoothService.h"
#include "../../services/RemoteService.h"

namespace {
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

const char* btStateShortTextZh(BluetoothService::State state) {
  switch (state) {
    case BluetoothService::State::Advertising:
      return "等待连接";
    case BluetoothService::State::Connected:
      return "已连接";
    case BluetoothService::State::Error:
      return "连接异常";
    default:
      return "关闭";
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

const char* remoteStateShortTextZh(RemoteService::State state) {
  switch (state) {
    case RemoteService::State::Sent:
      return "已发送";
    case RemoteService::State::NotConnected:
      return "未连接";
    case RemoteService::State::Busy:
      return "发送过快";
    case RemoteService::State::Ready:
      return "待触发";
    case RemoteService::State::Error:
      return "发送异常";
    default:
      return "空闲";
  }
}

}  // namespace

bool RemotePage::isBluetoothSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kBluetoothConnectItemIndex;
}

bool RemotePage::isRemoteCamSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kBluetoothRemoteCamItemIndex;
}

bool RemotePage::isPagerSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kBluetoothPagerItemIndex;
}

bool RemotePage::isPortraitSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isRemoteCamSelection(homeFocus, sectionFocus) ||
         isPagerSelection(homeFocus, sectionFocus);
}

uint8_t RemotePage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  return (isBluetoothSelection(homeFocus, sectionFocus) ||
          isRemoteCamSelection(homeFocus, sectionFocus) || isPagerSelection(homeFocus, sectionFocus))
             ? 1U
             : 0U;
}

bool RemotePage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool upEdge,
                                   bool downEdge, bool okEdge,
                                   uint32_t nowMs, BluetoothService& bluetooth,
                                   RemoteService& remote) const {
  if (isPagerSelection(homeFocus, sectionFocus)) {
    if (upEdge) {
      (void)remote.triggerPagerUp(nowMs, bluetooth);
      return true;
    }
    if (downEdge) {
      (void)remote.triggerPagerDown(nowMs, bluetooth);
      return true;
    }
    if (okEdge) {
      (void)remote.triggerPagerPause(nowMs, bluetooth);
      return true;
    }
    return false;
  }

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
                              DisplayMonoTft& display, const BluetoothService& bluetooth,
                              const RemoteService& remote) const {
  if (!isBluetoothSelection(homeFocus, sectionFocus) &&
      !isRemoteCamSelection(homeFocus, sectionFocus) && !isPagerSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  DetailHeader::render(display,
                       isBluetoothSelection(homeFocus, sectionFocus)
                           ? "蓝牙连接"
                           : (isRemoteCamSelection(homeFocus, sectionFocus) ? "蓝牙拍照" : "翻页器"),
                       yOffset);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  const bool portraitRemotePage = isPortraitSelection(homeFocus, sectionFocus) && width < height;

  char line1[64];
  char line2[64];
  char line3[64];
  char line4[64];

  if (isBluetoothSelection(homeFocus, sectionFocus)) {
    snprintf(line1, sizeof(line1), "设备名: %s", bluetooth.deviceName());
    snprintf(line2, sizeof(line2), "状态: %s", btStateTextZh(bluetooth.state()));
    snprintf(line3, sizeof(line3), "错误: %s", btErrorTextZh(bluetooth.error()));

    if (bluetooth.state() == BluetoothService::State::Connected) {
      snprintf(line4, sizeof(line4), "%s", "LEFT 返回后进入蓝牙拍照");
    } else if (bluetooth.state() == BluetoothService::State::Advertising) {
      snprintf(line4, sizeof(line4), "%s", "手机蓝牙配对本设备    LEFT 返回");
    } else {
      snprintf(line4, sizeof(line4), "%s", "OK 开始广播    LEFT 返回");
    }
  } else if (isRemoteCamSelection(homeFocus, sectionFocus)) {
    snprintf(line1, sizeof(line1), "蓝牙状态: %s", btStateTextZh(bluetooth.state()));
    snprintf(line2, sizeof(line2), "已发送: %lu",
             static_cast<unsigned long>(remote.triggerCount()));
    snprintf(line3, sizeof(line3), "结果: %s", remoteStateTextZh(remote.state()));

    if (bluetooth.state() == BluetoothService::State::Connected) {
      snprintf(line4, sizeof(line4), "%s", "OK 拍照    LEFT 返回");
    } else {
      snprintf(line4, sizeof(line4), "%s", "请先进入蓝牙连接完成配对    LEFT 返回");
    }
  } else {
    snprintf(line1, sizeof(line1), "蓝牙状态: %s", btStateTextZh(bluetooth.state()));
    snprintf(line2, sizeof(line2), "翻页: ↑%lu ↓%lu",
             static_cast<unsigned long>(remote.pagerUpCount()),
             static_cast<unsigned long>(remote.pagerDownCount()));
    snprintf(line3, sizeof(line3), "暂停: %lu", static_cast<unsigned long>(remote.pagerPauseCount()));
    snprintf(line4, sizeof(line4), "%s", bluetooth.state() == BluetoothService::State::Connected
                                               ? "UP/DOWN 翻页    OK 暂停"
                                               : "请先进入蓝牙连接完成配对");
  }

  if (portraitRemotePage) {
    char portraitLine1[32];
    char portraitLine2[32];
    char portraitLine3[32];
    snprintf(portraitLine1, sizeof(portraitLine1), "状态: %s",
             btStateShortTextZh(bluetooth.state()));
    if (isPagerSelection(homeFocus, sectionFocus)) {
      snprintf(portraitLine2, sizeof(portraitLine2), "翻页 ↑%lu ↓%lu",
               static_cast<unsigned long>(remote.pagerUpCount()),
               static_cast<unsigned long>(remote.pagerDownCount()));
      snprintf(portraitLine3, sizeof(portraitLine3), "暂停: %lu",
               static_cast<unsigned long>(remote.pagerPauseCount()));
    } else {
      snprintf(portraitLine2, sizeof(portraitLine2), "已发送: %lu",
               static_cast<unsigned long>(remote.triggerCount()));
      snprintf(portraitLine3, sizeof(portraitLine3), "结果: %s",
               remoteStateShortTextZh(remote.state()));
    }

    auto drawCentered = [&](const char* value, int16_t baseline) {
      const int16_t textWidth = static_cast<int16_t>(text.getUTF8Width(value));
      const int16_t x = (width - textWidth) / 2;
      text.drawUTF8(x < 4 ? 4 : x, static_cast<int16_t>(yOffset + baseline), value);
    };

    drawCentered(portraitLine1, 84);
    drawCentered(portraitLine2, 138);
    drawCentered(portraitLine3, 192);
    drawCentered(isPagerSelection(homeFocus, sectionFocus) ? "UP/DOWN 翻页" : "OK 拍照",
                 isPagerSelection(homeFocus, sectionFocus) ? 314 : 332);
    if (isPagerSelection(homeFocus, sectionFocus)) {
      drawCentered("OK 暂停", 346);
    }
    drawCentered("LEFT 返回", isPagerSelection(homeFocus, sectionFocus) ? 378 : 366);
  } else {
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 58), line1);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 86), line2);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 114), line3);
    text.drawUTF8(8, static_cast<int16_t>(yOffset + 142), line4);
  }

  canvas.drawRectangle(4, static_cast<int16_t>(yOffset + 32), width - 5,
                       static_cast<int16_t>(yOffset + height - 5), ST7305_COLOR_BLACK);
  return true;
}
