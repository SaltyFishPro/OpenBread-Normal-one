#include "MusicPage.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "../../bsp/DisplayMonoTft.h"
#include "../../log/Log.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"
#include "../IconBitmap.h"
#include "../TextUtils.h"
#include "../SdUiText.h"
#include "../assets/musicapp/music_nav_icons.h"
#include "../PopupView.h"

namespace {
// 音乐页动画触发日志：排查"底部导航栏动画跑到屏幕上方"这类问题时，用来确认
// 究竟是标签切换动画还是进入播放器动画在跑。排查完成后可置 0 关闭。

constexpr int16_t kPlayerBarX = 9;
constexpr int16_t kPlayerBarY = 112;
constexpr int16_t kPlayerBarWidth = 365;
constexpr int16_t kPlayerBarHeight = 50;
constexpr int16_t kPlayerBarRadius = 44;
constexpr int16_t kTabY = 121;
constexpr int16_t kTabWidth = 78;
constexpr int16_t kTabHeight = 32;
constexpr int16_t kTabRadius = 43;
constexpr int16_t kTextBaselineY = 143;
constexpr int16_t kListAreaX = 0;
constexpr int16_t kListAreaY = 0;
constexpr int16_t kListAreaWidth = 384;
constexpr int16_t kListAreaHeight = 112;
constexpr int16_t kListTextX = 14;
constexpr int16_t kListTopBaselineY = 25;
constexpr int16_t kListRowHeight = 26;
constexpr int16_t kListTextHeight = 14;
constexpr int16_t kListFocusPadX = 10;
constexpr int16_t kListFocusPadY = 5;
constexpr int16_t kListFocusRadius = 8;
constexpr int16_t kListTextMaxRight = 350;
constexpr int16_t kListProgressX = 366;
constexpr int16_t kListProgressY = 12;
constexpr int16_t kListProgressW = 4;
constexpr int16_t kListProgressH = 88;
constexpr uint32_t kScrollPeriodMs = 260;
constexpr int16_t kScrollHoldPx = 16;
constexpr uint32_t kAnimationDurationMs = 220;
constexpr uint32_t kListFocusAnimationDurationMs = 170;
constexpr uint32_t kPlayerTransitionDurationMs = 460;
constexpr uint32_t kPlayerTransitionMinDurationMs = 80;
constexpr uint32_t kAnimationFrameIntervalMs = 33;
// 弹窗动画的帧间隔，比常规动画更密以保证进场/退场顺滑。
constexpr uint32_t kPopupFrameIntervalMs = 16;
constexpr int8_t kVolumeStep = 1;
constexpr uint8_t kTabCount = 4;
constexpr float kPlayerTransitionPhase1End = 0.56f;
constexpr int16_t kPlayerCompactX = 324;
constexpr int16_t kPlayerCompactY = 112;
constexpr int16_t kPlayerCompactWidth = 50;
constexpr int16_t kPlayerCompactHeight = 50;
constexpr int16_t kPlayerSideX = 324;
constexpr int16_t kPlayerSideY = 6;
constexpr int16_t kPlayerSideWidth = 50;
constexpr int16_t kPlayerSideHeight = 156;
constexpr uint8_t kPlayerControlCount = 4;
constexpr int16_t kPlayerControlSlotH = kPlayerSideHeight / kPlayerControlCount;
constexpr int16_t kPlayerControlFocusW = 34;
constexpr int16_t kPlayerControlFocusH = 28;
constexpr int16_t kPlayerControlFocusRadius = 9;
constexpr int16_t kPlayerAreaX = 0;
constexpr int16_t kPlayerAreaY = 0;
constexpr int16_t kPlayerAreaWidth = 324;
constexpr int16_t kPlayerAreaHeight = 168;
constexpr int16_t kPlayerTitleX = 0;
constexpr int16_t kPlayerTitleY = 0;
constexpr int16_t kPlayerTitleWidth = 154;
constexpr int16_t kPlayerTitleHeight = 22;
constexpr int16_t kPlayerTitleBaselineY = 16;
constexpr int16_t kAudioOutputX = 231;
constexpr int16_t kAudioOutputY = 0;
constexpr int16_t kAudioOutputWidth = 90;
constexpr int16_t kAudioOutputHeight = 22;
constexpr int16_t kAudioOutputIconX = 238;
constexpr int16_t kAudioOutputIconY = 4;
constexpr int16_t kAudioOutputLabelX = 260;
constexpr int16_t kAudioOutputLabelBaselineY = 16;
constexpr int16_t kSpectrumX = 3;
constexpr int16_t kSpectrumY = 25;
constexpr int16_t kSpectrumWidth = 318;
constexpr int16_t kSpectrumHeight = 69;
constexpr uint8_t kSpectrumBarCount = 64;
constexpr int16_t kPlaybackProgressX = 3;
constexpr int16_t kPlaybackProgressY = 99;
constexpr int16_t kPlaybackProgressWidth = 318;
constexpr int16_t kPlaybackProgressHeight = 5;
constexpr int16_t kCurrentTimeX = 3;
constexpr int16_t kTimeTopY = 109;
constexpr int16_t kTimeBaselineY = 120;
constexpr int16_t kTotalTimeX = 293;
constexpr int16_t kLyricX = 36;
constexpr int16_t kLyricY = 107;
constexpr int16_t kLyricWidth = 252;
constexpr int16_t kLyricHeight = 55;
constexpr int16_t kLyricTextHeight = 12;
constexpr int16_t kLyricBaselineFromTop = 12;
constexpr int16_t kLyricRowStep = 13;
constexpr uint32_t kTitleScrollPeriodMs = 90;
constexpr uint32_t kLyricSwitchPeriodMs = 1800;
enum class NavTab : uint8_t {
  Music = 0,
  Favorite,
  Volume,
  Info,
};

const char* const kLabelsZh[] = {"音乐", "收藏", "音量", "详情"};

float clamp01(float value) { return AnimMath::clamp01(value); }

float easeOutCubic(float t) { return AnimMath::easeOutCubic(t); }

void clearMusicNavArea(DisplayMonoTft& display, int16_t yOffset) {
  auto& canvas = display.canvas();
  const int16_t navTop = static_cast<int16_t>(yOffset + kPlayerBarY);
  const int16_t navBottom =
      static_cast<int16_t>(yOffset + kPlayerBarY + kPlayerBarHeight - 1);
  canvas.drawFilledRectangle(0, navTop, static_cast<int16_t>(display.width() - 1), navBottom,
                             ST7305_COLOR_WHITE);
}

void clearMusicListArea(DisplayMonoTft& display) {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(kListAreaX, kListAreaY,
                             static_cast<int16_t>(kListAreaX + kListAreaWidth - 1),
                             static_cast<int16_t>(kListAreaY + kListAreaHeight - 1),
                             ST7305_COLOR_WHITE);
}

int16_t lerpInt(int16_t start, int16_t end, float t) {
  return AnimMath::lerpInt16Rounded(start, end, t);
}

int16_t easeOutCubicInt(int16_t start, int16_t end, float t) {
  return AnimMath::easeOutCubicRoundedInt16(start, end, t);
}

int16_t translatedRightEdge(int16_t xOffset, int16_t localRight) {
  return static_cast<int16_t>(xOffset + localRight);
}

const char* listStateText(MusicService::ListState state) {
  switch (state) {
    case MusicService::ListState::Scanning:
      return "正在扫描音乐...";
    case MusicService::ListState::SdMissing:
      return "SD卡未插入";
    case MusicService::ListState::Empty:
      return "未找到音乐文件";
    case MusicService::ListState::Error:
      return "SD卡读取失败";
    default:
      return "";
  }
}

int16_t tabX(uint8_t index) {
  const int16_t totalGap = kPlayerBarWidth - kTabCount * kTabWidth;
  const int16_t gapSlots = kTabCount + 1;
  return static_cast<int16_t>(kPlayerBarX +
                              (((int16_t)index + 1) * totalGap + gapSlots / 2) / gapSlots +
                              (int16_t)index * kTabWidth);
}

int16_t tabCenterX(uint8_t index) {
  return static_cast<int16_t>(tabX(index) + kTabWidth / 2);
}

const uint8_t* iconForTab(uint8_t index, bool favoriteEnabled) {
  switch (index % kTabCount) {
    case static_cast<uint8_t>(NavTab::Music):
      return music_nav_icon_music;
    case static_cast<uint8_t>(NavTab::Favorite):
      return favoriteEnabled ? music_nav_icon_favorite_filled : music_nav_icon_favorite_outline;
    case static_cast<uint8_t>(NavTab::Volume):
      return music_nav_icon_volume;
    default:
      return music_nav_icon_info;
  }
}

void drawBitmapIcon(ST7305_2p9_BW_DisplayDriver& canvas, const uint8_t* bitmap, int16_t centerX,
                    int16_t centerY, uint16_t fg) {
  const int16_t left = static_cast<int16_t>(centerX - MUSIC_NAV_ICON_WIDTH / 2);
  const int16_t top = static_cast<int16_t>(centerY - MUSIC_NAV_ICON_HEIGHT / 2);
  const uint8_t stride = (MUSIC_NAV_ICON_WIDTH + 7) / 8;

  for (int16_t y = 0; y < MUSIC_NAV_ICON_HEIGHT; ++y) {
    int16_t runStart = -1;
    for (int16_t x = 0; x < MUSIC_NAV_ICON_WIDTH; ++x) {
      const uint8_t value = pgm_read_byte(&bitmap[y * stride + x / 8]);
      const bool enabled = (value & (1U << (x % 8))) != 0;
      if (enabled && runStart < 0) {
        runStart = x;
      } else if (!enabled && runStart >= 0) {
        canvas.drawFastHLine(static_cast<int16_t>(left + runStart), static_cast<int16_t>(top + y),
                             static_cast<int16_t>(x - runStart), fg);
        runStart = -1;
      }
    }
    if (runStart >= 0) {
      canvas.drawFastHLine(static_cast<int16_t>(left + runStart), static_cast<int16_t>(top + y),
                           static_cast<int16_t>(MUSIC_NAV_ICON_WIDTH - runStart), fg);
    }
  }
}

void drawRevealedLabel(DisplayMonoTft& display, uint8_t index, int16_t yOffset,
                       float progress) {
  constexpr float kTextStartProgress = 0.45f;
  if (progress < kTextStartProgress) {
    return;
  }

  auto& text = display.text();
  const char* label = kLabelsZh[index % kTabCount];
  if (index % kTabCount == static_cast<uint8_t>(NavTab::Info)) {
    label = "播放";
  }
  const uint8_t labelChars = TextUtils::utf8CharCount(label);
  const float textProgress = clamp01((progress - kTextStartProgress) / (1.0f - kTextStartProgress));
  uint8_t visibleChars =
      static_cast<uint8_t>(1 + static_cast<uint8_t>((labelChars - 1) * textProgress + 0.5f));
  visibleChars = min<uint8_t>(visibleChars, labelChars);

  const char* suffix = TextUtils::utf8SuffixByChars(label, visibleChars);
  const int16_t finalRight = static_cast<int16_t>(tabX(index) + kTabWidth - 9);
  text.setFont(u8g2_font_7x14_tf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  const int16_t textWidth = text.getUTF8Width(suffix);
  text.drawUTF8(static_cast<int16_t>(finalRight - textWidth),
                static_cast<int16_t>(yOffset + kTextBaselineY), suffix);
}

void drawMusicNavFrame(DisplayMonoTft& display, uint8_t selectedIndex, bool favoriteEnabled,
                       int16_t yOffset, float) {
  auto& canvas=display.canvas(); auto& text=display.text();
  text.setFont(u8g2_font_7x14_tf); text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK); text.setBackgroundColor(ST7305_COLOR_WHITE);
  DrawUtils::drawClippedFilledRect(canvas,16,yOffset+128,367,yOffset+128,ST7305_COLOR_BLACK);
  if(selectedIndex==0) {
    text.drawUTF8(16,yOffset+153,"< 返回");
    text.drawUTF8(165,yOffset+153,"> 工具");
    text.drawUTF8(291,yOffset+153,"OK 播放");
    return;
  }
  text.drawUTF8(16,yOffset+153,"< 返回列表");
  const char* labels[]={favoriteEnabled?"已收藏":"收藏","音量"};
  for(uint8_t i=0;i<2;++i) {
    const int16_t x=175+i*91;
    const bool selected=selectedIndex==i+1;
    if(selected)DrawUtils::drawRoundRect(canvas,x,yOffset+135,82,24,12,ST7305_COLOR_BLACK,ST7305_COLOR_BLACK);
    text.setForegroundColor(selected?ST7305_COLOR_WHITE:ST7305_COLOR_BLACK);
    const int16_t w=text.getUTF8Width(labels[i]);
    text.drawUTF8(x+(82-w)/2,yOffset+152,labels[i]);
  }
}

void makeMusicListLine(const MusicService::Page& page, uint8_t row, char* line,
                       size_t lineSize) {
  const uint32_t absolute =
      static_cast<uint32_t>(page.pageIndex) * MusicService::kRowsPerPage + row + 1U;
  snprintf(line, lineSize, "%03lu %s", static_cast<unsigned long>(absolute),
           page.tracks[row].name);
}

int16_t musicListBaselineY(uint8_t row) {
  return static_cast<int16_t>(kListTopBaselineY + row * kListRowHeight);
}

void musicListFocusRect(U8G2_FOR_ST73XX& text, const char* value, int16_t baselineY,
                        int16_t xOffset, int16_t& x, int16_t& y, int16_t& width,
                        int16_t& height) {
  const int16_t textWidth = text.getUTF8Width(value);
  const int16_t maxTextWidth = static_cast<int16_t>(kListTextMaxRight - kListTextX);
  int16_t boxTextWidth = textWidth;
  if (boxTextWidth > maxTextWidth) {
    boxTextWidth = maxTextWidth;
  }

  x = static_cast<int16_t>(xOffset + kListTextX - kListFocusPadX);
  y = static_cast<int16_t>(baselineY - kListTextHeight - kListFocusPadY);
  width = static_cast<int16_t>(boxTextWidth + kListFocusPadX * 2);
  height = static_cast<int16_t>(kListTextHeight + kListFocusPadY * 2 + 1);
}

void drawClippedText(DisplayMonoTft& display, const char* value, int16_t x, int16_t baselineY,
                     int16_t maxRight, int16_t selectedMaskRight, int16_t areaRight,
                     bool selected, uint32_t nowMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t textWidth = text.getUTF8Width(value);
  const int16_t rowTop = static_cast<int16_t>(baselineY - 14);
  const int16_t rowBottom = static_cast<int16_t>(baselineY + 3);
  int16_t drawX = x;

  if (selected && textWidth > maxRight - x) {
    const int16_t overflow = static_cast<int16_t>(textWidth - (maxRight - x) + kScrollHoldPx);
    if (overflow + kScrollHoldPx > 0) {
      const int16_t phase = static_cast<int16_t>((nowMs / kScrollPeriodMs) %
                                                 static_cast<uint16_t>(overflow + kScrollHoldPx));
      drawX = static_cast<int16_t>(x - (phase > kScrollHoldPx ? phase - kScrollHoldPx : 0));
    }
  }

  text.drawUTF8(drawX, baselineY, value);
  if (selected) {
    if (maxRight <= selectedMaskRight) {
      canvas.drawFilledRectangle(maxRight, rowTop, selectedMaskRight, rowBottom,
                                 ST7305_COLOR_BLACK);
    }
    if (selectedMaskRight < areaRight) {
      canvas.drawFilledRectangle(static_cast<int16_t>(selectedMaskRight + 1), rowTop,
                                 areaRight, rowBottom, ST7305_COLOR_WHITE);
    }
  } else {
    canvas.drawFilledRectangle(maxRight, rowTop, areaRight, rowBottom, ST7305_COLOR_WHITE);
  }
}

void drawMusicListProgress(DisplayMonoTft& display, const MusicService& music,
                           uint32_t selectedAbsolute, int16_t xOffset) {
  auto& canvas = display.canvas();
  const int16_t progressX = static_cast<int16_t>(xOffset + kListProgressX);
  canvas.drawRectangle(progressX, kListProgressY,
                       static_cast<int16_t>(progressX + kListProgressW - 1),
                       static_cast<int16_t>(kListProgressY + kListProgressH - 1),
                       ST7305_COLOR_BLACK);

  int16_t thumbH = kListProgressH;
  if (music.trackCount() > 0) {
    thumbH = static_cast<int16_t>(kListProgressH / music.trackCount());
  }
  if (thumbH < 4) {
    thumbH = 4;
  }
  if (thumbH > kListProgressH) {
    thumbH = kListProgressH;
  }

  int16_t thumbY = kListProgressY;
  if (music.trackCount() > 1 && kListProgressH > thumbH) {
    const int16_t travel = static_cast<int16_t>(kListProgressH - thumbH);
    thumbY = static_cast<int16_t>(
        kListProgressY +
        (static_cast<int32_t>(travel) * selectedAbsolute) / (music.trackCount() - 1U));
  }

  canvas.drawFilledRectangle(progressX, thumbY,
                             static_cast<int16_t>(progressX + kListProgressW - 1),
                             static_cast<int16_t>(thumbY + thumbH - 1), ST7305_COLOR_BLACK);
}

void drawMusicList(DisplayMonoTft& display, const MusicService& music, uint16_t pageIndex,
                   uint8_t rowIndex, uint32_t nowMs, bool, uint8_t, uint8_t,
                   uint32_t, int16_t xOffset, SdBitmapFont* font) {
  auto& c=display.canvas();auto& t=display.text();
  DrawUtils::drawClippedFilledRect(c,xOffset,0,xOffset+383,127,ST7305_COLOR_WHITE);
  t.setFont(u8g2_font_7x14_tf);t.setFontMode(1);
  t.setForegroundColor(ST7305_COLOR_BLACK);t.setBackgroundColor(ST7305_COLOR_WHITE);
  t.drawUTF8(xOffset+16,22,"音乐");
  char count[24];snprintf(count,sizeof(count),"%lu 首",static_cast<unsigned long>(music.trackCount()));
  t.drawUTF8(xOffset+368-t.getUTF8Width(count),21,count);
  DrawUtils::drawClippedFilledRect(c,xOffset+16,29,xOffset+367,29,ST7305_COLOR_BLACK);
  if(music.listState()!=MusicService::ListState::Ready) {
    const char* msg=listStateText(music.listState());
    t.drawUTF8(TextUtils::centeredTextXInBox(t,msg,xOffset,384),77,msg);return;
  }
  if(!SdUiText::ready(font,32)) {
    const bool failed=font && font->state()==SdBitmapFont::State::Error;
    const char* msg=failed?"字体加载失败":"正在加载音乐列表...";
    t.drawUTF8(TextUtils::centeredTextXInBox(t,msg,xOffset,384),77,msg);
    if(failed)t.drawUTF8(xOffset+100,103,"OK 重试  Left 返回");
    return;
  }
  const auto& page=music.page();
  for(uint8_t i=0;i<page.count;++i) {
    const int16_t top=35+i*30;const bool selected=i==rowIndex;
    if(selected)DrawUtils::drawRoundRect(c,xOffset+16,top,352,27,13,ST7305_COLOR_BLACK,ST7305_COLOR_BLACK);
    char name[MusicService::kMaxNameLen];snprintf(name,sizeof(name),"%s",page.tracks[i].name);
    char* dot=strrchr(name,'.');if(dot && dot!=name)*dot=0;
    const uint16_t color=selected?ST7305_COLOR_WHITE:ST7305_COLOR_BLACK;
    const SdBitmapText::Clip clip={static_cast<int16_t>(xOffset+28),static_cast<int16_t>(top+2),
                                  static_cast<int16_t>(xOffset+320),static_cast<int16_t>(top+24)};
    int16_t x=xOffset+28;
    const int32_t textWidth=SdBitmapText::measureUTF8(*font,name);
    if(selected && textWidth>290) {
      const uint32_t overflow=static_cast<uint32_t>(textWidth-290);
      const uint32_t phase=(nowMs/70U)%(overflow+30U);
      x-=phase>15?static_cast<int16_t>(std::min<uint32_t>(overflow,phase-15)):0;
    }
    SdBitmapText::drawUTF8(c,*font,x,top+19,name,color,clip);
    t.setForegroundColor(color);t.setBackgroundColor(selected?ST7305_COLOR_BLACK:ST7305_COLOR_WHITE);
    if(music.currentTrack().path[0] && music.currentTrackIndex()==uint32_t(pageIndex)*MusicService::kRowsPerPage+i)
      t.drawUTF8(xOffset+335,top+19,"*");
    else if(selected)t.drawUTF8(xOffset+336,top+19,">");
  }
}

int16_t playerControlCenterY(uint8_t index) {
  return static_cast<int16_t>(kPlayerSideY + kPlayerControlSlotH * index +
                              kPlayerControlSlotH / 2);
}

const uint8_t* playerControlIconFor(uint8_t index, bool playing, bool shuffle) {
  switch (index % kPlayerControlCount) {
    case 0:
      return playing ? music_player_icon_pause : music_player_icon_play;
    case 1:
      return shuffle ? music_player_icon_shuffle : music_player_icon_sequential;
    case 2:
      return music_player_icon_previous;
    default:
      return music_player_icon_next;
  }
}

void drawPlayerSideControls(DisplayMonoTft& display, uint8_t selectedControl, bool playing,
                            bool shuffle) {
  auto& canvas = display.canvas();
  const int16_t centerX = static_cast<int16_t>(kPlayerSideX + kPlayerSideWidth / 2);

  for (uint8_t i = 0; i < kPlayerControlCount; ++i) {
    const int16_t centerY = playerControlCenterY(i);
    const bool selected = i == selectedControl;
    if (selected) {
      DrawUtils::drawRoundRect(canvas,
                               static_cast<int16_t>(centerX - kPlayerControlFocusW / 2),
                               static_cast<int16_t>(centerY - kPlayerControlFocusH / 2),
                               kPlayerControlFocusW, kPlayerControlFocusH,
                               kPlayerControlFocusRadius, ST7305_COLOR_WHITE,
                               ST7305_COLOR_WHITE);
    }
    drawBitmapIcon(canvas, playerControlIconFor(i, playing, shuffle), centerX, centerY,
                   selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  }
}

uint8_t pseudoSpectrumHeight(uint8_t index, uint32_t frame) {
  const uint8_t waveA = static_cast<uint8_t>((index * 7U + frame * 3U) % 23U);
  const uint8_t waveB = static_cast<uint8_t>(((index + 11U) * (index + 3U) + frame * 5U) % 31U);
  uint8_t value = static_cast<uint8_t>(4U + (waveA + waveB) / 2U);
  if ((index % 9U) == (frame % 9U)) {
    value = static_cast<uint8_t>(value + 24U);
  } else if ((index % 13U) == ((frame + 4U) % 13U)) {
    value = static_cast<uint8_t>(value + 14U);
  }
  if (index < 8U || index > kSpectrumBarCount - 9U) {
    value = static_cast<uint8_t>(value + 8U);
  }
  return min<uint8_t>(value, static_cast<uint8_t>(kSpectrumHeight - 8));
}

void drawMaskedScrollingText(DisplayMonoTft& display, const char* value, int16_t x, int16_t y,
                             int16_t width, int16_t height, int16_t baselineY,
                             uint32_t nowMs, uint32_t periodMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  canvas.drawFilledRectangle(x, y, static_cast<int16_t>(x + width - 1),
                             static_cast<int16_t>(y + height - 1), ST7305_COLOR_WHITE);
  const int16_t textWidth = text.getUTF8Width(value);
  int16_t drawX = x;
  if (textWidth > width) {
    const int16_t overflow = static_cast<int16_t>(textWidth - width + kScrollHoldPx * 2);
    const int16_t phase =
        static_cast<int16_t>((nowMs / periodMs) % static_cast<uint16_t>(overflow));
    drawX = static_cast<int16_t>(x - (phase > kScrollHoldPx ? phase - kScrollHoldPx : 0));
  }

  text.drawUTF8(drawX, baselineY, value);
  if (x > 0) {
    canvas.drawFilledRectangle(0, y, static_cast<int16_t>(x - 1),
                               static_cast<int16_t>(y + height - 1), ST7305_COLOR_WHITE);
  }
  if (x + width < kPlayerAreaWidth) {
    canvas.drawFilledRectangle(static_cast<int16_t>(x + width), y,
                               static_cast<int16_t>(kPlayerAreaWidth - 1),
                               static_cast<int16_t>(y + height - 1), ST7305_COLOR_WHITE);
  }
}

uint8_t interpolatedSpectrumValue(const uint8_t* bands, uint8_t index) {
  constexpr uint8_t kBands = MusicService::kSpectrumBandCount;
  const uint16_t position =
      static_cast<uint16_t>((static_cast<uint32_t>(index) * (kBands - 1U) * 256U) /
                            (kSpectrumBarCount - 1U));
  uint8_t lo = static_cast<uint8_t>(position >> 8U);
  const uint8_t frac = static_cast<uint8_t>(position & 0xFFU);
  if (lo >= kBands) {
    lo = static_cast<uint8_t>(kBands - 1U);
  }
  const uint8_t hi = min<uint8_t>(static_cast<uint8_t>(lo + 1U),
                                  static_cast<uint8_t>(kBands - 1U));
  const uint16_t mixed =
      static_cast<uint16_t>(bands[lo]) * static_cast<uint16_t>(256U - frac) +
      static_cast<uint16_t>(bands[hi]) * frac;
  return static_cast<uint8_t>((mixed + 128U) >> 8U);
}

uint8_t spectrumBarHeight(uint8_t value, uint8_t index, uint32_t frame, bool playing,
                          bool hasSpectrum) {
  if (!playing) {
    return static_cast<uint8_t>(max<uint8_t>(3, pseudoSpectrumHeight(index, 4U) / 3U));
  }
  if (!hasSpectrum) {
    return static_cast<uint8_t>(max<uint8_t>(3, pseudoSpectrumHeight(index, frame) / 2U));
  }

  const uint16_t maxH = static_cast<uint16_t>(kSpectrumHeight - 8);
  uint16_t height = 3U + (static_cast<uint16_t>(value) * maxH) / 255U;
  if (value > 192U) {
    height = static_cast<uint16_t>(height + 2U);
  }
  return static_cast<uint8_t>(min<uint16_t>(maxH, max<uint16_t>(3U, height)));
}

void drawPlayerSpectrum(DisplayMonoTft& display, const MusicService& music, uint32_t nowMs,
                        bool playing) {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(kSpectrumX, kSpectrumY,
                             static_cast<int16_t>(kSpectrumX + kSpectrumWidth - 1),
                             static_cast<int16_t>(kSpectrumY + kSpectrumHeight - 1),
                             ST7305_COLOR_WHITE);

  const uint32_t frame = playing ? (nowMs / 85U) : 4U;
  const uint8_t* spectrum = music.spectrumBands();
  bool hasSpectrum = false;
  for (uint8_t i = 0; i < MusicService::kSpectrumBandCount; ++i) {
    if (spectrum[i] > 0U) {
      hasSpectrum = true;
      break;
    }
  }
  const int16_t baseY = static_cast<int16_t>(kSpectrumY + kSpectrumHeight - 2);
  for (uint8_t i = 0; i < kSpectrumBarCount; ++i) {
    const int16_t x = static_cast<int16_t>(
        kSpectrumX + (static_cast<int32_t>(i) * kSpectrumWidth) / kSpectrumBarCount);
    const int16_t nextX = static_cast<int16_t>(
        kSpectrumX + (static_cast<int32_t>(i + 1U) * kSpectrumWidth) / kSpectrumBarCount);
    const int16_t cellW = static_cast<int16_t>(max<int16_t>(1, nextX - x));
    const int16_t barW = static_cast<int16_t>(max<int16_t>(1, cellW - 2));
    const uint8_t value = interpolatedSpectrumValue(spectrum, i);
    const uint8_t h = spectrumBarHeight(value, i, frame, playing, hasSpectrum);
    canvas.drawFilledRectangle(x, static_cast<int16_t>(baseY - h + 1),
                               static_cast<int16_t>(x + barW - 1), baseY,
                               ST7305_COLOR_BLACK);
    if (playing && hasSpectrum && value > 112U) {
      const int16_t dotY = static_cast<int16_t>(baseY - h - 3 - (value > 184U ? 2 : 0));
      if (dotY >= kSpectrumY) {
        canvas.drawFilledRectangle(x, dotY, static_cast<int16_t>(x + barW - 1),
                                   static_cast<int16_t>(dotY + 1), ST7305_COLOR_BLACK);
      }
    }
  }
}

void drawPlayerProgress(DisplayMonoTft& display, uint32_t currentSeconds,
                        uint32_t durationSeconds) {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(kPlaybackProgressX, kPlaybackProgressY,
                             static_cast<int16_t>(kPlaybackProgressX + kPlaybackProgressWidth - 1),
                             static_cast<int16_t>(kPlaybackProgressY + kPlaybackProgressHeight - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawRectangle(kPlaybackProgressX, kPlaybackProgressY,
                       static_cast<int16_t>(kPlaybackProgressX + kPlaybackProgressWidth - 1),
                       static_cast<int16_t>(kPlaybackProgressY + kPlaybackProgressHeight - 1),
                       ST7305_COLOR_BLACK);

  const int16_t travel = static_cast<int16_t>(kPlaybackProgressWidth - 1);
  if (durationSeconds > 0 && currentSeconds > durationSeconds) {
    currentSeconds = durationSeconds;
  }
  const int16_t dotX =
      durationSeconds == 0
          ? kPlaybackProgressX
          : static_cast<int16_t>(
                kPlaybackProgressX +
                (static_cast<uint32_t>(travel) * currentSeconds) / durationSeconds);
  if (dotX > kPlaybackProgressX) {
    canvas.drawFastHLine(static_cast<int16_t>(kPlaybackProgressX + 1),
                         static_cast<int16_t>(kPlaybackProgressY + 2),
                         static_cast<int16_t>(dotX - kPlaybackProgressX), ST7305_COLOR_BLACK);
  }
  canvas.drawFilledCircle(dotX, static_cast<int16_t>(kPlaybackProgressY + 2), 3,
                          ST7305_COLOR_BLACK);
}

void formatTime(uint32_t seconds, char* out, size_t outSize) {
  snprintf(out, outSize, "%lu:%02lu", static_cast<unsigned long>(seconds / 60U),
           static_cast<unsigned long>(seconds % 60U));
}

void formatDurationTime(uint32_t seconds, char* out, size_t outSize) {
  if (seconds == 0) {
    snprintf(out, outSize, "--:--");
    return;
  }
  formatTime(seconds, out, outSize);
}

void drawPlayerTimes(DisplayMonoTft& display, uint32_t currentSeconds,
                     uint32_t durationSeconds) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  canvas.drawFilledRectangle(kCurrentTimeX, kTimeTopY, 34,
                             static_cast<int16_t>(kTimeTopY + 12), ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(kTotalTimeX, kTimeTopY,
                             static_cast<int16_t>(kPlayerAreaWidth - 1),
                             static_cast<int16_t>(kTimeTopY + 12), ST7305_COLOR_WHITE);
  text.setFont(u8g2_font_6x12_mf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (durationSeconds > 0 && currentSeconds > durationSeconds) {
    currentSeconds = durationSeconds;
  }
  char current[10];
  char total[10];
  formatTime(currentSeconds, current, sizeof(current));
  formatDurationTime(durationSeconds, total, sizeof(total));
  text.drawUTF8(kCurrentTimeX, kTimeBaselineY, current);
  text.drawUTF8(kTotalTimeX, kTimeBaselineY, total);
}

void splitLyricLines(U8G2_FOR_ST73XX& text, const char* lyric, char lines[][64],
                     uint8_t& lineCount) {
  lineCount = 0;
  size_t lineLen = 0;
  size_t lastBreakLen = 0;
  size_t lastBreakSource = 0;
  const size_t length = strlen(lyric);
  size_t sourceStart = 0;

  for (size_t i = 0; i <= length && lineCount < 3; ++i) {
    const char c = lyric[i];
    const bool atEnd = c == '\0';
    const bool breakable = c == ' ' || c == '/' || atEnd;
    if (!atEnd && lineLen < 63) {
      lines[lineCount][lineLen++] = c;
      lines[lineCount][lineLen] = '\0';
    }
    if (breakable) {
      lastBreakLen = lineLen;
      lastBreakSource = i + 1;
    }
    if (atEnd || text.getUTF8Width(lines[lineCount]) > kLyricWidth) {
      if (atEnd) {
        ++lineCount;
        break;
      }
      size_t cutLen = lastBreakLen > 0 ? lastBreakLen : lineLen;
      size_t nextStart = lastBreakSource > sourceStart ? lastBreakSource : i;
      while (cutLen > 0 && lines[lineCount][cutLen - 1] == ' ') {
        --cutLen;
      }
      lines[lineCount][cutLen] = '\0';
      ++lineCount;
      if (lineCount >= 3) {
        break;
      }
      sourceStart = nextStart;
      i = sourceStart == 0 ? 0 : sourceStart - 1;
      lineLen = 0;
      lastBreakLen = 0;
      lastBreakSource = sourceStart;
      lines[lineCount][0] = '\0';
    }
  }

  if (lineCount == 0) {
    strcpy(lines[0], "");
    lineCount = 1;
  }
}

void drawCenteredLyricText(DisplayMonoTft& display, const char* value) {
  auto& text = display.text();
  char lines[3][64] = {};
  uint8_t lineCount = 0;
  splitLyricLines(text, value, lines, lineCount);
  const int16_t blockH = static_cast<int16_t>(lineCount * kLyricRowStep);
  const int16_t baselineY = static_cast<int16_t>(
      kLyricY + (kLyricHeight - blockH) / 2 + kLyricBaselineFromTop);

  for (uint8_t i = 0; i < lineCount; ++i) {
    const int16_t lineX = TextUtils::centeredTextXInBox(text, lines[i], kLyricX, kLyricWidth);
    text.drawUTF8(lineX, static_cast<int16_t>(baselineY + i * kLyricRowStep), lines[i]);
  }
}

void drawPlayerLyrics(DisplayMonoTft& display, const MusicService& music, uint32_t nowMs,
                      uint32_t currentSeconds, bool playing) {
  static const char* const kLyrics[] = {
      "Follow the light / across the quiet night",
      "Every beat is moving through the screen",
      "Hold this moment / let the music breathe",
  };
  auto& canvas = display.canvas();
  auto& text = display.text();
  canvas.drawFilledRectangle(kLyricX, kLyricY,
                             static_cast<int16_t>(kLyricX + kLyricWidth - 1),
                             static_cast<int16_t>(kLyricY + kLyricHeight - 1),
                             ST7305_COLOR_WHITE);

  const uint8_t lyricIndex =
      playing ? static_cast<uint8_t>((currentSeconds / 4U) % 3U) : 1U;
  text.setFont(u8g2_font_6x12_mf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (music.playbackState() == MusicService::PlaybackState::Error) {
    char debug[96];
    if (music.audioDebugText()[0] != '\0') {
      snprintf(debug, sizeof(debug), "%s / %s", music.errorText(), music.audioDebugText());
    } else {
      snprintf(debug, sizeof(debug), "%s", music.errorText());
    }
    drawCenteredLyricText(display, debug);
    return;
  }

  char lines[3][64] = {};
  uint8_t lineCount = 0;
  splitLyricLines(text, kLyrics[lyricIndex], lines, lineCount);
  const int16_t blockH = static_cast<int16_t>(lineCount * kLyricRowStep);
  int16_t baselineY = static_cast<int16_t>(
      kLyricY + (kLyricHeight - blockH) / 2 + kLyricBaselineFromTop);
  if (playing && lineCount > 1) {
    const uint32_t phase = nowMs % kLyricSwitchPeriodMs;
    if (phase > kLyricSwitchPeriodMs - 360U) {
      baselineY = static_cast<int16_t>(baselineY - (phase - (kLyricSwitchPeriodMs - 360U)) / 90U);
    }
  }

  for (uint8_t i = 0; i < lineCount; ++i) {
    const int16_t lineX = TextUtils::centeredTextXInBox(text, lines[i], kLyricX, kLyricWidth);
    text.drawUTF8(lineX, static_cast<int16_t>(baselineY + i * kLyricRowStep), lines[i]);
  }

  canvas.drawFilledRectangle(kLyricX, static_cast<int16_t>(kLyricY + kLyricHeight),
                             static_cast<int16_t>(kLyricX + kLyricWidth - 1),
                             static_cast<int16_t>(kPlayerAreaHeight - 1), ST7305_COLOR_WHITE);
}

void drawHeadphoneIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y) {
  canvas.drawFastHLine(static_cast<int16_t>(x + 4), y, 8, ST7305_COLOR_BLACK);
  canvas.drawFastVLine(static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 1), 5,
                       ST7305_COLOR_BLACK);
  canvas.drawFastVLine(static_cast<int16_t>(x + 12), static_cast<int16_t>(y + 1), 5,
                       ST7305_COLOR_BLACK);
  canvas.drawFilledRectangle(x, static_cast<int16_t>(y + 7), static_cast<int16_t>(x + 4),
                             static_cast<int16_t>(y + 12), ST7305_COLOR_BLACK);
  canvas.drawFilledRectangle(static_cast<int16_t>(x + 11), static_cast<int16_t>(y + 7),
                             static_cast<int16_t>(x + 15), static_cast<int16_t>(y + 12),
                             ST7305_COLOR_BLACK);
}

void drawSpeakerIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y) {
  canvas.drawFilledRectangle(x, static_cast<int16_t>(y + 5), static_cast<int16_t>(x + 3),
                             static_cast<int16_t>(y + 10), ST7305_COLOR_BLACK);
  canvas.drawLine(static_cast<uint>(x + 4), static_cast<uint>(y + 5),
                  static_cast<uint>(x + 9), static_cast<uint>(y + 2), ST7305_COLOR_BLACK);
  canvas.drawLine(static_cast<uint>(x + 4), static_cast<uint>(y + 10),
                  static_cast<uint>(x + 9), static_cast<uint>(y + 13), ST7305_COLOR_BLACK);
  canvas.drawFastVLine(static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 2), 12,
                       ST7305_COLOR_BLACK);
  canvas.drawFastVLine(static_cast<int16_t>(x + 12), static_cast<int16_t>(y + 4), 8,
                       ST7305_COLOR_BLACK);
  canvas.drawFastVLine(static_cast<int16_t>(x + 15), static_cast<int16_t>(y + 6), 4,
                       ST7305_COLOR_BLACK);
}

bool isPlaybackActive(const MusicService& music) {
  return music.playbackState() == MusicService::PlaybackState::Playing ||
         music.playbackState() == MusicService::PlaybackState::Opening;
}

bool hasPlayableCurrentTrack(const MusicService& music) {
  return music.currentTrack().path[0] != '\0' &&
         music.playbackState() != MusicService::PlaybackState::Stopped &&
         music.playbackState() != MusicService::PlaybackState::Error;
}

void drawAudioOutputIndicator(DisplayMonoTft& display, bool headphoneInserted) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  canvas.drawFilledRectangle(kAudioOutputX, kAudioOutputY,
                             static_cast<int16_t>(kAudioOutputX + kAudioOutputWidth - 1),
                             static_cast<int16_t>(kAudioOutputY + kAudioOutputHeight - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawRectangle(kAudioOutputX, kAudioOutputY,
                       static_cast<int16_t>(kAudioOutputX + kAudioOutputWidth - 1),
                       static_cast<int16_t>(kAudioOutputY + kAudioOutputHeight - 1),
                       ST7305_COLOR_BLACK);

  if (headphoneInserted) {
    drawHeadphoneIcon(canvas, kAudioOutputIconX, kAudioOutputIconY);
  } else {
    drawSpeakerIcon(canvas, kAudioOutputIconX, kAudioOutputIconY);
  }

  text.setFont(u8g2_font_6x12_mf);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  text.drawUTF8(kAudioOutputLabelX, kAudioOutputLabelBaselineY,
                headphoneInserted ? "HP" : "SPK");
}

uint8_t volumePercent(uint8_t volume) {
  const uint16_t percent =
      (static_cast<uint16_t>(volume) * 100U + MusicService::kMaxVolume / 2U) /
      MusicService::kMaxVolume;
  return static_cast<uint8_t>(min<uint16_t>(100U, percent));
}

void drawVolumePopup(DisplayMonoTft& display, const MusicService& music,
                     const PopupView::InfoState& state, uint32_t nowMs) {
  char percentText[8];
  snprintf(percentText, sizeof(percentText), "%u%%", volumePercent(music.volume()));
  PopupView::drawInfo(display, 0, state, "音量", percentText, nowMs);
}

const char* playerTitleFor(const MusicService& music, uint8_t rowIndex) {
  if (music.currentTrack().name[0] != '\0') {
    return music.currentTrack().name;
  }
  if (music.listState() == MusicService::ListState::Ready && music.page().count > 0) {
    const uint8_t safeRow = min<uint8_t>(rowIndex, static_cast<uint8_t>(music.page().count - 1U));
    return music.page().tracks[safeRow].name[0] != '\0' ? music.page().tracks[safeRow].name
                                                        : "OpenBread Music";
  }
  return "OpenBread Music - UI Preview";
}

void drawPlayerPreviewSurface(DisplayMonoTft& display, const MusicService& music,
                              uint8_t rowIndex, uint32_t, bool playing, SdBitmapFont* font,
                              SdBitmapFont* large, uint8_t control, int16_t lyricShift) {
  auto& canvas=display.canvas();auto& text=display.text();
  DrawUtils::drawClippedFilledRect(canvas,0,0,383,167,ST7305_COLOR_WHITE);
  text.setFont(u8g2_font_7x14_tf);text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);text.setBackgroundColor(ST7305_COLOR_WHITE);
  char name[MusicService::kMaxNameLen];snprintf(name,sizeof(name),"%s",playerTitleFor(music,rowIndex));
  char* dot=strrchr(name,'.');if(dot && dot!=name)*dot=0;
  const SdBitmapText::Clip titleClip={16,2,263,24};
  if(SdUiText::ready(font))SdBitmapText::drawUTF8(canvas,*font,16,20,name,ST7305_COLOR_BLACK,titleClip);
  else text.drawUTF8(16,20,name);
  const char* status=playing?"播放中":"已暂停";
  text.drawUTF8(368-text.getUTF8Width(status),20,status);
  DrawUtils::drawClippedFilledRect(canvas,16,26,367,26,ST7305_COLOR_BLACK);
  const MusicLyrics& lyrics=music.lyrics();
  const int current=lyrics.current(music.currentSeconds()*1000U);
  auto lyric=[&](const char* line,int16_t baseline,SdBitmapFont* face,int16_t top,int16_t bottom) {
    if(!line || !*line || !SdUiText::ready(face))return;
    const int32_t length=SdBitmapText::measureUTF8(*face,line);
    const int16_t x=static_cast<int16_t>(std::max<int32_t>(16,(384-length)/2));
    const SdBitmapText::Clip clip={16,top,367,bottom};
    SdBitmapText::drawUTF8(canvas,*face,x,baseline,line,ST7305_COLOR_BLACK,clip);
  };
  if(lyrics.state()==MusicLyrics::State::Ready) {
    lyric(lyrics.line(current-1),46+lyricShift,font,28,49);
    lyric(lyrics.line(current),69+lyricShift,SdUiText::ready(large)?large:font,47,73);
    lyric(lyrics.line(current+1),91+lyricShift,font,72,95);
  } else if(lyrics.state()==MusicLyrics::State::Loading) {
    const char* label="正在加载歌词";
    text.drawUTF8((384-text.getUTF8Width(label))/2,65,label);
  } else {
    canvas.drawCircle(192,56,19,ST7305_COLOR_BLACK);
    canvas.drawCircle(192,56,4,ST7305_COLOR_BLACK);
    const char* label="暂无歌词";text.drawUTF8((384-text.getUTF8Width(label))/2,91,label);
  }
  char cur[10],total[10];formatTime(music.currentSeconds(),cur,sizeof(cur));
  formatDurationTime(music.durationSeconds(),total,sizeof(total));
  text.drawUTF8(16,109,cur);text.drawUTF8(368-text.getUTF8Width(total),109,total);
  DrawUtils::drawClippedFilledRect(canvas,62,102,321,103,ST7305_COLOR_BLACK);
  if(music.durationSeconds()) {
    const uint32_t position=std::min(music.currentSeconds(),music.durationSeconds());
    DrawUtils::drawClippedFilledRect(canvas,62,101,62+259ULL*position/music.durationSeconds(),104,ST7305_COLOR_BLACK);
  }
  const char* labels[]={playing?"暂停":"播放",music.shuffleEnabled()?"随机":"顺序","上一首","下一首"};
  for(uint8_t i=0;i<4;++i) {
    const int16_t x=16+i*89;const bool active=i==control;
    if(active)DrawUtils::drawRoundRect(canvas,x,115,82,27,13,ST7305_COLOR_BLACK,ST7305_COLOR_BLACK);
    text.setForegroundColor(active?ST7305_COLOR_WHITE:ST7305_COLOR_BLACK);
    text.setBackgroundColor(active?ST7305_COLOR_BLACK:ST7305_COLOR_WHITE);
    text.drawUTF8(x+(82-text.getUTF8Width(labels[i]))/2,134,labels[i]);
  }
  text.setForegroundColor(ST7305_COLOR_BLACK);text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(16,162,"< 返回列表");
  const char* footer="上下 选择 OK 确认";
  text.drawUTF8(368-text.getUTF8Width(footer),162,footer);
}

void drawPlayerTransitionFrame(DisplayMonoTft& display, const MusicService& music,
                               uint16_t pageIndex, uint8_t rowIndex,
                               uint32_t nowMs, bool focusAnimating, uint8_t focusFromRow,
                               uint8_t focusToRow,
                               uint32_t focusAnimStartMs, float progress, SdBitmapFont* font) {
  auto& canvas = display.canvas();
  progress = clamp01(progress);

  const float phase1Progress = clamp01(progress / kPlayerTransitionPhase1End);
  const float phase2Progress =
      clamp01((progress - kPlayerTransitionPhase1End) /
              (1.0f - kPlayerTransitionPhase1End));
  const float phase1 = easeOutCubic(phase1Progress);
  const float phase2 = easeOutCubic(phase2Progress);

  const int16_t listOffsetX = lerpInt(0, static_cast<int16_t>(display.width()), phase1);
  if (listOffsetX < static_cast<int16_t>(display.width())) {
    drawMusicList(display, music, pageIndex, rowIndex, nowMs, focusAnimating,
                  focusFromRow, focusToRow, focusAnimStartMs, listOffsetX, font);
  }

  int16_t barX = lerpInt(kPlayerBarX, kPlayerCompactX, phase1);
  int16_t barY = kPlayerCompactY;
  int16_t barWidth = lerpInt(kPlayerBarWidth, kPlayerCompactWidth, phase1);
  int16_t barHeight = kPlayerCompactHeight;

  if (progress > kPlayerTransitionPhase1End) {
    barX = kPlayerSideX;
    barY = lerpInt(kPlayerCompactY, kPlayerSideY, phase2);
    barWidth = kPlayerSideWidth;
    barHeight = lerpInt(kPlayerCompactHeight, kPlayerSideHeight, phase2);
  }

  DrawUtils::drawRoundRect(canvas, barX, barY, barWidth, barHeight,
                           static_cast<int16_t>(min(barWidth, barHeight) / 2),
                           ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
}
}  // namespace

bool MusicPage::isMusicListSelection(uint8_t homeFocus, uint8_t sectionFocus) const {
  return homeFocus == kHomeIndex && sectionFocus == kMusicListItemIndex;
}

uint8_t MusicPage::detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isMusicListSelection(homeFocus, sectionFocus) ? 1U : 0U;
}

void MusicPage::resetState(uint8_t homeFocus, uint8_t sectionFocus) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return;
  }
  resetStateUnchecked();
}

