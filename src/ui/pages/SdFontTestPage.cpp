#include "SdFontTestPage.h"

#include "../../bsp/DisplayMonoTft.h"
#include "../../services/SdBitmapFont.h"
#include "../../services/SdCardService.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../SdBitmapText.h"

namespace {
constexpr uint16_t kSizes[] = {14, 20, 24};
constexpr const char* kLabels[] = {"14 px", "20 px", "24 px"};
const char* errorText(SdBitmapFont::Error error) {
  switch (error) {
    case SdBitmapFont::Error::OpenFailed: return "找不到对应字号的字库";
    case SdBitmapFont::Error::SizeMismatch: return "字库字号与文件名不一致";
    case SdBitmapFont::Error::Header: return "字库格式不正确";
    case SdBitmapFont::Error::Read: return "读取失败，请检查SD卡";
    case SdBitmapFont::Error::Crc: return "字库不完整，请重新复制";
    case SdBitmapFont::Error::Glyph: return "字形数据不正确";
    default: return "请选择字号后重试";
  }
}
}

void SdFontTestPage::open(SdBitmapFont& font, SdCardService& sd, bool refreshCard) {
  if (refreshCard || sdUnavailable_) {
    font.end();  // A retry/new card must not reuse previous verification results.
    sdUnavailable_ = sd.refresh() != SdCardService::Status::Ready;
  }
  if (!sdUnavailable_) (void)font.open(kSizes[sizeIndex_]);
}

void SdFontTestPage::begin(SdBitmapFont& font, SdCardService& sd) {
  sizeIndex_ = 0;
  open(font, sd, true);
}

bool SdFontTestPage::handleInput(bool up, bool down, bool ok,
                                SdBitmapFont& font, SdCardService& sd) {
  if (!up && !down && !ok) return false;
  if (up) sizeIndex_ = static_cast<uint8_t>((sizeIndex_ + 2U) % 3U);
  else if (down) sizeIndex_ = static_cast<uint8_t>((sizeIndex_ + 1U) % 3U);
  open(font, sd, !up && !down && ok);
  return true;
}

void SdFontTestPage::render(DisplayMonoTft& display, int16_t yOffset, SdBitmapFont& font) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  DetailHeader::render(display, "SD字库测试", yOffset);
  for (uint8_t i = 0; i < 3U; ++i) {
    const int16_t x = 16 + i * 88;
    const bool selected = i == sizeIndex_;
    const uint16_t fg = selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
    const uint16_t bg = selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
    if (selected) DrawUtils::drawClippedFilledRect(canvas, x - 4, yOffset + 34,
                                                  x + 70, yOffset + 53, bg);
    text.setForegroundColor(fg);
    text.setBackgroundColor(bg);
    text.drawUTF8(x, yOffset + 49, kLabels[i]);
  }
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  if (!sdUnavailable_ && font.state() == SdBitmapFont::State::Ready) {
    const SdBitmapText::Clip clip = {16, static_cast<int16_t>(yOffset + 56),
                                     static_cast<int16_t>(display.width() - 17),
                                     static_cast<int16_t>(yOffset + 140)};
    SdBitmapText::drawUTF8(canvas, font, 16, yOffset + 84, "你好，面包！Normal",
                           ST7305_COLOR_BLACK, clip);
    SdBitmapText::drawUTF8(canvas, font, 16, yOffset + 110, "0123456789 Aa",
                           ST7305_COLOR_BLACK, clip);
    SdBitmapText::drawUTF8(canvas, font, 16, yOffset + 136, "Unicode \xF0\xA0\xAE\xB7 \xC3\xA9",
                           ST7305_COLOR_BLACK, clip);
  } else {
    const char* status = sdUnavailable_ ? "未检测到可用SD卡" :
        font.state() == SdBitmapFont::State::Checking ? "正在检查字库，请稍候" : errorText(font.error());
    text.drawUTF8(16, yOffset + 89, status);
    text.drawUTF8(16, yOffset + 119, "字库请放入 Font 文件夹");
  }
  canvas.drawFastHLine(16, yOffset + 144, display.width() - 32, ST7305_COLOR_BLACK);
  text.drawUTF8(16, yOffset + 163, "Up/Down 字号  OK 重试  Left 返回");
}
