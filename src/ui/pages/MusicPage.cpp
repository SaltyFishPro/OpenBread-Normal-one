#include "MusicPage.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "../../bsp/DisplayMonoTft.h"
#include "../AnimMath.h"
#include "../DrawUtils.h"
#include "../IconBitmap.h"
#include "../TextUtils.h"
#include "../assets/musicapp/music_nav_icons.h"
#include "../assets/ui/pop_up_window.h"

namespace {
// 音乐页动画触发日志：排查"底部导航栏动画跑到屏幕上方"这类问题时，用来确认
// 究竟是标签切换动画还是进入播放器动画在跑。排查完成后可置 0 关闭。
#ifndef OB_MUSIC_LOG_ENABLED
#define OB_MUSIC_LOG_ENABLED 1
#endif

void musicLog(const char* fmt, ...) {
#if OB_MUSIC_LOG_ENABLED
  if (!Serial) {
    return;
  }
  Serial.print("[MUSIC] ");
  va_list args;
  va_start(args, fmt);
  Serial.vprintf(fmt, args);
  va_end(args);
  Serial.println();
#else
  (void)fmt;
#endif
}

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
constexpr int16_t kVolumePopupTitleTopOffset = 35;
constexpr int16_t kVolumePopupValueBaselineOffset = 63;

const IconBitmap::Anim kVolumePopupWindow = {
    reinterpret_cast<const uint8_t*>(&pop_up_window_frames[0][0]),
    POP_UP_WINDOW_FRAME_BYTES,
    POP_UP_WINDOW_FRAME_WIDTH,
    POP_UP_WINDOW_FRAME_HEIGHT,
    POP_UP_WINDOW_FRAME_DELAY,
    POP_UP_WINDOW_FRAME_COUNT};

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
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  const int16_t textWidth = text.getUTF8Width(suffix);
  text.drawUTF8(static_cast<int16_t>(finalRight - textWidth),
                static_cast<int16_t>(yOffset + kTextBaselineY), suffix);
}