void MusicPage::handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus, MusicService& music,
                                  SdCardService& sd) {
  (void)music;
  (void)sd;
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return;
  }

  resetStateUnchecked();
  (void)music.startListScan(sd);
  refreshFavoriteState(music);
}

bool MusicPage::handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                                  bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                                  uint32_t nowMs, MusicService& music) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return false;
  }

  if (PopupView::isVisible(volumePopup_.anim)) {
    if (leftEdge || okEdge) {
      PopupView::requestClose(volumePopup_, nowMs);
      return true;
    }
    if (upEdge || downEdge) {
      (void)music.adjustVolume(upEdge ? kVolumeStep : -kVolumeStep);
      return true;
    }
    return rightEdge || upEdge || downEdge || okEdge;
  }

  if (playerViewState_ == PlayerViewState::Player) {
    if (leftEdge) {
      const uint32_t index = music.currentTrackIndex();
      if (index < music.trackCount()) {
        const uint16_t page = index / MusicService::kRowsPerPage;
        if (page == pageIndex_ || music.loadPage(page)) {
          pageIndex_ = page;
          rowIndex_ = index % MusicService::kRowsPerPage;
          refreshFavoriteState(music);
        }
      }
      playerViewState_ = PlayerViewState::List;
      return true;
    }
    if (upEdge) {
      movePlayerControl(-1);
      return true;
    }
    if (downEdge) {
      movePlayerControl(1);
      return true;
    }
    if (okEdge) {
      if (playerControlIndex_ == static_cast<uint8_t>(PlayerControl::PlayPause)) {
        switch (music.playbackState()) {
          case MusicService::PlaybackState::Playing:
          case MusicService::PlaybackState::Paused:
          case MusicService::PlaybackState::Suspended:
            (void)music.togglePause(nowMs);
            break;
          case MusicService::PlaybackState::Opening:
            break;
          case MusicService::PlaybackState::Stopped:
          case MusicService::PlaybackState::Error:
          default:
            (void)playSelectedTrack(music);
            break;
        }
        setPlayerUiPlaying(isPlaybackActive(music), nowMs);
      } else if (playerControlIndex_ == static_cast<uint8_t>(PlayerControl::PlayMode)) {
        music.setShuffleEnabled(!music.shuffleEnabled());
        playerUiShuffle_ = music.shuffleEnabled();
      } else if (playerControlIndex_ == static_cast<uint8_t>(PlayerControl::Previous)) {
        if (!hasPlayableCurrentTrack(music)) {
          (void)playSelectedTrack(music);
        } else {
          (void)music.previous();
        }
        setPlayerUiPlaying(isPlaybackActive(music), nowMs);
      } else if (playerControlIndex_ == static_cast<uint8_t>(PlayerControl::Next)) {
        if (!hasPlayableCurrentTrack(music)) {
          (void)playSelectedTrack(music);
        } else {
          (void)music.next();
        }
        setPlayerUiPlaying(isPlaybackActive(music), nowMs);
      }
      return true;
    }
    return rightEdge || upEdge || downEdge || okEdge;
  }

  if (playerViewState_ == PlayerViewState::Entering ||
      playerViewState_ == PlayerViewState::Leaving) {
    if (leftEdge) playerViewState_ = PlayerViewState::List;
    return leftEdge || rightEdge || upEdge || downEdge || okEdge;
  }
  if (leftEdge) {
    if (selectedIndex_ != 0) { selectedIndex_=0; return true; }
    return false;
  }
  if (rightEdge) {
    selectedIndex_=1; refreshFavoriteState(music); return true;
  }
  if (upEdge || downEdge) {
    if (selectedIndex_) selectedIndex_=upEdge?1:2;
    else { moveListFocus(upEdge?-1:1,nowMs,music); refreshFavoriteState(music); }
    return true;
  }
  if (!okEdge) return false;
  if (selectedIndex_==1) {
    const auto* track=focusedTrack(music);
    if(track && music.toggleFavorite(*track))refreshFavoriteState(music);
    return true;
  }
  if (selectedIndex_==2) { PopupView::open(volumePopup_,nowMs); return true; }
  if (playSelectedTrack(music)) {
    playerControlIndex_=0;
    playerViewState_=PlayerViewState::Player;
  }
  return true;
}

