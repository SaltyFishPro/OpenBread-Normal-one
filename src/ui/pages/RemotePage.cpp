#include "RemotePage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../../services/SdBitmapFont.h"
#include "../SdBitmapText.h"
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

void drawThickLine(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x1, int16_t y1,
                   int16_t x2, int16_t y2, uint16_t color) {
  canvas.drawLine(x1, y1, x2, y2, color);
  if (x1 == x2) canvas.drawLine(x1 + 1, y1, x2 + 1, y2, color);
  else if (y1 == y2) canvas.drawLine(x1, y1 + 1, x2, y2 + 1, color);
  else canvas.drawLine(x1 + 1, y1, x2 + 1, y2, color);
}

void drawBluetoothIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t cx, int16_t cy,
                       uint16_t color) {
  drawThickLine(canvas, cx, cy - 31, cx, cy + 31, color);
  drawThickLine(canvas, cx, cy - 31, cx + 19, cy - 14, color);
  drawThickLine(canvas, cx + 19, cy - 14, cx, cy, color);
  drawThickLine(canvas, cx, cy, cx + 19, cy + 14, color);
  drawThickLine(canvas, cx + 19, cy + 14, cx, cy + 31, color);
  drawThickLine(canvas, cx - 15, cy - 20, cx + 19, cy + 20, color);
  drawThickLine(canvas, cx - 15, cy + 20, cx + 19, cy - 20, color);
}

void drawCameraIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t cx, int16_t cy,
                    bool inverted) {
  const uint16_t ink = inverted ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  const uint16_t fill = inverted ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  canvas.drawFilledRectangle(cx - 25, cy - 15, cx + 25, cy + 15, fill);
  canvas.drawRectangle(cx - 23, cy - 13, cx + 23, cy + 14, ink);
  canvas.drawRectangle(cx - 22, cy - 12, cx + 22, cy + 13, ink);
  canvas.drawRectangle(cx - 11, cy - 20, cx + 4, cy - 14, ink);
  canvas.drawRectangle(cx - 10, cy - 19, cx + 3, cy - 14, ink);
  canvas.drawCircle(cx, cy, 9, ink);
  canvas.drawCircle(cx, cy, 8, ink);
}

void drawPagerIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t cx, int16_t cy,
                   RemoteService::Feedback feedback, uint8_t frame) {
  canvas.drawRectangle(cx - 21, cy - 27, cx + 20, cy + 26, ST7305_COLOR_WHITE);
  canvas.drawRectangle(cx - 20, cy - 26, cx + 19, cy + 25, ST7305_COLOR_WHITE);
  canvas.drawRectangle(cx - 10, cy - 17, cx + 10, cy - 15, ST7305_COLOR_WHITE);
  canvas.drawLine(cx - 9, cy + 2, cx + 9, cy + 2, ST7305_COLOR_WHITE);
  canvas.drawLine(cx - 9, cy + 12, cx + 9, cy + 12, ST7305_COLOR_WHITE);

  if (feedback != RemoteService::Feedback::PagerUp &&
      feedback != RemoteService::Feedback::PagerDown) return;
  const int16_t travel = static_cast<int16_t>((frame % 4U) * 3U);
  const int16_t arrowY = feedback == RemoteService::Feedback::PagerUp
                             ? static_cast<int16_t>(cy + 10 - travel)
                             : static_cast<int16_t>(cy - 10 + travel);
  const int16_t tipY = feedback == RemoteService::Feedback::PagerUp
                           ? static_cast<int16_t>(arrowY - 8)
                           : static_cast<int16_t>(arrowY + 8);
  canvas.drawLine(cx + 30, arrowY, cx + 30, tipY, ST7305_COLOR_WHITE);
  if (feedback == RemoteService::Feedback::PagerUp) {
    canvas.drawLine(cx + 30, tipY, cx + 25, tipY + 5, ST7305_COLOR_WHITE);
    canvas.drawLine(cx + 30, tipY, cx + 35, tipY + 5, ST7305_COLOR_WHITE);
  } else {
    canvas.drawLine(cx + 30, tipY, cx + 25, tipY - 5, ST7305_COLOR_WHITE);
    canvas.drawLine(cx + 30, tipY, cx + 35, tipY - 5, ST7305_COLOR_WHITE);
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
                              const RemoteService& remote, SdBitmapFont* titleFont) const {
  if (!isBluetoothSelection(homeFocus, sectionFocus) &&
      !isRemoteCamSelection(homeFocus, sectionFocus) && !isPagerSelection(homeFocus, sectionFocus)) {
    return false;
  }

  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const char* title = isBluetoothSelection(homeFocus, sectionFocus)
                          ? "蓝牙连接"
                          : (isRemoteCamSelection(homeFocus, sectionFocus) ? "蓝牙拍照" : "翻页器");
  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + height - 1), ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);

  const bool portraitRemotePage = isPortraitSelection(homeFocus, sectionFocus) && width < height;
  auto drawCentered = [&](const char* value, int16_t baseline) {
    const int16_t textWidth = static_cast<int16_t>(text.getUTF8Width(value));
    const int16_t x = (width - textWidth) / 2;
    text.drawUTF8(x < 4 ? 4 : x, static_cast<int16_t>(yOffset + baseline), value);
  };
  auto drawTitle = [&](const char* value, int16_t x, int16_t baseline, bool centered) {
    const int16_t top = yOffset < 0 ? 0 : yOffset;
    const SdBitmapText::Clip clip = {0, top, static_cast<int16_t>(width - 1),
                                     static_cast<int16_t>(height - 1)};
    if (titleFont && titleFont->state() == SdBitmapFont::State::Ready) {
      const int16_t titleWidth = static_cast<int16_t>(SdBitmapText::measureUTF8(*titleFont, value));
      const int16_t drawX = centered ? static_cast<int16_t>((width - titleWidth) / 2) : x;
      SdBitmapText::drawUTF8(canvas, *titleFont, drawX,
                             static_cast<int16_t>(yOffset + baseline), value,
                             ST7305_COLOR_WHITE, clip);
      return;
    }
    if (centered) x = static_cast<int16_t>((width - text.getUTF8Width(value)) / 2);
    text.drawUTF8(x, static_cast<int16_t>(yOffset + baseline), value);
  };

  if (!portraitRemotePage) {
    text.drawUTF8(16, static_cast<int16_t>(yOffset + 22), "无线功能");
    const char* category = "蓝牙";
    text.drawUTF8(static_cast<int16_t>(width - 16 - text.getUTF8Width(category)),
                  static_cast<int16_t>(yOffset + 22), category);
    canvas.drawLine(16, static_cast<uint>(yOffset + 31), width - 16,
                    static_cast<uint>(yOffset + 31), ST7305_COLOR_WHITE);
    drawBluetoothIcon(canvas, 70, static_cast<int16_t>(yOffset + 80), ST7305_COLOR_WHITE);
    canvas.drawLine(122, static_cast<uint>(yOffset + 48), 122,
                    static_cast<uint>(yOffset + 125), ST7305_COLOR_WHITE);
    drawTitle(title, 144, 62, false);
    text.drawUTF8(144, static_cast<int16_t>(yOffset + 89), btStateTextZh(bluetooth.state()));
    text.drawUTF8(144, static_cast<int16_t>(yOffset + 112), bluetooth.deviceName());
    canvas.drawLine(16, static_cast<uint>(yOffset + height - 25), width - 16,
                    static_cast<uint>(yOffset + height - 25), ST7305_COLOR_WHITE);
    text.drawUTF8(16, static_cast<int16_t>(yOffset + height - 8),
                  bluetooth.state() == BluetoothService::State::Connected
                      ? "OK 停止广播" : "OK 开始广播");
    const char* back = "LEFT 返回";
    text.drawUTF8(static_cast<int16_t>(width - 16 - text.getUTF8Width(back)),
                  static_cast<int16_t>(yOffset + height - 8), back);
  } else {
    drawTitle(title, 0, 25, true);
    canvas.drawLine(16, static_cast<uint>(yOffset + 35), width - 16,
                    static_cast<uint>(yOffset + 35), ST7305_COLOR_WHITE);
    drawCentered(bluetooth.isConnected() ? "已连接" : "未连接", 148);
    if (isRemoteCamSelection(homeFocus, sectionFocus)) {
      const bool flash = remote.feedback() == RemoteService::Feedback::Camera &&
                         remote.feedbackFrame() < 3U;
      drawCameraIcon(canvas, static_cast<int16_t>(width / 2),
                     static_cast<int16_t>(yOffset + 92), flash);
      char sent[24];
      snprintf(sent, sizeof(sent), "已发送 %lu",
               static_cast<unsigned long>(remote.triggerCount()));
      drawCentered(sent, 178);
      drawCentered(bluetooth.isConnected() ? "OK 拍照" : "请先连接蓝牙", 310);
    } else {
      drawPagerIcon(canvas, static_cast<int16_t>(width / 2),
                    static_cast<int16_t>(yOffset + 90), remote.feedback(), remote.feedbackFrame());
      char counts[32];
      snprintf(counts, sizeof(counts), "翻页 ↑%lu ↓%lu",
               static_cast<unsigned long>(remote.pagerUpCount()),
               static_cast<unsigned long>(remote.pagerDownCount()));
      drawCentered(counts, 178);
      snprintf(counts, sizeof(counts), "暂停 %lu",
               static_cast<unsigned long>(remote.pagerPauseCount()));
      drawCentered(counts, 202);
      drawCentered(bluetooth.isConnected() ? "↑↓ 上下滑动" : "请先连接蓝牙", 300);
      drawCentered("OK 点按", 326);
    }
    canvas.drawLine(16, static_cast<uint>(yOffset + height - 25), width - 16,
                    static_cast<uint>(yOffset + height - 25), ST7305_COLOR_WHITE);
    text.drawUTF8(16, static_cast<int16_t>(yOffset + height - 8), "LEFT 返回");
  }
  return true;
}
