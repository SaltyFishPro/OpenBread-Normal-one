#include "SystemUpdatePage.h"

#include "../../app/FirmwareInfo.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../DetailHeader.h"
#include "../DrawUtils.h"
#include "../TextUtils.h"

namespace {
constexpr int16_t kHeroX = 20;
constexpr int16_t kDetailX = 164;
constexpr int16_t kDetailWidth = 204;
constexpr int16_t kLogTop = 70;
constexpr int16_t kLogBottom = 113;
constexpr int16_t kLinePitch = 22;
const char kLocalNotes[] =
    "新增电脑状态看板\n优化 USB 连接等待\n完善答案之书记录\n"
    "新增四档电量图标\n修复蓝牙重启问题\n改善菜单切换效果";
const char kEmptyNotes[] = "本次更新暂无详细说明";

bool showsLogs(OtaService::State state) {
  return state == OtaService::State::Idle || state == OtaService::State::UpToDate ||
         state == OtaService::State::UpdateAvailable || state == OtaService::State::ReadyToApply;
}

const char* errorText(OtaService::Error error) {
  using Error = OtaService::Error;
  switch (error) {
    case Error::NoCredentials: return "请先完成 WiFi 配网";
    case Error::ConnectTimeout: return "WiFi 连接超时";
    case Error::HttpRequestFailed: return "无法连接更新服务";
    case Error::HttpStatusInvalid: return "更新服务暂不可用";
    case Error::ManifestInvalid: return "更新信息无法读取";
    case Error::ProductMismatch: return "固件型号不匹配";
    case Error::ChannelMismatch: return "更新通道不匹配";
    case Error::VersionNotNew: return "没有可用的新版本";
    case Error::UrlInvalid: return "固件下载地址无效";
    case Error::SizeInvalid: return "固件大小不符合要求";
    case Error::ShaInvalid: return "固件校验信息无效";
    case Error::DownloadFailed: return "下载中断，请重试";
    case Error::UpdateBeginFailed: return "无法准备更新空间";
    case Error::UpdateWriteFailed: return "固件写入失败";
    case Error::UpdateFinalizeFailed: return "固件保存失败";
    case Error::ShaMismatch: return "固件校验不通过";
    case Error::ApplyNotReady: return "请先完成固件下载";
    case Error::PostApplyVerifyFailed: return "重启后版本核验失败";
    case Error::Busy: return "正在处理，请稍候";
    default: return "更新失败，请重试";
  }
}

void sizeText(char* out, size_t capacity, uint32_t bytes) {
  // 整数计算，不在每帧使用浮点格式化。
  const uint32_t tenths = static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 10U) / 1048576U);
  snprintf(out, capacity, "%lu.%lu MB", static_cast<unsigned long>(tenths / 10U),
           static_cast<unsigned long>(tenths % 10U));
}

void drawFitted(U8G2_FOR_ST73XX& text, int16_t x, int16_t baseline, const char* value,
                int16_t width) {
  char line[80] = {};
  size_t used = 0;
  const char* p = value;
  while (p && *p && used + 5 < sizeof(line)) {
    const uint8_t len = TextUtils::utf8CharLen(*p);
    if (strlen(p) < len) break;
    memcpy(line + used, p, len);
    line[used + len] = '\0';
    if (text.getUTF8Width(line) > width) {
      line[used] = '\0';
      break;
    }
    used += len;
    p += len;
  }
  if (p && *p) {
    while (used && text.getUTF8Width(line) + text.getUTF8Width("...") > width) {
      --used;
      while (used && TextUtils::utf8IsContinuation(line[used])) --used;
      line[used] = '\0';
    }
    memcpy(line + used, "...", 4);
  }
  text.drawUTF8(x, baseline, line);
}
}  // namespace

SystemUpdatePage::View SystemUpdatePage::viewOf(const OtaService& ota) {
  const auto& manifest = ota.manifest();
  View view;
  view.state = ota.state();
  view.error = ota.error();
  view.version = manifest.version;
  view.notes = manifest.releaseNotes[0] ? manifest.releaseNotes : manifest.releaseNote;
  view.size = manifest.size;
  view.downloaded = ota.downloadedBytes();
  view.total = ota.totalBytes();
  view.percent = ota.progressPercent();
  view.connecting = ota.isConnecting();
  view.postApplyResult = ota.hasPostApplyResult();
  view.postApplySucceeded = ota.postApplySucceeded();
  return view;
}