bool MusicPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, MusicService& music,
                                 SdCardService& sd) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return false;
  }

  selectionAnimating_ = false;
  listFocusAnimating_ = false;
  volumePopup_ = PopupView::InfoState{};
  music.exitMusic(sd);
  resetStateUnchecked();
  return false;
}

bool MusicPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                             DisplayMonoTft& display, const MusicService& music,
                             uint32_t nowMs, SdBitmapFont* font, SdBitmapFont* large) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return false;
  }
  if (yOffset != 0) return true;

  (void)prepareListFont(music, font);
  const float progress = selectionAnimating_ ? selectionProgress(nowMs) : 1.0f;
  const bool listFocusAnimating = isListFocusAnimating(nowMs);

  if (playerViewState_ != PlayerViewState::List) {
    playerViewState_ = PlayerViewState::Player;
    playerUiPlaying_ = isPlaybackActive(music);
    playerUiShuffle_ = music.shuffleEnabled();
    const int lyric = music.lyrics().current(music.currentSeconds() * 1000U);
    if (lyric != previousLyric_) {
      previousLyric_ = lyric;
      lyricChangeMs_ = nowMs;
    }
    const uint32_t elapsed = nowMs - lyricChangeMs_;
    const int16_t shift = lyric >= 0 && elapsed < 200U ? static_cast<int16_t>((200U - elapsed) / 10U) : 0;
    drawPlayerPreviewSurface(display, music, rowIndex_, nowMs, playerUiPlaying_, font, large, playerControlIndex_, shift);
    if (!listFocusAnimating) {
      listFocusAnimating_ = false;
    }
    return true;
  }

  if (music.listState() == MusicService::ListState::Ready && large &&
      large->state() == SdBitmapFont::State::Checking && SdUiText::ready(font,32)) {
    auto& canvas=display.canvas();auto& text=display.text();
    DrawUtils::drawClippedFilledRect(canvas,0,0,383,127,ST7305_COLOR_WHITE);
    text.setFont(u8g2_font_7x14_tf);text.setFontMode(1);
    text.setForegroundColor(ST7305_COLOR_BLACK);text.setBackgroundColor(ST7305_COLOR_WHITE);
    const char* message="正在准备字体";
    text.drawUTF8(TextUtils::centeredTextXInBox(text,message,0,384),77,message);
    drawMusicNavFrame(display,selectedIndex_,favoriteEnabled_,yOffset,1.0f);
    return true;
  }

  longNameVisible_ = false;
  if (music.listState() == MusicService::ListState::Ready && rowIndex_ < music.page().count &&
      SdUiText::ready(font, 32)) {
    longNameVisible_ = SdBitmapText::measureUTF8(*font, music.page().tracks[rowIndex_].name) > 290;
  }
  drawMusicList(display, music, pageIndex_, rowIndex_, nowMs, listFocusAnimating,
                listFocusFromRow_, listFocusToRow_, listFocusAnimStartMs_, 0, font);
  drawMusicNavFrame(display, selectedIndex_, favoriteEnabled_, yOffset, progress);
  if (PopupView::isVisible(volumePopup_.anim)) {
    // 音量弹窗的进场/退场动画在这里推进，退场结束即自动消失。
    PopupView::update(volumePopup_, nowMs);
    drawVolumePopup(display, music, volumePopup_, nowMs);
  }
  if (!isSelectionAnimating(nowMs)) {
    selectionAnimating_ = false;
  }
  if (!listFocusAnimating) {
    listFocusAnimating_ = false;
  }
  return true;
}

