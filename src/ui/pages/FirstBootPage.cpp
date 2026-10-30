#include "FirstBootPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../../services/FontSetupService.h"
#include "../../services/TimeService.h"
#include "../../services/WifiProvisionService.h"
#include "../IconBitmap.h"
#include "../DrawUtils.h"
#include "../assets/BootFontData.h"
#include "HomePage.h"

namespace {
using State = FontSetupService::State;

const char* const kLabels[] = {"SD卡", "字库", "网络", "设置"};
const char* const kWeekdays[] = {"日", "一", "二", "三", "四", "五", "六"};

struct Copy {
  const char* title;
  const char* detail;
  const char* action;
  const char* footer;
};

Copy copyFor(const FontSetupService& setup) {
  switch (setup.state()) {
    case State::Checking: return {"正在检查SD卡", "请稍候", "", ""};
    case State::NeedCard: return {"请插入SD卡", "插入后按OK重新检查", "OK 重新检查", ""};
    case State::NeedFonts: return {"需要下载字体包", "SD卡可用  缺少必要文件", "OK 确认", ""};
    case State::Portal: return {"连接设备热点", "", "", "192.168.4.1"};
    case State::Connecting: return {"正在连接WiFi", "连接成功后将开始下载", "", ""};
    case State::Downloading: return {"正在下载字体包", "", "", "请保持供电及SD卡连接"};
    case State::Verifying: return {"正在校验字体包", "检查完成后将自动重启", "", ""};
    case State::Restarting: return {"字体已准备就绪", "检查完成后将自动重启", "", ""};
    case State::Error:
      if (setup.error() == FontSetupService::Error::NoSpace)
        return {"下载未完成", "SD卡空间不足  按OK重试", "OK 重试", ""};
      if (setup.error() == FontSetupService::Error::Write)
        return {"下载未完成", "SD卡写入失败  按OK重试", "OK 重试", ""};
      if (setup.error() == FontSetupService::Error::Hash)
        return {"下载未完成", "字体包验证失败  按OK重试", "OK 重试", ""};
      return {"下载未完成", "网络中断  按OK重试", "OK 重试", ""};
    case State::Ready: return {"正在进入主菜单", "SD卡和字体文件已通过检查", "", ""};
  }
  return {"", "", "", ""};
}

uint8_t categoryFor(State state) {
  if (state == State::Ready) return 3;
  if (state == State::NeedCard || state == State::Checking) return 0;
  if (state == State::NeedFonts || state == State::Verifying ||
      state == State::Restarting) return 1;
  return 2;
}

const BootFontData::Glyph* glyphFor(uint32_t cp, bool large) {
  if (large) {
    for (const auto& glyph : BootFontData::kGlyphs20)
      if (glyph.codepoint == cp) return &glyph;
  } else {
    for (const auto& glyph : BootFontData::kGlyphs14)
      if (glyph.codepoint == cp) return &glyph;
  }
  return nullptr;
}

int16_t drawText(DisplayMonoTft& display, int16_t x, int16_t baseline,
                 const char* value, bool large) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  text.setFont(large ? u8g2_font_10x20_tf : u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  for (const auto* p = reinterpret_cast<const uint8_t*>(value); *p;) {
    uint32_t cp = 0;
    if (*p < 0x80U) {
      cp = *p++;
    } else if ((*p & 0xE0U) == 0xC0U && p[1]) {
      cp = ((p[0] & 0x1FU) << 6U) | (p[1] & 0x3FU);
      p += 2;
    } else if ((*p & 0xF0U) == 0xE0U && p[1] && p[2]) {
      cp = ((p[0] & 0x0FU) << 12U) | ((p[1] & 0x3FU) << 6U) | (p[2] & 0x3FU);
      p += 3;
    } else {
      ++p;
      continue;
    }
    if (cp < 128U) {
      char ascii[] = {static_cast<char>(cp), 0};
      text.drawUTF8(x, baseline, ascii);
      x += text.getUTF8Width(ascii);
      continue;
    }
    const auto* glyph = glyphFor(cp, large);
    if (!glyph) { x += large ? 20 : 14; continue; }
    const uint8_t* bitmap = (large ? BootFontData::kBitmap20 : BootFontData::kBitmap14) +
                            glyph->offset;
    const uint8_t stride = (glyph->width + 7U) / 8U;
    for (uint8_t row = 0; row < glyph->height; ++row) {
      const int16_t y = baseline + glyph->yOffset + row;
      if (y < 0 || y >= display.height()) continue;
      for (uint8_t col = 0; col < glyph->width; ++col) {
        const int16_t px = x + glyph->xOffset + col;
        if (px < 0 || px >= display.width()) continue;
        if (bitmap[row * stride + col / 8U] & (0x80U >> (col % 8U)))
          canvas.drawFastHLine(px, y, 1, ST7305_COLOR_WHITE);
      }
    }
    x += glyph->advance;
  }
  return x;
}

int16_t textWidth(DisplayMonoTft& display, const char* value) {
  auto& text = display.text();
  text.setFont(u8g2_font_7x14_tf);
  int16_t width = 0;
  for (const auto* p = reinterpret_cast<const uint8_t*>(value); *p;) {
    if (*p < 128U) {
      char ascii[] = {static_cast<char>(*p++), 0};
      width += text.getUTF8Width(ascii);
      continue;
    }
    if ((*p & 0xE0U) == 0xC0U && p[1]) { p += 2; continue; }
    if ((*p & 0xF0U) != 0xE0U || !p[1] || !p[2]) { ++p; continue; }
    const uint32_t cp = ((p[0] & 0x0FU) << 12U) | ((p[1] & 0x3FU) << 6U) |
                        (p[2] & 0x3FU);
    const auto* glyph = glyphFor(cp, false);
    width += glyph ? glyph->advance : 14;
    p += 3;
  }
  return width;
}
}  // namespace