void SystemUpdatePage::prepareEnter() {
  notes_ = nullptr;
  lineCount_ = page_ = previousPage_ = 0;
  active_ = logsVisible_ = animating_ = manualPause_ = false;
}

void SystemUpdatePage::syncLogs(const View& view, U8G2_FOR_ST73XX& text) {
  logsVisible_ = showsLogs(view.state);
  const char* source = view.state == OtaService::State::Idle ? kLocalNotes : view.notes;
  if (!source || !*source) source = kEmptyNotes;
  if (source == notes_ && view.state == notesState_) return;
  notes_ = source;
  notesState_ = view.state;
  lineCount_ = page_ = previousPage_ = 0;
  animating_ = manualPause_ = false;
  text.setFont(chinese_font_all);
  const char* p = source;
  while (*p && lineCount_ < kMaxLines) {
    while (*p == '\n' || *p == '\r') ++p;
    if (!*p) break;
    const char* start = p;
    uint8_t bytes = 0;
    int16_t width = 0;
    while (*p && *p != '\n' && *p != '\r') {
      const uint8_t len = TextUtils::utf8CharLen(*p);
      char glyph[5] = {};
      if (strlen(p) < len) break;
      memcpy(glyph, p, len);
      const int16_t glyphWidth = text.getUTF8Width(glyph);
      if (bytes && (width + glyphWidth > kDetailWidth || bytes + len > 63)) break;
      bytes += len;
      width += glyphWidth;
      p += len;
    }
    if (!bytes) break;
    lines_[lineCount_++] = {static_cast<uint16_t>(start - source), bytes};
  }
}

void SystemUpdatePage::startSlide(uint8_t next, int8_t direction, uint32_t nowMs) {
  previousPage_ = page_;
  page_ = next;
  direction_ = direction;
  slideStartMs_ = lastAdvanceMs_ = nowMs;
  animating_ = true;
}

bool SystemUpdatePage::update(const View& view, bool active, bool popupVisible,
                               uint32_t nowMs, U8G2_FOR_ST73XX& text) {
  if (!active) {
    active_ = animating_ = false;
    return false;
  }
  const char* oldNotes = notes_;
  const auto oldState = notesState_;
  syncLogs(view, text);
  const bool reset = !active_ || notes_ != oldNotes || notesState_ != oldState;
  active_ = true;
  if (reset || popupVisible || !logsVisible_) {
    lastAdvanceMs_ = nowMs;
    const bool wasAnimating = animating_;
    animating_ = false;
    return reset || wasAnimating;
  }
  if (animating_ && nowMs - slideStartMs_ >= kSlideMs) {
    animating_ = false;
    return true;  // 提交最后一帧后停止绘制。
  }
  if (manualPause_ && nowMs - lastManualMs_ < kManualPauseMs) return false;
  manualPause_ = false;
  if (!animating_ && logPageCount() > 1 && nowMs - lastAdvanceMs_ >= kAutoMs) {
    startSlide(static_cast<uint8_t>((page_ + 1U) % logPageCount()), 1, nowMs);
    return true;
  }
  return false;
}

bool SystemUpdatePage::handleLogInput(bool up, bool down, uint32_t nowMs) {
  if ((!up && !down) || !logsVisible_ || logPageCount() < 2) return false;
  lastManualMs_ = lastAdvanceMs_ = nowMs;
  manualPause_ = true;
  if (up && page_ > 0) startSlide(static_cast<uint8_t>(page_ - 1), -1, nowMs);
  else if (down && page_ + 1 < logPageCount()) startSlide(static_cast<uint8_t>(page_ + 1), 1, nowMs);
  return true;
}