bool MusicPage::renderDetailNavOnly(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                                    DisplayMonoTft& display, uint32_t nowMs) {
  if (!isMusicListSelection(homeFocus, sectionFocus) ||
      playerViewState_ != PlayerViewState::List) {
    return false;
  }

  const float progress = selectionAnimating_ ? selectionProgress(nowMs) : 1.0f;
  clearMusicNavArea(display, yOffset);
  drawMusicNavFrame(display, selectedIndex_, favoriteEnabled_, yOffset, progress);
  if (!isSelectionAnimating(nowMs)) {
    selectionAnimating_ = false;
  }
  return true;
}

bool MusicPage::renderDetailListOnly(uint8_t homeFocus, uint8_t sectionFocus,
                                     DisplayMonoTft& display, const MusicService& music,
                                     uint32_t nowMs, SdBitmapFont* font) {
  if (!isMusicListSelection(homeFocus, sectionFocus) ||
      playerViewState_ != PlayerViewState::List) {
    return false;
  }

  (void)prepareListFont(music, font);
  const bool listFocusAnimating = isListFocusAnimating(nowMs);
  clearMusicListArea(display);
  drawMusicList(display, music, pageIndex_, rowIndex_, nowMs, listFocusAnimating,
                listFocusFromRow_, listFocusToRow_, listFocusAnimStartMs_, 0, font);
  if (!listFocusAnimating) {
    listFocusAnimating_ = false;
  }
  return true;
}