void drawMusicNavFrame(DisplayMonoTft& display, uint8_t selectedIndex, bool favoriteEnabled,
                       int16_t yOffset, float progress) {
  auto& canvas = display.canvas();
  progress = clamp01(progress);

  DrawUtils::drawRoundRect(canvas, kPlayerBarX, static_cast<int16_t>(yOffset + kPlayerBarY),
                           kPlayerBarWidth, kPlayerBarHeight, kPlayerBarRadius,
                           ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);

  for (uint8_t i = 0; i < kTabCount; ++i) {
    if (i == selectedIndex && progress > 0.0f) {
      continue;
    }
    drawBitmapIcon(canvas, iconForTab(i, favoriteEnabled), tabCenterX(i),
                   static_cast<int16_t>(yOffset + kTabY + kTabHeight / 2),
                   ST7305_COLOR_WHITE);
  }

  if (progress > 0.0f) {
    const float easedProgress = easeOutCubic(progress);
    const int16_t selectedX = tabX(selectedIndex);
    const int16_t pillWidth = lerpInt(kTabHeight, kTabWidth, easedProgress);
    const int16_t pillX = static_cast<int16_t>(selectedX + (kTabWidth - pillWidth) / 2);
    const int16_t iconX =
        lerpInt(tabCenterX(selectedIndex), static_cast<int16_t>(selectedX + 21), easedProgress);

    DrawUtils::drawRoundRect(canvas, pillX, static_cast<int16_t>(yOffset + kTabY), pillWidth,
                             kTabHeight, kTabRadius, ST7305_COLOR_WHITE, ST7305_COLOR_WHITE);
    drawBitmapIcon(canvas, iconForTab(selectedIndex, favoriteEnabled), iconX,
                   static_cast<int16_t>(yOffset + kTabY + kTabHeight / 2),
                   ST7305_COLOR_BLACK);
    drawRevealedLabel(display, selectedIndex, yOffset, progress);
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
                   uint8_t rowIndex, uint32_t nowMs, bool focusAnimating,
                   uint8_t focusFromRow, uint8_t focusToRow,
                   uint32_t focusAnimStartMs, int16_t xOffset) {
  auto& canvas = display.canvas();
  auto& text = display.text();

  canvas.drawFilledRectangle(static_cast<int16_t>(kListAreaX + xOffset), kListAreaY,
                             translatedRightEdge(xOffset, kListAreaWidth - 1),
                             static_cast<int16_t>(kListAreaHeight - 1), ST7305_COLOR_WHITE);
  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);

  if (music.listState() != MusicService::ListState::Ready) {
    const char* state = listStateText(music.listState());
    text.drawUTF8(TextUtils::centeredTextXInBox(text, state, xOffset, kListAreaWidth), 52, state);
    if (music.errorText()[0] != '\0') {
      text.setFont(u8g2_font_7x14_tf);
      text.drawUTF8(static_cast<int16_t>(xOffset + 8), 76, music.errorText());
    }
    return;
  }

  const MusicService::Page& page = music.page();
  if (page.count == 0) {
    return;
  }

  rowIndex = min<uint8_t>(rowIndex, static_cast<uint8_t>(page.count - 1U));

  char selectedLine[96];
  makeMusicListLine(page, rowIndex, selectedLine, sizeof(selectedLine));
  int16_t toFocusX = 0;
  int16_t toFocusY = 0;
  int16_t toFocusW = 0;
  int16_t toFocusH = 0;
  musicListFocusRect(text, selectedLine, musicListBaselineY(rowIndex), xOffset, toFocusX, toFocusY,
                     toFocusW, toFocusH);

  int16_t focusX = toFocusX;
  int16_t focusY = toFocusY;
  int16_t focusW = toFocusW;
  int16_t focusH = toFocusH;
  if (focusAnimating && focusFromRow < page.count && focusToRow < page.count) {
    char fromLine[96];
    makeMusicListLine(page, focusFromRow, fromLine, sizeof(fromLine));
    int16_t fromFocusX = 0;
    int16_t fromFocusY = 0;
    int16_t fromFocusW = 0;
    int16_t fromFocusH = 0;
    musicListFocusRect(text, fromLine, musicListBaselineY(focusFromRow), xOffset, fromFocusX,
                       fromFocusY, fromFocusW, fromFocusH);
    const uint32_t elapsed = nowMs - focusAnimStartMs;
    const float progress = elapsed >= kListFocusAnimationDurationMs
                               ? 1.0f
                               : static_cast<float>(elapsed) /
                                     static_cast<float>(kListFocusAnimationDurationMs);
    focusX = easeOutCubicInt(fromFocusX, toFocusX, progress);
    focusY = easeOutCubicInt(fromFocusY, toFocusY, progress);
    focusW = easeOutCubicInt(fromFocusW, toFocusW, progress);
    focusH = easeOutCubicInt(fromFocusH, toFocusH, progress);
  }

  DrawUtils::drawRoundRect(canvas, focusX, focusY, focusW, focusH, kListFocusRadius,
                           ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);

  for (uint8_t i = 0; i < page.count; ++i) {
    const int16_t baselineY = musicListBaselineY(i);
    const bool selected = i == rowIndex;
    int16_t rowFocusX = 0;
    int16_t rowFocusY = 0;
    int16_t rowFocusW = 0;
    int16_t rowFocusH = 0;
    if (selected) {
      text.setForegroundColor(ST7305_COLOR_WHITE);
      text.setBackgroundColor(ST7305_COLOR_BLACK);
      text.setFontMode(0);
    } else {
      text.setForegroundColor(ST7305_COLOR_BLACK);
      text.setBackgroundColor(ST7305_COLOR_WHITE);
      text.setFontMode(1);
    }

    char line[96];
    makeMusicListLine(page, i, line, sizeof(line));
    if (selected) {
      musicListFocusRect(text, line, baselineY, xOffset, rowFocusX, rowFocusY, rowFocusW,
                         rowFocusH);
    }
    const int16_t selectedMaskRight =
        selected ? static_cast<int16_t>(rowFocusX + rowFocusW - 1)
                 : translatedRightEdge(xOffset, kListAreaWidth - 1);
    drawClippedText(display, line, static_cast<int16_t>(xOffset + kListTextX), baselineY,
                    translatedRightEdge(xOffset, kListTextMaxRight), selectedMaskRight,
                    translatedRightEdge(xOffset, kListAreaWidth - 1), selected, nowMs);
  }

  const uint32_t selectedAbsolute =
      static_cast<uint32_t>(pageIndex) * MusicService::kRowsPerPage + rowIndex;
  drawMusicListProgress(display, music, selectedAbsolute, xOffset);
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
                     uint32_t popupElapsedMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t popupW = static_cast<int16_t>(kVolumePopupWindow.frameWidth);
  const int16_t popupH = static_cast<int16_t>(kVolumePopupWindow.frameHeight);
  const int16_t popupX = static_cast<int16_t>((width - popupW) / 2);
  const int16_t popupY = static_cast<int16_t>((height - popupH) / 2);
  const uint16_t popupFrame = IconBitmap::frameAt(kVolumePopupWindow, popupElapsedMs);
  IconBitmap::drawFrame(canvas, kVolumePopupWindow, popupFrame, popupX, popupY, popupW, popupH,
                        false, 0, static_cast<int16_t>(height - 1));

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
  const char* title = "音量";
  text.drawUTF8(TextUtils::centeredTextXInBox(text, title, popupX, popupW),
                static_cast<int16_t>(popupY + kVolumePopupTitleTopOffset), title);

  char percentText[8];
  snprintf(percentText, sizeof(percentText), "%u%%", volumePercent(music.volume()));
  text.setFont(u8g2_font_7x14B_tf);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, percentText, popupX, popupW),
                static_cast<int16_t>(popupY + kVolumePopupValueBaselineOffset), percentText);

  text.setFont(chinese_font_all);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(1);
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
                              uint8_t rowIndex, uint32_t nowMs, bool playing) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  canvas.drawFilledRectangle(kPlayerAreaX, kPlayerAreaY,
                             static_cast<int16_t>(kPlayerAreaX + kPlayerAreaWidth - 1),
                             static_cast<int16_t>(kPlayerAreaY + kPlayerAreaHeight - 1),
                             ST7305_COLOR_WHITE);

  text.setFont(chinese_font_all);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  drawMaskedScrollingText(display, playerTitleFor(music, rowIndex), kPlayerTitleX, kPlayerTitleY,
                          kPlayerTitleWidth, kPlayerTitleHeight, kPlayerTitleBaselineY, nowMs,
                          kTitleScrollPeriodMs);
  drawAudioOutputIndicator(display, music.headphonesInserted());
  drawPlayerSpectrum(display, music, nowMs, playing);
  drawPlayerProgress(display, music.currentSeconds(), music.durationSeconds());
  drawPlayerTimes(display, music.currentSeconds(), music.durationSeconds());
  drawPlayerLyrics(display, music, nowMs, music.currentSeconds(), playing);
}