void FirstBootPage::render(DisplayMonoTft& display, const FontSetupService& setup,
                           const WifiProvisionService& wifi, const TimeService& time,
                           uint8_t batteryPercent, bool batteryValid) const {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(0, 0, display.width() - 1, display.height() - 1,
                             ST7305_COLOR_BLACK);
  const auto& snapshot = time.snapshot();
  char line[52];
  if (batteryValid) {
    snprintf(line, sizeof(line), "%u%%", static_cast<unsigned>(batteryPercent));
    drawText(display, 12, 17, line, false);
  }
  if (snapshot.valid) {
    snprintf(line, sizeof(line), "%02u:%02u", snapshot.now.hour, snapshot.now.minute);
    drawText(display, 170, 17, line, false);
    snprintf(line, sizeof(line), "%u月%u日 周%s", snapshot.now.month, snapshot.now.day,
             kWeekdays[snapshot.now.weekday % 7U]);
    drawText(display, 372 - textWidth(display, line), 17, line, false);
  }

  const uint8_t focus = categoryFor(setup.state());
  const uint8_t order[] = {focus, static_cast<uint8_t>((focus + 1U) % 4U),
                           static_cast<uint8_t>((focus + 2U) % 4U)};
  for (uint8_t i = 0; i < 3U; ++i) {
    const int16_t centerX = 48 + 70 * i;
    const int16_t size = i == 0 ? 42 : 30;
    const uint8_t icon = order[i];
    const uint16_t frame = icon == 0 ? 1U : 0U;
    IconBitmap::drawFrame(canvas, HomePage::bootIcon(icon), frame, centerX - size / 2,
                          29 + (42 - size) / 2, size, size,
                          true, 29, 85, i != 0);
    if (i == 0) {
      drawText(display, centerX - textWidth(display, kLabels[icon]) / 2,
               83, kLabels[icon], false);
      canvas.drawFastHLine(centerX - 10, 87, 20, ST7305_COLOR_WHITE);
    }
  }
  canvas.drawFastHLine(14, 87, 356, ST7305_COLOR_WHITE);

  const Copy copy = copyFor(setup);
  drawText(display, 18, 116, copy.title, true);
  const char* detail = copy.detail;
  if (setup.state() == State::Portal) detail = wifi.apSsid();
  if (setup.state() == State::Downloading) {
    const uint32_t tenths = static_cast<uint32_t>(
        (static_cast<uint64_t>(FontBundle::kTotalBytes) * setup.progressPercent() * 10U) /
        (100U * 1000000U));
    snprintf(line, sizeof(line), "%lu.%lu / 9.6 MB  %u%%",
             static_cast<unsigned long>(tenths / 10U),
             static_cast<unsigned long>(tenths % 10U), setup.progressPercent());
    detail = line;
  }
  drawText(display, 18, 140, detail, false);
  if (copy.footer[0]) drawText(display, 18, 160, copy.footer, false);
  if (copy.action[0]) {
    const int16_t width = textWidth(display, copy.action);
    const int16_t left = 368 - width - 12;
    DrawUtils::drawRoundRect(canvas, left, 143, width + 12, 23, 11,
                             ST7305_COLOR_BLACK, ST7305_COLOR_WHITE);
    drawText(display, left + 6, 159, copy.action, false);
  }
  if (setup.state() == State::Downloading) {
    for (int16_t x = 18; x < 366; x += 4)
      canvas.drawFastHLine(x, 166, 1, ST7305_COLOR_WHITE);
    const int16_t filled = 348 * setup.progressPercent() / 100U;
    if (filled > 0) canvas.drawFastHLine(18, 165, filled, ST7305_COLOR_WHITE);
  }
}
