#include "DeviceAdminPage.h"
#include "DeviceAdminIcons.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DrawUtils.h"
#include "../SdUiText.h"
#include <qrcode.h>
#include <cstring>
#include <cstdio>

namespace {
uint8_t qrRows[29][4] = {};
int qrSize = 0;
char cachedSsid[33] = {};
void captureQr(esp_qrcode_handle_t qr) {
  qrSize = esp_qrcode_get_size(qr);
  if (qrSize <= 0 || qrSize > 29) { qrSize = 0; return; }
  memset(qrRows, 0, sizeof(qrRows));
  for (int y=0; y<qrSize; ++y) for (int x=0; x<qrSize; ++x)
    if (esp_qrcode_get_module(qr,x,y)) qrRows[y][x/8] |= 0x80U >> (x%8);
}
bool prepareQr(const char* ssid) {
  if (qrSize && strcmp(ssid,cachedSsid)==0) return true;
  char payload[80]; snprintf(payload,sizeof(payload),"WIFI:T:nopass;S:%s;;",ssid);
  esp_qrcode_config_t config = {}; config.display_func = captureQr;
  config.max_qrcode_version = 3; config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  qrSize = 0;
  if (esp_qrcode_generate(&config,payload) != ESP_OK || !qrSize) return false;
  snprintf(cachedSsid,sizeof(cachedSsid),"%s",ssid); return true;
}
void drawIcon(ST7305_2p9_BW_DisplayDriver& canvas, int x, int y, const uint8_t* bits, int size) {
  const int stride = (size + 7) / 8;
  for (int row = 0; row < size; ++row) {
    if (y + row < 0 || y + row >= canvas.getDisplayHeight()) continue;
    for (int col = 0; col < size;) {
      if (!(bits[row * stride + col / 8] & (0x80U >> (col % 8)))) { ++col; continue; }
      const int start = col++;
      while (col < size && (bits[row * stride + col / 8] & (0x80U >> (col % 8)))) ++col;
      DrawUtils::drawClippedFilledRect(canvas, x + start, y + row, x + col - 1,
                                      y + row, ST7305_COLOR_WHITE);
    }
  }
}
void rightText(DisplayMonoTft& display, int baseline, const char* text) {
  display.text().drawUTF8(368 - display.text().getUTF8Width(text), baseline, text);
}
void backHint(DisplayMonoTft& display, int off, const char* text) {
  for (int row = 0; row < 7; ++row) {
    const int x = 17 + (row < 4 ? 3 - row : row - 3);
    DrawUtils::drawClippedFilledRect(display.canvas(), x, off + 145 + row,
                                    x, off + 145 + row, ST7305_COLOR_WHITE);
  }
  display.text().drawUTF8(26, off + 154, text);
}
void okHint(DisplayMonoTft& display, int off, const char* action) {
  auto& t = display.text();
  const int actionX = 368 - t.getUTF8Width(action);
  const int keyWidth = t.getUTF8Width("OK") + 12;
  const int keyX = actionX - 5 - keyWidth;
  DrawUtils::drawRoundRect(display.canvas(), keyX, off + 137, keyWidth, 23, 4,
                          ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
  t.drawUTF8(keyX + 6, off + 154, "OK");
  t.drawUTF8(actionX, off + 154, action);
}
}
void DeviceAdminPage::renderView(DisplayMonoTft& display, int16_t off, const View& view,
                             SdBitmapFont* ui20, SdBitmapFont* ui24) {
  auto& c=display.canvas(); auto& t=display.text();
  DrawUtils::drawClippedFilledRect(c,0,off,display.width()-1,off+display.height()-1,ST7305_COLOR_BLACK);
  t.setFont(u8g2_font_7x14_tf); t.setFontMode(1); t.setForegroundColor(ST7305_COLOR_WHITE); t.setBackgroundColor(ST7305_COLOR_BLACK);
  drawIcon(c,16,off+10,DeviceAdminIcons::kMiniDevice,16);
  t.drawUTF8(40,off+23,"设备管理");
  if (view.active && !view.updating) {
    rightText(display,off+23,"热点");
    const SdBitmapText::Clip clip = {16, static_cast<int16_t>(off+34), 252, static_cast<int16_t>(off+61)};
    SdUiText::draw(display,ui20,16,off+57,"扫码连接设备热点",ST7305_COLOR_WHITE,clip);
    t.drawUTF8(16,off+82,view.ssid);
    t.drawUTF8(16,off+102,"http://192.168.4.1");
    char line[40]; snprintf(line,sizeof(line),"验证码 %.3s %.3s",view.code,
        strlen(view.code)>=3 ? view.code+3 : ""); t.drawUTF8(16,off+122,line);
    if (view.confirm) t.drawUTF8(16,off+154,"OK 确认关闭    左 取消");
    else { backHint(display,off,"关闭管理"); rightText(display,off+154,view.status); }
    if (prepareQr(view.ssid)) {
      constexpr int size=100, x=268, padding=4, content=size-2*padding;
      const int units=qrSize+8;
      int y=off+32;
      DrawUtils::drawClippedFilledRect(c,x,y,x+size-1,y+size-1,ST7305_COLOR_WHITE);
      for (int row=0;row<qrSize;++row) for(int col=0;col<qrSize;++col)
        if(qrRows[row][col/8] & (0x80U>>(col%8))) {
          const int left=x+padding+(col+4)*content/units;
          const int right=x+padding+(col+5)*content/units-1;
          const int top=y+padding+(row+4)*content/units;
          const int bottom=y+padding+(row+5)*content/units-1;
          DrawUtils::drawClippedFilledRect(c,left,top,right,bottom,ST7305_COLOR_BLACK);
        }
    }
  } else {
    const uint8_t* icon=DeviceAdminIcons::kDeviceIcon;
    const char* title="设备管理";
    const char* subtitle="开启热点，用手机管理";
    if (view.updating) {
      icon=DeviceAdminIcons::kUpdateIcon; title="正在更新"; subtitle="请保持电源连接";
    } else if (view.starting) {
      icon=DeviceAdminIcons::kLoadingIcon; title="正在开启热点"; subtitle="请稍候";
    } else if (view.failed) {
      icon=DeviceAdminIcons::kWarningIcon; title="热点开启失败"; subtitle="请重试";
    } else if (view.closed) {
      icon=DeviceAdminIcons::kDoneIcon; title="管理已关闭"; subtitle="热点与 WiFi 已关闭";
    }
    drawIcon(c,16,off+48,icon,76);
    const SdBitmapText::Clip clip = {112, static_cast<int16_t>(off+50), 368, static_cast<int16_t>(off+91)};
    SdUiText::draw(display,ui24,112,off+81,title,ST7305_COLOR_WHITE,clip);
    t.drawUTF8(112,off+109,subtitle);
    if (view.updating) { t.drawUTF8(16,off+154,"更新完成后自动重启"); return; }
    backHint(display,off,view.starting ? "取消" : "返回");
    if (!view.starting) okHint(display,off,view.failed ? "重试" : view.closed ? "重新开启" : "开启");
  }
}