void drawPlayerTransitionFrame(DisplayMonoTft& display, const MusicService& music,
                               uint16_t pageIndex, uint8_t rowIndex,
                               uint32_t nowMs, bool focusAnimating, uint8_t focusFromRow,
                               uint8_t focusToRow,
                               uint32_t focusAnimStartMs, float progress) {
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
                  focusFromRow, focusToRow, focusAnimStartMs, listOffsetX);
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

  if (volumePopupOpen_) {
    if (leftEdge || okEdge) {
      volumePopupOpen_ = false;
      return true;
    }
    if (upEdge) {
      (void)music.adjustVolume(kVolumeStep);
      return true;
    }
    if (downEdge) {
      (void)music.adjustVolume(-kVolumeStep);
      return true;
    }
    return rightEdge || upEdge || downEdge || okEdge;
  }

  if (playerViewState_ == PlayerViewState::Player) {
    if (leftEdge) {
      startPlayerTransition(false, nowMs);
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
    if (leftEdge) {
      startPlayerTransition(false, nowMs);
      return true;
    }
    return leftEdge || rightEdge || upEdge || downEdge || okEdge;
  }

  if (leftEdge) {
    const uint8_t nextIndex = (selectedIndex_ == 0) ? static_cast<uint8_t>(kTabCount - 1)
                                                    : static_cast<uint8_t>(selectedIndex_ - 1);
    startSelectionAnimation(nextIndex, nowMs);
    refreshFavoriteState(music);
    return true;
  }

  if (rightEdge) {
    startSelectionAnimation(static_cast<uint8_t>((selectedIndex_ + 1) % kTabCount), nowMs);
    refreshFavoriteState(music);
    return true;
  }

  const bool listScrollEnabled =
      selectedIndex_ == static_cast<uint8_t>(NavTab::Music) ||
      selectedIndex_ == static_cast<uint8_t>(NavTab::Favorite) ||
      selectedIndex_ == static_cast<uint8_t>(NavTab::Volume) ||
      selectedIndex_ == static_cast<uint8_t>(NavTab::Info);
  if (listScrollEnabled) {
    if (upEdge) {
      moveListFocus(-1, nowMs, music);
      refreshFavoriteState(music);
      return true;
    }
    if (downEdge) {
      moveListFocus(1, nowMs, music);
      refreshFavoriteState(music);
      return true;
    }
  }

  if (okEdge && selectedIndex_ == static_cast<uint8_t>(NavTab::Info)) {
    if (!playSelectedTrack(music)) {
      return true;
    }
    selectionAnimating_ = false;
    listFocusAnimating_ = false;
    setPlayerUiPlaying(isPlaybackActive(music), nowMs);
    playerUiShuffle_ = music.shuffleEnabled();
    startPlayerTransition(true, nowMs);
    return true;
  }

  if (okEdge && selectedIndex_ == static_cast<uint8_t>(NavTab::Volume)) {
    volumePopupOpen_ = true;
    volumePopupStartMs_ = nowMs;
    return true;
  }

  if (okEdge && selectedIndex_ == static_cast<uint8_t>(NavTab::Favorite)) {
    const MusicService::TrackInfo* track = focusedTrack(music);
    if (track == nullptr || !music.toggleFavorite(*track)) {
      return false;
    }
    refreshFavoriteState(music);
    return true;
  }

  return false;
}