void SystemUpdatePage::drawLogPage(DisplayMonoTft& display, uint8_t page, int16_t yOffset) {
  for (uint8_t row = 0; row < 2; ++row) {
    const uint8_t index = static_cast<uint8_t>(page * 2U + row);
    if (index >= lineCount_) break;
    char line[64] = {};
    memcpy(line, notes_ + lines_[index].offset, lines_[index].bytes);
    display.text().drawUTF8(kDetailX, static_cast<int16_t>(yOffset + 86 + row * kLinePitch), line);
  }
}

void SystemUpdatePage::render(DisplayMonoTft& display, int16_t yOffset, const View& view,
                               uint32_t nowMs) {
  using State = OtaService::State;
  auto& text = display.text();
  auto& canvas = display.canvas();
  syncLogs(view, text);
  text.setFont(chinese_font_all);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  if (logsVisible_) {
    if (animating_) {
      uint32_t t = nowMs - slideStartMs_;
      if (t > kSlideMs) t = kSlideMs;
      // 缓出曲线，仅在 550ms 滚动期间计算。
      const uint32_t remaining = kSlideMs - t;
      const int16_t shift = static_cast<int16_t>(44U - 44U * remaining * remaining / (kSlideMs * kSlideMs));
      drawLogPage(display, previousPage_, static_cast<int16_t>(yOffset - direction_ * shift));
      drawLogPage(display, page_, static_cast<int16_t>(yOffset + direction_ * (44 - shift)));
    } else {
      drawLogPage(display, page_, yOffset);
    }
    // 整帧绘制时遮去视口上下的字形，再画标题/状态/页码，避免滚动残留。
    DrawUtils::drawClippedFilledRect(canvas, kDetailX, yOffset, 383,
                                    static_cast<int16_t>(yOffset + kLogTop - 1), ST7305_COLOR_WHITE);
    DrawUtils::drawClippedFilledRect(canvas, kDetailX, static_cast<int16_t>(yOffset + kLogBottom + 1),
                                    383, static_cast<int16_t>(yOffset + 167), ST7305_COLOR_WHITE);
  }
  DetailHeader::render(display, "系统更新", yOffset);
  text.setFont(u8g2_font_7x14_tf);
  text.drawUTF8(kHeroX, yOffset + 54, "Normal One");

  const bool transfer = view.state == State::Downloading || view.state == State::Verifying;
  const bool success = view.state == State::UpToDate ||
      (view.state == State::Idle && view.postApplyResult && view.postApplySucceeded);
  const char* version = (view.state != State::Idle && view.state != State::Checking &&
                          view.state != State::Error && view.version && *view.version)
                           ? view.version : FirmwareInfo::kVersion;
  char number[24];
  snprintf(number, sizeof(number), "%u%%", static_cast<unsigned>(view.percent));
  if (success) {
    canvas.drawCircle(66, yOffset + 87, 20, ST7305_COLOR_BLACK);
    canvas.drawLine(55, yOffset + 87, 63, yOffset + 95, ST7305_COLOR_BLACK);
    canvas.drawLine(63, yOffset + 95, 79, yOffset + 78, ST7305_COLOR_BLACK);
    canvas.drawLine(55, yOffset + 88, 63, yOffset + 96, ST7305_COLOR_BLACK);
    canvas.drawLine(63, yOffset + 96, 79, yOffset + 79, ST7305_COLOR_BLACK);
  } else {
    const char* big = transfer ? number : version;
    text.setFont(u8g2_font_logisoso32_tr);
    if (text.getUTF8Width(big) > 128) text.setFont(u8g2_font_logisoso24_tr);
    drawFitted(text, kHeroX, yOffset + 103, big, 128);
  }
  text.setFont(chinese_font_all);
  char current[64];
  snprintf(current, sizeof(current), "当前 %s", FirmwareInfo::kVersion);
  drawFitted(text, kHeroX, yOffset + 130, current, 128);

  const char* status = "本版更新日志";
  const char* action = "OK 检查更新";
  const char* line1 = "";
  const char* line2 = "";
  const bool busy = view.state == State::Checking || transfer || view.state == State::Applying;
  switch (view.state) {
    case State::Checking:
      status = "正在检查更新";
      line1 = view.connecting ? "正在连接 WiFi" : "正在获取更新信息";
      line2 = "请稍候，可按 Left 取消";
      action = "检查中";
      break;
    case State::UpdateAvailable: status = "发现新版本"; action = "OK 下载更新"; break;
    case State::UpToDate: status = "已是最新版本"; break;
    case State::Downloading:
      status = view.connecting ? "正在连接 WiFi" : "正在下载更新";
      action = "下载中";
      break;
    case State::Verifying: status = "正在校验固件"; action = "校验中"; break;
    case State::ReadyToApply: status = "更新已准备就绪"; action = "OK 重启更新"; break;
    case State::Applying:
      status = "正在重启更新"; line1 = "设备即将重新启动"; line2 = "请勿断开电源"; action = "重启中"; break;
    case State::Error:
      status = "更新未完成"; line1 = errorText(view.error);
      line2 = view.error == OtaService::Error::NoCredentials ? "Left 返回设置进行配网" : "按 OK 重新检查更新";
      action = "OK 重试";
      break;
    default:
      if (view.postApplyResult) status = view.postApplySucceeded ? "系统更新成功" : "更新核验失败";
      break;
  }
  text.drawUTF8(kDetailX, yOffset + 56, status);
  char meta[80] = {};
  if (transfer) {
    DrawUtils::drawClippedRect(canvas, kDetailX, yOffset + 77, 367, yOffset + 86, ST7305_COLOR_BLACK);
    const int16_t fill = static_cast<int16_t>(202U * (view.percent > 100 ? 100 : view.percent) / 100U);
    for (int16_t y = 0; y < 8; ++y) {
      for (int16_t x = 0; x < fill; x += 6) {
        if ((x + y) % 6 < 3) {
          const int16_t run = static_cast<int16_t>((x + 3 < fill) ? 3 : (fill - x));
          if (run > 0) {
            canvas.drawFastHLine(kDetailX + 1 + x, yOffset + 78 + y, run,
                                 ST7305_COLOR_BLACK);
          }
        }
      }
    }
    char downloaded[20], total[20];
    sizeText(downloaded, sizeof(downloaded), view.downloaded);
    sizeText(total, sizeof(total), view.total);
    snprintf(meta, sizeof(meta), "%s / %s", downloaded, total);
    text.setFont(u8g2_font_7x14_tf);
    text.drawUTF8(kDetailX, yOffset + 107, meta);
    text.setFont(chinese_font_all);
    text.drawUTF8(kDetailX, yOffset + 130, "请勿断开电源");
  } else if (logsVisible_) {
    const char* label = view.state == State::Idle ? "本版日志" : "更新说明";
    if (view.state == State::UpdateAvailable) {
      char size[20];
      sizeText(size, sizeof(size), view.size);
      snprintf(meta, sizeof(meta), "%s  %s %u/%u", size, label, page_ + 1, logPageCount());
    } else {
      snprintf(meta, sizeof(meta), "%s %u/%u", label, page_ + 1, logPageCount());
    }
    drawFitted(text, kDetailX, yOffset + 130, meta, kDetailWidth);
  } else {
    drawFitted(text, kDetailX, yOffset + 86, line1, kDetailWidth);
    drawFitted(text, kDetailX, yOffset + 108, line2, kDetailWidth);
    text.drawUTF8(kDetailX, yOffset + 130, busy ? "正在处理更新" : "可随时重新检查");
  }
  canvas.drawFastHLine(16, yOffset + 136, 352, ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setFontMode(0);
  text.drawUTF8(16, yOffset + 22, busy && view.state != State::Applying ? "Left 取消" : "Left 返回");
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
  const int16_t actionWidth = static_cast<int16_t>(text.getUTF8Width(action) + 16);
  const int16_t actionX = static_cast<int16_t>(368 - actionWidth);
  DrawUtils::drawRoundRect(canvas, actionX, yOffset + 141, actionWidth, 24, 6,
                           busy ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
  text.setForegroundColor(busy ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
  text.setBackgroundColor(busy ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
  text.setFontMode(0);
  text.drawUTF8(actionX + 8, yOffset + 158, action);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setFontMode(1);
}