bool MusicPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                    uint32_t nowMs) const {
  return isMusicListSelection(homeFocus, sectionFocus) &&
         (selectionAnimating_ || isListFocusAnimating(nowMs) ||
          longNameVisible_ ||
          PopupView::isAnimating(volumePopup_.anim) ||
          playerViewState_ == PlayerViewState::Entering ||
          playerViewState_ == PlayerViewState::Leaving ||
          playerViewState_ == PlayerViewState::Player);
}

bool MusicPage::needsNavAnimationFrame(uint8_t, uint8_t, uint32_t) const { return false; }
bool MusicPage::needsListAnimationFrame(uint8_t, uint8_t, uint32_t) const { return false; }

uint32_t MusicPage::detailFrameIntervalMs(uint8_t homeFocus, uint8_t sectionFocus) const {
  // 音量弹窗的进场/退场按 60fps 出帧，其余动画保持 30fps。
  if (PopupView::isAnimating(volumePopup_.anim)) {
    return kPopupFrameIntervalMs;
  }
  return isMusicListSelection(homeFocus, sectionFocus) ? 250U : 0U;
}

void MusicPage::startSelectionAnimation(uint8_t nextIndex, uint32_t nowMs) {
  LogD("MUSIC", "nav tab switch %u -> %u row=%u", static_cast<unsigned>(selectedIndex_),
       static_cast<unsigned>(nextIndex), static_cast<unsigned>(rowIndex_));
  selectedIndex_ = static_cast<uint8_t>(nextIndex % kTabCount);
  selectionAnimating_ = true;
  selectionAnimStartMs_ = nowMs;
}