bool MusicPage::handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, MusicService& music,
                                 SdCardService& sd) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return false;
  }

  selectionAnimating_ = false;
  listFocusAnimating_ = false;
  volumePopupOpen_ = false;
  music.exitMusic(sd);
  resetStateUnchecked();
  return false;
}

bool MusicPage::renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                             DisplayMonoTft& display, const MusicService& music,
                             uint32_t nowMs) {
  if (!isMusicListSelection(homeFocus, sectionFocus)) {
    return false;
  }

  const float progress = selectionAnimating_ ? selectionProgress(nowMs) : 1.0f;
  const bool listFocusAnimating = isListFocusAnimating(nowMs);

  if (playerViewState_ != PlayerViewState::List) {
    const bool playerAnimating = isPlayerTransitionAnimating(nowMs);
    const float playerProgress = playerTransitionProgress(nowMs);

    if (playerViewState_ == PlayerViewState::Leaving && !playerAnimating) {
      playerViewState_ = PlayerViewState::List;
      playerTransitionStartProgress_ = 0.0f;
      playerTransitionTargetProgress_ = 0.0f;
      drawMusicList(display, music, pageIndex_, rowIndex_, nowMs, listFocusAnimating,
                    listFocusFromRow_, listFocusToRow_, listFocusAnimStartMs_, 0);
      drawMusicNavFrame(display, selectedIndex_, favoriteEnabled_, yOffset, 1.0f);
    } else {
      drawPlayerTransitionFrame(display, music, pageIndex_, rowIndex_, nowMs,
                                listFocusAnimating, listFocusFromRow_, listFocusToRow_,
                                listFocusAnimStartMs_, playerProgress);
    }

    if (playerViewState_ == PlayerViewState::Entering && !playerAnimating) {
      playerViewState_ = PlayerViewState::Player;
      playerTransitionStartProgress_ = 1.0f;
      playerTransitionTargetProgress_ = 1.0f;
    }
    if (playerViewState_ == PlayerViewState::Player) {
      playerUiPlaying_ = isPlaybackActive(music);
      playerUiShuffle_ = music.shuffleEnabled();
      drawPlayerPreviewSurface(display, music, rowIndex_, nowMs, playerUiPlaying_);
      drawPlayerSideControls(display, playerControlIndex_, playerUiPlaying_, playerUiShuffle_);
    }
    if (!listFocusAnimating) {
      listFocusAnimating_ = false;
    }
    return true;
  }

  drawMusicList(display, music, pageIndex_, rowIndex_, nowMs, listFocusAnimating,
                listFocusFromRow_, listFocusToRow_, listFocusAnimStartMs_, 0);
  drawMusicNavFrame(display, selectedIndex_, favoriteEnabled_, yOffset, progress);
  if (volumePopupOpen_) {
    drawVolumePopup(display, music, nowMs - volumePopupStartMs_);
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
                                     uint32_t nowMs) {
  if (!isMusicListSelection(homeFocus, sectionFocus) ||
      playerViewState_ != PlayerViewState::List) {
    return false;
  }

  const bool listFocusAnimating = isListFocusAnimating(nowMs);
  clearMusicListArea(display);
  drawMusicList(display, music, pageIndex_, rowIndex_, nowMs, listFocusAnimating,
                listFocusFromRow_, listFocusToRow_, listFocusAnimStartMs_, 0);
  if (!listFocusAnimating) {
    listFocusAnimating_ = false;
  }
  return true;
}

bool MusicPage::needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                    uint32_t nowMs) const {
  return isMusicListSelection(homeFocus, sectionFocus) &&
         (selectionAnimating_ || isListFocusAnimating(nowMs) || isVolumePopupAnimating(nowMs) ||
          playerViewState_ == PlayerViewState::Entering ||
          playerViewState_ == PlayerViewState::Leaving ||
          playerViewState_ == PlayerViewState::Player);
}