void MusicPage::startPlayerTransition(bool entering, uint32_t nowMs) {
  const float currentProgress = playerTransitionProgress(nowMs);
  LogD("MUSIC", "player transition %s tab=%u row=%u progress=%u/100", entering ? "enter" : "leave",
       static_cast<unsigned>(selectedIndex_), static_cast<unsigned>(rowIndex_),
           static_cast<unsigned>(currentProgress * 100.0f + 0.5f));
  playerTransitionStartProgress_ = currentProgress;
  playerTransitionTargetProgress_ = entering ? 1.0f : 0.0f;
  playerTransitionStartMs_ = nowMs;

  const float distance = entering ? (1.0f - currentProgress) : currentProgress;
  uint32_t duration =
      static_cast<uint32_t>(kPlayerTransitionDurationMs * distance + 0.5f);
  if (duration < kPlayerTransitionMinDurationMs && distance > 0.0f) {
    duration = kPlayerTransitionMinDurationMs;
  }
  playerTransitionDurationMs_ = duration;

  if (distance <= 0.0f || duration == 0U) {
    playerViewState_ = entering ? PlayerViewState::Player : PlayerViewState::List;
    playerTransitionStartProgress_ = playerTransitionTargetProgress_;
    return;
  }

  playerViewState_ = entering ? PlayerViewState::Entering : PlayerViewState::Leaving;
}

void MusicPage::setPlayerUiPlaying(bool playing, uint32_t nowMs) {
  (void)nowMs;
  playerUiPlaying_ = playing;
}

void MusicPage::movePlayerControl(int8_t delta) {
  int8_t next = static_cast<int8_t>(playerControlIndex_) + delta;
  if (next < 0) {
    next = static_cast<int8_t>(kPlayerControlCount - 1);
  } else if (next >= static_cast<int8_t>(kPlayerControlCount)) {
    next = 0;
  }
  playerControlIndex_ = static_cast<uint8_t>(next);
}

void MusicPage::moveListFocus(int8_t delta, uint32_t nowMs, MusicService& music) {
  if (music.listState() != MusicService::ListState::Ready || music.trackCount() == 0) {
    return;
  }

  const uint16_t oldPage = pageIndex_;
  const uint8_t oldRow = rowIndex_;
  int32_t index = static_cast<int32_t>(selectedAbsoluteIndex()) + delta;
  if (index < 0) {
    return;
  } else if (index >= static_cast<int32_t>(music.trackCount())) {
    return;
  }

  const uint16_t newPage = static_cast<uint16_t>(index / MusicService::kRowsPerPage);
  const uint8_t newRow = static_cast<uint8_t>(index % MusicService::kRowsPerPage);
  if (newPage != pageIndex_) {
    if (!music.loadPage(newPage)) return;
    pageIndex_ = newPage;
    listFocusAnimating_ = false;
  } else if (newRow != oldRow) {
    listFocusAnimating_ = false;
    listFocusFromRow_ = oldRow;
    listFocusToRow_ = newRow;
    listFocusAnimStartMs_ = nowMs;
  }
  rowIndex_ = newRow;
  if (oldPage != pageIndex_) {
    rowIndex_ = newRow;
  }
}