bool MusicPage::needsNavAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                       uint32_t nowMs) const {
  (void)nowMs;
  return isMusicListSelection(homeFocus, sectionFocus) &&
         playerViewState_ == PlayerViewState::List && selectionAnimating_ && !volumePopupOpen_;
}

bool MusicPage::needsListAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus,
                                        uint32_t nowMs) const {
  return isMusicListSelection(homeFocus, sectionFocus) &&
         playerViewState_ == PlayerViewState::List && isListFocusAnimating(nowMs) &&
         !volumePopupOpen_;
}

uint32_t MusicPage::detailFrameIntervalMs(uint8_t homeFocus, uint8_t sectionFocus) const {
  return isMusicListSelection(homeFocus, sectionFocus) ? kAnimationFrameIntervalMs : 0U;
}

void MusicPage::startSelectionAnimation(uint8_t nextIndex, uint32_t nowMs) {
  musicLog("nav tab switch %u -> %u row=%u", static_cast<unsigned>(selectedIndex_),
           static_cast<unsigned>(nextIndex), static_cast<unsigned>(rowIndex_));
  selectedIndex_ = static_cast<uint8_t>(nextIndex % kTabCount);
  selectionAnimating_ = true;
  selectionAnimStartMs_ = nowMs;
}

void MusicPage::startPlayerTransition(bool entering, uint32_t nowMs) {
  const float currentProgress = playerTransitionProgress(nowMs);
  musicLog("player transition %s tab=%u row=%u progress=%u/100", entering ? "enter" : "leave",
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
    index = static_cast<int32_t>(music.trackCount() - 1U);
  } else if (index >= static_cast<int32_t>(music.trackCount())) {
    index = 0;
  }

  const uint16_t newPage = static_cast<uint16_t>(index / MusicService::kRowsPerPage);
  const uint8_t newRow = static_cast<uint8_t>(index % MusicService::kRowsPerPage);
  if (newPage != pageIndex_) {
    if (music.loadPage(newPage)) {
      pageIndex_ = newPage;
    }
    listFocusAnimating_ = false;
  } else if (newRow != oldRow) {
    listFocusAnimating_ = true;
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

bool MusicPage::isVolumePopupAnimating(uint32_t nowMs) const {
  if (!volumePopupOpen_) {
    return false;
  }
  return IconBitmap::frameAt(kVolumePopupWindow, nowMs - volumePopupStartMs_) + 1U <
         kVolumePopupWindow.frameCount;
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
  volumePopupOpen_ = false;
  volumePopupStartMs_ = 0;
  playerViewState_ = PlayerViewState::List;
  playerControlIndex_ = 0;
  playerUiPlaying_ = false;
  playerUiShuffle_ = false;
  playerTransitionStartProgress_ = 0.0f;
  playerTransitionTargetProgress_ = 0.0f;
  playerTransitionStartMs_ = 0;
  playerTransitionDurationMs_ = 0;
}