void MusicPage::refreshFavoriteState(MusicService& music) {
  const MusicService::TrackInfo* track = focusedTrack(music);
  favoriteEnabled_ = track != nullptr && music.isFavorite(*track);
}

bool MusicPage::playSelectedTrack(MusicService& music) {
  if (music.listState() != MusicService::ListState::Ready || music.trackCount() == 0) {
    return false;
  }

  const uint32_t index = selectedAbsoluteIndex();
  if (index >= music.trackCount()) {
    return false;
  }
  return music.playTrack(index);
}

bool MusicPage::prepareListFont(const MusicService& music, SdBitmapFont* font) {
  if (music.listState() != MusicService::ListState::Ready || !SdUiText::ready(font, 32)) {
    listFontPrepared_ = false;
    return false;
  }

  const MusicService::Page& page = music.page();
  const uint32_t session = font->session();
  if (listFontPrepared_ && preparedFontSession_ == session && preparedFontPage_ == page.pageIndex) {
    return true;
  }

  // Warm only the visible page. The full NBFT stays on SD and the existing
  // bounded glyph cache retains the metrics/bitmap data needed by the first draw.
  listFontPrepared_ = false;
  for (uint8_t row = 0; row < page.count; ++row) {
    char line[96];
    makeMusicListLine(page, row, line, sizeof(line));
    (void)SdBitmapText::measureUTF8(*font, line);
    if (!SdUiText::ready(font, 32)) return false;
  }

  preparedFontSession_ = session;
  preparedFontPage_ = page.pageIndex;
  listFontPrepared_ = true;
  return true;
}

uint32_t MusicPage::selectedAbsoluteIndex() const {
  return static_cast<uint32_t>(pageIndex_) * MusicService::kRowsPerPage + rowIndex_;
}

const MusicService::TrackInfo* MusicPage::focusedTrack(const MusicService& music) const {
  if (music.listState() != MusicService::ListState::Ready) {
    return nullptr;
  }

  const MusicService::Page& page = music.page();
  if (rowIndex_ >= page.count) {
    return nullptr;
  }
  return &page.tracks[rowIndex_];
}

float MusicPage::selectionProgress(uint32_t nowMs) const {
  if (!selectionAnimating_) {
    return 1.0f;
  }

  const uint32_t elapsed = nowMs - selectionAnimStartMs_;
  if (elapsed >= kAnimationDurationMs) {
    return 1.0f;
  }
  return static_cast<float>(elapsed) / static_cast<float>(kAnimationDurationMs);
}

float MusicPage::playerTransitionProgress(uint32_t nowMs) const {
  if (playerViewState_ == PlayerViewState::Player) {
    return 1.0f;
  }
  if (playerViewState_ == PlayerViewState::List) {
    return 0.0f;
  }
  if (playerTransitionDurationMs_ == 0U) {
    return playerTransitionTargetProgress_;
  }

  const uint32_t elapsed = nowMs - playerTransitionStartMs_;
  if (elapsed >= playerTransitionDurationMs_) {
    return playerTransitionTargetProgress_;
  }

  const float t = static_cast<float>(elapsed) / static_cast<float>(playerTransitionDurationMs_);
  return playerTransitionStartProgress_ +
         (playerTransitionTargetProgress_ - playerTransitionStartProgress_) * clamp01(t);
}

bool MusicPage::isSelectionAnimating(uint32_t nowMs) const {
  return selectionAnimating_ && (nowMs - selectionAnimStartMs_) < kAnimationDurationMs;
}

bool MusicPage::isListFocusAnimating(uint32_t nowMs) const {
  return listFocusAnimating_ &&
         (nowMs - listFocusAnimStartMs_) < kListFocusAnimationDurationMs;
}

bool MusicPage::isPlayerTransitionAnimating(uint32_t nowMs) const {
  if (playerViewState_ != PlayerViewState::Entering &&
      playerViewState_ != PlayerViewState::Leaving) {
    return false;
  }
  return (nowMs - playerTransitionStartMs_) < playerTransitionDurationMs_;
}

void MusicPage::resetStateUnchecked() {
  selectedIndex_ = static_cast<uint8_t>(NavTab::Music);
  favoriteEnabled_ = false;
  selectionAnimating_ = false;
  selectionAnimStartMs_ = 0;
  pageIndex_ = 0;
  rowIndex_ = 0;
  listFocusAnimating_ = false;
  listFocusFromRow_ = 0;
  listFocusToRow_ = 0;
  listFocusAnimStartMs_ = 0;
  volumePopup_ = PopupView::InfoState{};
  playerViewState_ = PlayerViewState::List;
  playerControlIndex_ = 0;
  playerUiPlaying_ = false;
  playerUiShuffle_ = false;
  playerTransitionStartProgress_ = 0.0f;
  playerTransitionTargetProgress_ = 0.0f;
  playerTransitionStartMs_ = 0;
  playerTransitionDurationMs_ = 0;
  preparedFontSession_ = 0;
  preparedFontPage_ = 0;
  listFontPrepared_ = false;
  longNameVisible_ = false;
  previousLyric_ = -2;
  lyricChangeMs_ = 0;
}
