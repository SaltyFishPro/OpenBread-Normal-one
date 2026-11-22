#include "DeviceSelfTestPage.h"

#include <cstdio>

#include "../../bsp/DisplayMonoTft.h"
#include "../../log/Log.h"
#include "../DrawUtils.h"
#include "../TextUtils.h"
#include "../SdUiText.h"
#include "../../services/IicScanService.h"
#include "../../services/RtcTestService.h"
#include "../../services/SdCardService.h"
#include "../../services/SdBitmapFont.h"
#include "../../services/ImuTestService.h"
#include "../../services/PowerDiagnosticService.h"
#include "../../services/AudioTestService.h"
#include "../../services/MotorTestService.h"
#include "../../bsp/PeripheralPower.h"

namespace {
constexpr uint8_t kPageSize = 5;
constexpr int16_t kHeaderHeight = 28;
constexpr int16_t kListX = 14;
constexpr int16_t kFirstBaselineY = 51;
constexpr int16_t kRowStep = 23;
constexpr int16_t kTextHeight = 14;
constexpr int16_t kFocusPadX = 7;
constexpr int16_t kFocusPadY = 4;
constexpr int16_t kDividerX = 164;
constexpr int16_t kDescriptionX = 184;
constexpr int16_t kDescriptionFirstBaselineY = 70;
constexpr int16_t kDescriptionLineStep = 26;
constexpr int16_t kFooterHeight = 18;
constexpr uint8_t kScreenTestItemIndex = 1;
constexpr uint8_t kButtonTestItemIndex = 2;
constexpr uint8_t kIicTestItemIndex = 3;
constexpr uint8_t kRtcTestItemIndex = 4;
constexpr uint8_t kSdTestItemIndex = 5;
constexpr uint8_t kAudioTestItemIndex = 6;
constexpr uint8_t kMotorTestItemIndex = 7;
constexpr uint8_t kImuTestItemIndex = 8;
constexpr uint8_t kPowerTestItemIndex = 10;
constexpr uint8_t kFontTestItemIndex = 11;
constexpr uint8_t kScreenPatternCount = 8;
constexpr uint8_t kAllScreenPatternsViewed = 0xFFU;
constexpr uint8_t kAllButtonsCompleted = 0x1FU;
constexpr int16_t kPatternFooterHeight = 22;

const char* const kScreenPatternNames[kScreenPatternCount] = {
    "全白", "全黑", "边框十字", "横向条纹",
    "纵向条纹", "棋盘格", "文字样张", "动态刷新"};

const char* const kButtonNames[5] = {"Left", "Right", "Up", "Down", "OK"};

struct SelfTestItemText {
  const char* label;
  const char* description[3];
};

const SelfTestItemText kSelfTestItems[DeviceSelfTestPage::kItemCount] = {
    {"快速自检", {"检查基础硬件", "汇总设备状态", "不执行高耗能测试"}},
    {"屏幕测试", {"显示测试图案", "检查坏点残影", "确认方向与边界"}},
    {"按键测试", {"检测五个按键", "检查按下与释放", "识别卡键异常"}},
    {"IIC设备测试", {"扫描IIC总线", "检查设备响应", "显示发现的地址"}},
    {"RTC测试", {"读取实时时钟", "检查时间递增", "不修改当前时间"}},
    {"SD卡测试", {"检查插卡状态", "读取容量信息", "默认不写入文件"}},
    {"音频测试", {"播放短提示音", "检查音频输出", "完成后关闭音频"}},
    {"马达测试", {"选择触觉效果", "检查马达响应", "完成后立即停止"}},
    {"IMU测试", {"读取运动数据", "检查姿态变化", "识别异常固定值"}},
    {"无线测试", {"检查蓝牙与WiFi", "显示连接状态", "完成后关闭无线"}},
    {"电源测试", {"读取电池电量", "检查MAX17048", "显示LDO当前状态"}},
    {"SD字库测试", {"读取Font文件夹", "预览三个字号", "检查中文与符号"}},
};

uint8_t pageCount() {
  return static_cast<uint8_t>((DeviceSelfTestPage::kItemCount + kPageSize - 1U) / kPageSize);
}
}  // namespace

void DeviceSelfTestPage::reset() {
  view_ = View::List;
  focusIndex_ = 0;
  focusAnimating_ = false;
  screenPatternIndex_ = 0;
  viewedScreenPatterns_ = 0;
  confirmScreenNormal_ = true;
  screenResult_ = ScreenResult::NotRun;
  lastButtonMask_ = 0;
  pressedButtonMask_ = 0;
  completedButtonMask_ = 0;
  buttonTestPassed_ = false;
  fpsWindowStartMs_ = 0;
  fpsFrameCount_ = 0;
  displayedFps_ = 0;
  rtcPageIndex_ = 0;
  imuPageIndex_ = 0;
}

bool DeviceSelfTestPage::handleInput(bool leftEdge, bool leftLongEdge, bool upEdge,
                                     bool downEdge, bool okEdge,
                                     const ButtonState& buttons, uint32_t nowMs,
                                     IicScanService& iicScanService,
                                     RtcTestService& rtcTestService, SdCardService& sdCardService,
                                     ImuTestService& imuTestService,
                                     PowerDiagnosticService& powerDiagnosticService,
                                     AudioTestService& audioTestService,
                                     MotorTestService& motorTestService,
                                     PeripheralPower& peripheralPower, SdBitmapFont& font,
                                     bool musicBusy) {
  if (view_ == View::FontTest) {
    if (leftEdge || leftLongEdge) {
      font.end();
      sdCardService.end();
      view_ = View::List;
      return true;
    }
    return fontTestPage_.handleInput(upEdge, downEdge, okEdge, font, sdCardService);
  }
  if (view_ == View::ButtonTest) {
    const bool changed = updateButtonTest(buttons);
    if (leftLongEdge) {
      LogI("SELFTEST/BUTTON", "cancel completed=0x%02X", static_cast<unsigned>(completedButtonMask_));
      view_ = View::List;
      return true;
    }
    if (buttonTestPassed_ && okEdge) {
      view_ = View::List;
      return true;
    }
    return changed || leftEdge || upEdge || downEdge || okEdge;
  }

  if (view_ == View::IicTest) {
    if (leftEdge || leftLongEdge) {
      iicScanService.stop();
      view_ = View::List;
      return true;
    }
    if (okEdge) {
      return iicScanService.start(nowMs);
    }
    return false;
  }

  if (view_ == View::RtcTest) {
    if (leftEdge || leftLongEdge) {
      rtcTestService.cancel();
      view_ = View::List;
      return true;
    }
    if (upEdge || downEdge) {
      rtcPageIndex_ = static_cast<uint8_t>(1U - rtcPageIndex_);
      return true;
    }
    if (okEdge) {
      return rtcTestService.start(nowMs);
    }
    return false;
  }
  if(view_==View::SdTest){if(leftEdge||leftLongEdge){sdCardService.end();view_=View::List;return true;}if(okEdge){sdCardService.refresh();return true;}return false;}
  if (view_ == View::ImuTest) {
    if (leftEdge || leftLongEdge) {
      imuTestService.cancel();
      view_ = View::List;
      return true;
    }
    if (upEdge || downEdge) {
      imuPageIndex_ = imuPageIndex_ >= 2U ? 0 : static_cast<uint8_t>(imuPageIndex_ + 1U);
      return true;
    }
    if (okEdge) {
      if (imuPageIndex_ == 2U) {
        (void)imuTestService.calibrate();
      } else {
        (void)imuTestService.start(nowMs);
      }
      return true;
    }
    return false;
  }
  if (view_ == View::PowerTest) {
    if (leftEdge || leftLongEdge) {
      view_ = View::List;
      return true;
    }
    if (upEdge || downEdge) {
      powerPageIndex_ = static_cast<uint8_t>(1U - powerPageIndex_);
      return true;
    }
    if (okEdge) {
      (void)powerDiagnosticService.run(peripheralPower);
      return true;
    }
    return false;
  }

  if (view_ == View::AudioTest) {
    if (leftEdge || leftLongEdge) {
      audioTestService.cancel();
      view_ = View::List;
      return true;
    }
    if (upEdge || downEdge) {
      if (audioTestService.state() == AudioTestService::State::WaitingConfirm) {
        audioTestService.toggleChoice();
        return true;
      }
      return false;
    }
    if (okEdge) {
      if (audioTestService.state() == AudioTestService::State::WaitingConfirm) {
        return audioTestService.confirm(nowMs);
      }
      if (!audioTestService.isActive()) {
        const bool started = audioTestService.start(nowMs, peripheralPower, musicBusy);
        return started || audioTestService.error() != AudioTestService::Error::None;
      }
    }
    return false;
  }

  if (view_ == View::MotorTest) {
    if (leftEdge || leftLongEdge) {
      motorTestService.cancel();
      view_ = View::List;
      return true;
    }
    if (upEdge || downEdge) {
      if (motorTestService.isActive()) {
        return false;
      }
      if (upEdge) {
        motorTestService.selectPrev();
      } else {
        motorTestService.selectNext();
      }
      return true;
    }
    if (okEdge && !motorTestService.isActive()) {
      const bool started = motorTestService.play(nowMs);
      return started || motorTestService.error() != MotorTestService::Error::None;
    }
    return false;
  }

  if (view_ != View::List && (leftEdge || leftLongEdge)) {
    LogI("SELFTEST/DISPLAY", "cancel pattern=%u", static_cast<unsigned>(screenPatternIndex_ + 1U));
    view_ = View::List;
    return true;
  }

  if (view_ == View::ScreenIntro) {
    if (okEdge) {
      viewedScreenPatterns_ = 0;
      selectScreenPattern(0);
      view_ = View::ScreenPattern;
      LogI("SELFTEST/DISPLAY", "start patterns=%u", static_cast<unsigned>(kScreenPatternCount));
      return true;
    }
    return false;
  }

  if (view_ == View::ScreenPattern) {
    if (upEdge) {
      const uint8_t next = screenPatternIndex_ == 0
                               ? static_cast<uint8_t>(kScreenPatternCount - 1U)
                               : static_cast<uint8_t>(screenPatternIndex_ - 1U);
      selectScreenPattern(next);
      return true;
    }
    if (downEdge) {
      selectScreenPattern(static_cast<uint8_t>((screenPatternIndex_ + 1U) %
                                               kScreenPatternCount));
      return true;
    }
    if (okEdge) {
      if (viewedScreenPatterns_ == kAllScreenPatternsViewed) {
        confirmScreenNormal_ = true;
        view_ = View::ScreenConfirm;
      }
      return true;
    }
    return false;
  }

  if (view_ == View::ScreenConfirm) {
    if (upEdge || downEdge) {
      confirmScreenNormal_ = !confirmScreenNormal_;
      return true;
    }
    if (okEdge) {
      screenResult_ = confirmScreenNormal_ ? ScreenResult::Passed : ScreenResult::Failed;
      LogI("SELFTEST/DISPLAY", "finish result=%s", confirmScreenNormal_ ? "passed" : "failed");
      view_ = View::List;
      return true;
    }
    return false;
  }

  if (upEdge) {
    const uint8_t previous = focusIndex_;
    focusIndex_ = focusIndex_ == 0 ? static_cast<uint8_t>(kItemCount - 1U)
                                   : static_cast<uint8_t>(focusIndex_ - 1U);
    focusFromY_ = static_cast<int16_t>(40 + (previous % kPageSize) * 21);
    focusAnimating_ = previous / kPageSize == focusIndex_ / kPageSize;
    focusStartMs_ = nowMs;
    return true;
  }
  if (downEdge) {
    const uint8_t previous = focusIndex_;
    focusIndex_ = static_cast<uint8_t>((focusIndex_ + 1U) % kItemCount);
    focusFromY_ = static_cast<int16_t>(40 + (previous % kPageSize) * 21);
    focusAnimating_ = previous / kPageSize == focusIndex_ / kPageSize;
    focusStartMs_ = nowMs;
    return true;
  }
  if (okEdge && focusIndex_ == kScreenTestItemIndex) {
    view_ = View::ScreenIntro;
    return true;
  }
  if (okEdge && focusIndex_ == kButtonTestItemIndex) {
    lastButtonMask_ = buttonMask(buttons);
    pressedButtonMask_ = 0;
    completedButtonMask_ = 0;
    buttonTestPassed_ = false;
    view_ = View::ButtonTest;
    LogI("SELFTEST/BUTTON", "start initial_mask=0x%02X", static_cast<unsigned>(lastButtonMask_));
    return true;
  }
  if (okEdge && focusIndex_ == kIicTestItemIndex) {
    view_ = View::IicTest;
    iicScanService.start(nowMs);
    return true;
  }
  if (okEdge && focusIndex_ == kRtcTestItemIndex) {
    view_ = View::RtcTest;
    rtcPageIndex_ = 0;
    rtcTestService.start(nowMs);
    return true;
  }
  if(okEdge&&focusIndex_==kSdTestItemIndex){view_=View::SdTest;sdCardService.refresh();return true;}
  if (okEdge && focusIndex_ == kAudioTestItemIndex) {
    view_ = View::AudioTest;
    return true;
  }
  if (okEdge && focusIndex_ == kMotorTestItemIndex) {
    view_ = View::MotorTest;
    return true;
  }
  if (okEdge && focusIndex_ == kImuTestItemIndex) {
    view_ = View::ImuTest;
    imuPageIndex_ = 0;
    imuTestService.start(nowMs);
    return true;
  }
  if (okEdge && focusIndex_ == kPowerTestItemIndex) {
    view_ = View::PowerTest;
    powerPageIndex_ = 0;
    (void)powerDiagnosticService.run(peripheralPower);
    return true;
  }
  if (okEdge && focusIndex_ == kFontTestItemIndex) {
    view_ = View::FontTest;
    fontTestPage_.begin(font, sdCardService);
    return true;
  }
  return false;
}

void DeviceSelfTestPage::render(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs,
                                const IicScanService& iicScanService,
                                const RtcTestService& rtcTestService,const SdCardService& sdCardService,
                                const ImuTestService& imuTestService,
                                const PowerDiagnosticService& powerDiagnosticService,
                                const AudioTestService& audioTestService,
                                const MotorTestService& motorTestService, SdBitmapFont& font,
                                SdBitmapFont* ui20) {
  if (view_ == View::FontTest) {
    fontTestPage_.render(display, yOffset, font, ui20);
    return;
  }
  if (view_ == View::ScreenPattern) {
    renderScreenPattern(display, yOffset, nowMs);
    return;
  }
  if (view_ == View::List) {
    renderList(display, yOffset, nowMs, ui20);
    return;
  }
  renderModern(display, yOffset, ui20, iicScanService, rtcTestService, sdCardService,
               imuTestService, powerDiagnosticService, audioTestService, motorTestService);
}

bool DeviceSelfTestPage::needsAnimationFrame() const {
  return (view_ == View::ScreenPattern && screenPatternIndex_ == 7U) ||
         (view_ == View::List && focusAnimating_);
}

void DeviceSelfTestPage::renderList(DisplayMonoTft& display, int16_t yOffset,
                                        uint32_t nowMs, SdBitmapFont* ui20) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  if (yOffset >= display.height()) return;
  DrawUtils::drawClippedFilledRect(canvas, 0, yOffset, display.width()-1,
                                  yOffset+display.height()-1, ST7305_COLOR_WHITE);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(1);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  SdUiText::draw(display, ui20, 16, yOffset+26, "设备自检", ST7305_COLOR_BLACK,
                 {16, 0, 300, static_cast<int16_t>(display.height()-1)});
  char count[16];
  snprintf(count, sizeof(count), "%02u / 12", static_cast<unsigned>(focusIndex_+1));
  text.drawUTF8(display.width()-16-text.getUTF8Width(count), yOffset+25, count);
  DrawUtils::drawClippedFilledRect(canvas, 16, yOffset+35, display.width()-17, yOffset+35,
                                  ST7305_COLOR_BLACK);
  DrawUtils::drawClippedFilledRect(canvas, 163, yOffset+43, 163, yOffset+139,
                                  ST7305_COLOR_BLACK);
  const uint8_t start = (focusIndex_ / kPageSize) * kPageSize;
  const int16_t target = 40 + (focusIndex_ % kPageSize) * 21;
  const uint32_t elapsed = nowMs - focusStartMs_;
  int16_t marker = target;
  if (focusAnimating_ && elapsed < 180U) {
    const uint32_t remaining = 180U - elapsed;
    const uint32_t eased = 180U - (remaining * remaining / 180U);
    marker = focusFromY_ + (target - focusFromY_) * static_cast<int32_t>(eased) / 180;
  } else if (focusAnimating_) {
    focusAnimating_ = false;
  }
  DrawUtils::drawRoundRect(canvas, 12, yOffset+marker, 140, 20, 8,
                           ST7305_COLOR_BLACK, ST7305_COLOR_BLACK);
  for (uint8_t row=0; row<kPageSize && start+row<kItemCount; ++row) {
    const int16_t rowY = 40 + row*21;
    const bool selected = marker >= rowY-10 && marker <= rowY+10;
    text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
    text.drawUTF8(24, yOffset+rowY+15, kSelfTestItems[start+row].label);
  }
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(181, yOffset+57, "检测说明");
  for (uint8_t i=0; i<3; ++i)
    text.drawUTF8(181, yOffset+80+i*22, kSelfTestItems[focusIndex_].description[i]);
  DrawUtils::drawClippedFilledRect(canvas, 16, yOffset+146, display.width()-17, yOffset+146,
                                  ST7305_COLOR_BLACK);
  text.drawUTF8(16, yOffset+162, "↑↓ 选择");
  const char* ok = "OK 进入";
  text.drawUTF8(display.width()-16-text.getUTF8Width(ok), yOffset+162, ok);
}

void DeviceSelfTestPage::renderModern(DisplayMonoTft& d, int16_t y, SdBitmapFont* font,
    const IicScanService& iic, const RtcTestService& rtc, const SdCardService& sd,
    const ImuTestService& imu, const PowerDiagnosticService& power,
    const AudioTestService& audio, const MotorTestService& motor) const {
  if (y >= d.height()) return;
  auto& c = d.canvas(); auto& t = d.text();
  const uint16_t black=ST7305_COLOR_BLACK, white=ST7305_COLOR_WHITE;
  DrawUtils::drawClippedFilledRect(c,0,y,d.width()-1,y+d.height()-1,white);
  t.setFont(u8g2_font_7x14_tf); t.setFontMode(1); t.setForegroundColor(black); t.setBackgroundColor(white);
  const char* title=kSelfTestItems[focusIndex_].label;
  SdUiText::draw(d,font,16,y+26,title,black,{16,0,280,static_cast<int16_t>(d.height()-1)});
  auto line=[&](int x,int yy,int w) { DrawUtils::drawClippedFilledRect(c,x,y+yy,x+w-1,y+yy,black); };
  auto small=[&](const char* s,int x,int yy) { t.drawUTF8(x,y+yy,s); };
  auto right=[&](const char* s,int yy) { small(s,368-t.getUTF8Width(s),yy); };
  auto footer=[&](const char* left,const char* r) {line(16,145,352);small(left,16,162);right(r,162);};
  auto pill=[&](const char* s) {
    int width=t.getUTF8Width(s)+25; if(width>352)width=352;
    DrawUtils::drawRoundRect(c,16,y+43,width,23,8,black,black);
    t.setForegroundColor(white);small(s,28,60);t.setForegroundColor(black);
  };
  auto pair=[&](const char* label,const char* value,int yy) {small(label,24,yy);right(value,yy);line(24,yy+8,328);};
  auto choice=[&](const char* prompt,bool first,const char* a,const char* b) {
    small(prompt,24,103);
    const char* names[]={a,b};
    for (int i=0;i<2;++i) {
      const int x=192+i*82; const bool selected=(i==0)==first;
      if(selected) DrawUtils::drawRoundRect(c,x,y+85,72,25,8,black,black);
      else DrawUtils::drawClippedRect(c,x,y+85,x+71,y+109,black);
      t.setForegroundColor(selected?white:black);
      small(names[i],x+(72-t.getUTF8Width(names[i]))/2,103);
    }
    t.setForegroundColor(black);
  };
  char linebuf[96];
  if(view_==View::ScreenIntro) {
    right("01 / 08",24); line(16,34,352); pill("检查坏点、残影和显示方向");
    small("依次查看 8 种全屏测试图案",24,94);small("全部查看后，确认屏幕显示是否正常",24,119);
    footer("Left 返回","OK 开始");return;
  }
  if(view_==View::ScreenConfirm) {
    right("完成检查",24);line(16,34,352);pill("8 种图案已查看");
    choice("屏幕显示正常吗？",confirmScreenNormal_,"正常","异常");footer("↑↓ 选择","OK 确认");return;
  }
  if(view_==View::ButtonTest) {
    right("按下并释放",24);line(16,34,352);
    for(uint8_t i=0;i<5;++i) {
      int x=16+i*71; bool pressed=(lastButtonMask_&(1U<<i))!=0;
      if(pressed)DrawUtils::drawRoundRect(c,x,y+47,64,59,8,black,black);
      else DrawUtils::drawClippedRect(c,x,y+47,x+63,y+105,black);
      t.setForegroundColor(pressed?white:black);small(kButtonNames[i],x+(64-t.getUTF8Width(kButtonNames[i]))/2,70);
      const char* state=(completedButtonMask_&(1U<<i))?"已通过":"待测试";
      small(state,x+(64-t.getUTF8Width(state))/2,94);t.setForegroundColor(black);
    }
    small(buttonTestPassed_?"全部按键通过":"依次按下并释放全部按键",24,133);
    footer("长按 Left 取消",buttonTestPassed_?"OK 返回":"");return;
  }
  if(view_==View::IicTest) {
    right("总线扫描",24);line(16,34,352);
    const auto state=iic.state();const auto& result=iic.result();
    if(state==IicScanService::State::Scanning)snprintf(linebuf,sizeof(linebuf),"正在扫描 0x%02X / 0x77",iic.currentAddress());
    else if(state==IicScanService::State::Scanned)snprintf(linebuf,sizeof(linebuf),"扫描完成：发现 %u 个设备",result.deviceCount);
    else snprintf(linebuf,sizeof(linebuf),"%s",state==IicScanService::State::Error?"总线初始化失败":"正在初始化总线");
    pill(linebuf);small("发现地址",24,91);
    snprintf(linebuf,sizeof(linebuf),"错误:%u",result.errorCount);
    right(linebuf,91);
    for(uint8_t i=0;i<result.deviceCount && i<10;++i){snprintf(linebuf,sizeof(linebuf),"0x%02X",result.addresses[i]);small(linebuf,24+(i%5)*68,112+(i/5)*22);}
    footer("Left 返回","OK 重新扫描");return;
  }
  if(view_==View::RtcTest) {
    snprintf(linebuf,sizeof(linebuf),"%u / 2",rtcPageIndex_+1);right(linebuf,24);line(16,34,352);
    const char* status=rtc.state()==RtcTestService::State::Failed?"RTC 测试失败":rtc.state()==RtcTestService::State::Waiting?"正在等待时间递增":rtc.state()==RtcTestService::State::Warning?"走时正常，存在配置建议":"走时与省电配置正常";
    if(rtc.state()==RtcTestService::State::Failed) {
      switch(rtc.error()) {
        case RtcTestService::Error::Unavailable: status="RTC 设备不可用"; break;
        case RtcTestService::Error::RegisterRead: status="配置寄存器读取失败"; break;
        case RtcTestService::Error::FirstTimeRead:
        case RtcTestService::Error::SecondTimeRead: status="时间寄存器读取失败"; break;
        case RtcTestService::Error::InvalidTime: status="日期或时间无效"; break;
        case RtcTestService::Error::NotAdvancing: status="RTC 时间未递增"; break;
        default: break;
      }
    }
    pill(status); const auto& tm=rtc.latestTime(); const auto& diag=rtc.diagnostics();
    if(!rtcPageIndex_){snprintf(linebuf,sizeof(linebuf),"%02u:%02u:%02u",tm.hour,tm.minute,tm.second);pair("当前时间",linebuf,94);snprintf(linebuf,sizeof(linebuf),"20%02u-%02u-%02u",tm.year,tm.month,tm.day);pair("当前日期",linebuf,128);}
    else {snprintf(linebuf,sizeof(linebuf),"Control1 0x%02X  Control2 0x%02X",diag.control1,diag.control2);small(linebuf,24,90);snprintf(linebuf,sizeof(linebuf),"Offset 0x%02X  建议项 %u",diag.offset,__builtin_popcount(rtc.warnings()));small(linebuf,24,116);small(rtc.warnings()?"存在配置建议，请检查日志":"CLKOUT 与定时器已关闭",24,138);}
    footer("↑↓ 翻页  Left 返回","OK 重测");return;
  }
  if(view_==View::SdTest) {
    right("只读检查",24);line(16,34,352);
    pill(sd.status()==SdCardService::Status::Ready?(sd.rootReadable()?"只读测试通过":"根目录读取失败"):"SD 卡不可用");
    snprintf(linebuf,sizeof(linebuf),"%llu MB",static_cast<unsigned long long>(sd.totalBytes()/1048576ULL));pair("总容量",linebuf,94);
    snprintf(linebuf,sizeof(linebuf),"%llu MB",static_cast<unsigned long long>(sd.freeBytes()/1048576ULL));pair("可用容量",linebuf,128);
    footer("Left 返回","OK 重试");return;
  }
  if(view_==View::AudioTest) {
    right(audio.headphonesInserted()?"耳机":"扬声器",24);line(16,34,352);
    const bool waiting=audio.state()==AudioTestService::State::WaitingConfirm;
    const bool active=audio.isActive();
    const bool passed=audio.state()==AudioTestService::State::Passed;
    const bool failed=audio.state()==AudioTestService::State::Failed;
    const char* status=waiting?"提示音播放结束":active?"正在播放提示音":passed?"测试通过":failed?"测试失败，请重试":"按 OK 播放提示音";
    if(failed) {
      switch(audio.error()) {
        case AudioTestService::Error::MusicBusy: status="音乐播放中，请先退出"; break;
        case AudioTestService::Error::DacPower: status="音频供电或静音控制失败"; break;
        case AudioTestService::Error::ToneInit: status="音频硬件初始化失败"; break;
        case AudioTestService::Error::ToneWrite: status="音频数据写入失败"; break;
        case AudioTestService::Error::NotHeard: status="未确认听到提示音"; break;
        case AudioTestService::Error::Timeout: status="测试超时"; break;
        default: break;
      }
    }
    pill(status);
    if(waiting)choice("听到提示音了吗？",audio.heardSelected(),"是","否");
    else {small("1 kHz · 500 ms · 固定低音量",24,101);small(failed?"请检查硬件或查看串口日志":"请留意设备声音",24,128);}
    footer(waiting?"↑↓ 选择  Left 返回":"Left 返回",waiting?"OK 确认":active?"正在测试":"OK 播放");return;
  }
  if(view_==View::MotorTest) {
    const bool active=motor.isActive();
    const bool finished=motor.state()==MotorTestService::State::Finished;
    const bool failed=motor.state()==MotorTestService::State::Failed;
    const char* status="LRA 就绪";
    if(active)status="LRA 播放中";
    else if(finished)status="LRA 已完成";
    else if(failed) {
      switch(motor.error()) {
        case MotorTestService::Error::StartFailed: status="LRA 启动失败"; break;
        case MotorTestService::Error::Timeout: status="LRA 超时"; break;
        case MotorTestService::Error::ChipNoResponse: status="LRA 无响应"; break;
        case MotorTestService::Error::ChipConfig: status="LRA 配置失败"; break;
        default: status="LRA 失败"; break;
      }
    }
    right(status,24);line(16,34,352);
    for(uint8_t i=0;i<MotorTestService::kModeCount;++i) {
      const int16_t rowY=54+i*21;
      const bool selected=i==motor.selectedIndex();
      if(selected)DrawUtils::drawRoundRect(c,14,y+rowY-15,356,20,8,black,black);
      t.setForegroundColor(selected?white:black);
      small(motor.modeName(i),24,rowY);
      right(motor.modeHint(i),rowY);
      t.setForegroundColor(black);
    }
    footer("↑↓ 选择  Left 返回",active?"播放中":"OK 播放");return;
  }
  if(view_==View::ImuTest) {
    snprintf(linebuf,sizeof(linebuf),"%u / 3",imuPageIndex_+1);right(linebuf,24);line(16,34,352);
    if(imuPageIndex_==0){pill(imu.state()==ImuTestService::State::Failed?"测试失败，请按 OK 重试":imu.state()==ImuTestService::State::Powering?"正在开启传感器":imu.hasPassed()?"测试通过，正在持续检测":"持续检测中，请轻晃设备");snprintf(linebuf,sizeof(linebuf),"0x%02X",imu.address());pair("设备地址",linebuf,94);snprintf(linebuf,sizeof(linebuf),"0x%02X",imu.whoAmI());pair("芯片标识",linebuf,128);}
    else if(imuPageIndex_==1){auto& v=imu.latest();small("加速度",24,62);snprintf(linebuf,sizeof(linebuf),"X %d   Y %d   Z %d",v.ax,v.ay,v.az);small(linebuf,24,87);line(24,96,328);small("陀螺仪",24,116);snprintf(linebuf,sizeof(linebuf),"X %d   Y %d   Z %d",v.gx,v.gy,v.gz);small(linebuf,24,138);}
    else {const auto& v=imu.latest();int dx=(static_cast<int>(v.ay)-imu.calAy())*96/4096,dy=-(static_cast<int>(v.ax)-imu.calAx())*36/4096; if(dx>96)dx=96;if(dx< -96)dx=-96;if(dy>36)dy=36;if(dy< -36)dy=-36;DrawUtils::drawClippedRect(c,170,y+56,213,y+99,black);DrawUtils::drawClippedFilledRect(c,176+dx,y+61+dy,207+dx,y+92+dy,black);const char* info=imu.calibrated()?"已校准，倾转设备查看方框移动":"放正设备后按 OK 校准";small(info,(384-t.getUTF8Width(info))/2,128);}
    footer("↑↓ 翻页  Left 返回",imuPageIndex_==2?"OK 校准":"OK 重测");return;
  }
  if(view_==View::PowerTest) {
    snprintf(linebuf,sizeof(linebuf),"%u / 2",powerPageIndex_+1);right(linebuf,24);line(16,34,352);auto& v=power.reading();
    if(!powerPageIndex_){pill(power.state()==PowerDiagnosticService::State::Failed?"电量计通信失败":power.state()==PowerDiagnosticService::State::Warning?"电池数据异常":"电池状态正常");snprintf(linebuf,sizeof(linebuf),"%u.%02u%% 约%u mAh",v.stateOfChargeX100/100,v.stateOfChargeX100%100,power.estimatedRemainingMah());pair("剩余电量",linebuf,94);snprintf(linebuf,sizeof(linebuf),"%u mV",v.voltageMv);pair("电池电压",linebuf,128);}
    else {const auto rate=v.chargeRateX100;snprintf(linebuf,sizeof(linebuf),"%s%lu.%02lu%% / h",rate<0?"-":"+",static_cast<unsigned long>((rate<0?-rate:rate)/100),static_cast<unsigned long>((rate<0?-rate:rate)%100));pair("变化率",linebuf,61);int32_t current=power.estimatedCurrentMaX10();snprintf(linebuf,sizeof(linebuf),"%s%lu.%01lu mA",current<0?"-":"+",static_cast<unsigned long>((current<0?-current:current)/10),static_cast<unsigned long>((current<0?-current:current)%10));pair("估算电流",linebuf,88);snprintf(linebuf,sizeof(linebuf),"%s / %s",power.audioLdoEnabled()?"开启":"关闭",power.sensorLdoEnabled()?"开启":"关闭");pair("音频 / 传感器 LDO",linebuf,115);snprintf(linebuf,sizeof(linebuf),"版本 0x%04X  状态 0x%04X",v.version,v.status);small(linebuf,24,140);}
    footer("↑↓ 翻页  Left 返回","OK 重测");
  }
}

void DeviceSelfTestPage::renderScreenIntro(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + height - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "屏幕测试", 0, width),
                static_cast<int16_t>(yOffset + 22), "屏幕测试");

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(38, static_cast<int16_t>(yOffset + 62), "将依次显示八种全屏图案");
  text.drawUTF8(38, static_cast<int16_t>(yOffset + 91), "Up/Down：切换测试图案");
  text.drawUTF8(38, static_cast<int16_t>(yOffset + 120), "全部查看后按OK确认结果");
  text.drawUTF8(38, static_cast<int16_t>(yOffset + 149), "Left：取消测试");
}

void DeviceSelfTestPage::renderScreenPattern(DisplayMonoTft& display, int16_t yOffset,
                                             uint32_t nowMs) {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const int16_t bottom = static_cast<int16_t>(yOffset + height - 1);

  canvas.drawFilledRectangle(0, yOffset, width - 1, bottom, ST7305_COLOR_WHITE);

  switch (screenPatternIndex_) {
    case 0:
      break;
    case 1:
      canvas.drawFilledRectangle(0, yOffset, width - 1, bottom, ST7305_COLOR_BLACK);
      break;
    case 2:
      canvas.drawRectangle(0, yOffset, width - 1, bottom, ST7305_COLOR_BLACK);
      canvas.drawLine(0, static_cast<int16_t>(yOffset + height / 2), width - 1,
                      static_cast<int16_t>(yOffset + height / 2), ST7305_COLOR_BLACK);
      canvas.drawLine(static_cast<int16_t>(width / 2), yOffset,
                      static_cast<int16_t>(width / 2), bottom, ST7305_COLOR_BLACK);
      break;
    case 3:
      for (int16_t y = yOffset; y <= bottom; y = static_cast<int16_t>(y + 8)) {
        const int16_t stripeBottom = static_cast<int16_t>((y + 3) < bottom ? y + 3 : bottom);
        canvas.drawFilledRectangle(0, y, width - 1, stripeBottom, ST7305_COLOR_BLACK);
      }
      break;
    case 4:
      for (int16_t x = 0; x < width; x = static_cast<int16_t>(x + 8)) {
        const int16_t stripeRight = static_cast<int16_t>((x + 3) < width ? x + 3 : width - 1);
        canvas.drawFilledRectangle(x, yOffset, stripeRight, bottom, ST7305_COLOR_BLACK);
      }
      break;
    case 5: {
      constexpr int16_t kCellSize = 16;
      for (int16_t y = 0; y < height; y = static_cast<int16_t>(y + kCellSize)) {
        for (int16_t x = 0; x < width; x = static_cast<int16_t>(x + kCellSize)) {
          if (((x / kCellSize) + (y / kCellSize)) % 2 == 0) {
            const int16_t right = static_cast<int16_t>(
                (x + kCellSize - 1) < width ? x + kCellSize - 1 : width - 1);
            const int16_t cellBottom = static_cast<int16_t>(
                (y + kCellSize - 1) < height ? yOffset + y + kCellSize - 1 : bottom);
            canvas.drawFilledRectangle(x, static_cast<int16_t>(yOffset + y), right, cellBottom,
                                       ST7305_COLOR_BLACK);
          }
        }
      }
      break;
    }
    case 6:
      canvas.drawRectangle(0, yOffset, width - 1, bottom, ST7305_COLOR_BLACK);
      text.setFont(u8g2_font_7x14_tf);
      text.setFontMode(1);
      text.setBackgroundColor(ST7305_COLOR_WHITE);
      text.setForegroundColor(ST7305_COLOR_BLACK);
      text.drawUTF8(24, static_cast<int16_t>(yOffset + 38), "中文显示：设备屏幕测试");
      text.drawUTF8(24, static_cast<int16_t>(yOffset + 70), "数字：0123456789");
      text.drawUTF8(24, static_cast<int16_t>(yOffset + 102), "符号：+-*/!?  @#");
      text.drawUTF8(24, static_cast<int16_t>(yOffset + 134), "检查文字、边界与方向");
      break;
    default: {
      constexpr int16_t kSquareSize = 42;
      constexpr uint32_t kMoveStepMs = 8;
      const int16_t moveWidth = static_cast<int16_t>(width - kSquareSize);
      const int16_t moveHeight = static_cast<int16_t>(height - kPatternFooterHeight - kSquareSize);
      const uint32_t xPeriod = static_cast<uint32_t>(moveWidth * 2);
      const uint32_t yPeriod = static_cast<uint32_t>(moveHeight * 2);
      uint32_t xPhase = (nowMs / kMoveStepMs) % xPeriod;
      uint32_t yPhase = (nowMs / (kMoveStepMs + 3U)) % yPeriod;
      if (xPhase > static_cast<uint32_t>(moveWidth)) {
        xPhase = xPeriod - xPhase;
      }
      if (yPhase > static_cast<uint32_t>(moveHeight)) {
        yPhase = yPeriod - yPhase;
      }
      const int16_t squareX = static_cast<int16_t>(xPhase);
      const int16_t squareY = static_cast<int16_t>(yOffset + yPhase);
      canvas.drawFilledRectangle(squareX, squareY,
                                 static_cast<int16_t>(squareX + kSquareSize - 1),
                                 static_cast<int16_t>(squareY + kSquareSize - 1),
                                 ST7305_COLOR_BLACK);
      canvas.drawFilledRectangle(static_cast<int16_t>(squareX + 8),
                                 static_cast<int16_t>(squareY + 8),
                                 static_cast<int16_t>(squareX + kSquareSize - 9),
                                 static_cast<int16_t>(squareY + kSquareSize - 9),
                                 ST7305_COLOR_WHITE);

      if (fpsWindowStartMs_ == 0U) {
        fpsWindowStartMs_ = nowMs;
        fpsFrameCount_ = 0;
      }
      ++fpsFrameCount_;
      const uint32_t elapsedMs = nowMs - fpsWindowStartMs_;
      if (elapsedMs >= 1000U) {
        displayedFps_ = static_cast<uint16_t>(
            (static_cast<uint32_t>(fpsFrameCount_) * 1000U) / elapsedMs);
        fpsWindowStartMs_ = nowMs;
        fpsFrameCount_ = 0;
      }
      break;
    }
  }

  const bool blackBackground = screenPatternIndex_ == 1U;
  const uint16_t footerBackground = blackBackground ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  const uint16_t footerForeground = blackBackground ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  const int16_t footerTop = static_cast<int16_t>(yOffset + height - kPatternFooterHeight);
  canvas.drawFilledRectangle(0, footerTop, width - 1, bottom, footerBackground);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(blackBackground ? 0 : 1);
  text.setBackgroundColor(footerBackground);
  text.setForegroundColor(footerForeground);

  char itemText[64];
  snprintf(itemText, sizeof(itemText), "%u/8 %s  Up/Down切换",
           static_cast<unsigned>(screenPatternIndex_ + 1U),
           screenPatternIndex_ == 7U ? "动态" : kScreenPatternNames[screenPatternIndex_]);
  text.drawUTF8(8, static_cast<int16_t>(yOffset + height - 5), itemText);

  if (screenPatternIndex_ == 7U) {
    char fpsText[20];
    snprintf(fpsText, sizeof(fpsText), "FPS:%u", static_cast<unsigned>(displayedFps_));
    const int16_t fpsWidth = text.getUTF8Width(fpsText);
    text.drawUTF8(static_cast<int16_t>(width - fpsWidth - 8),
                  static_cast<int16_t>(yOffset + height - 5), fpsText);
  }
}

void DeviceSelfTestPage::renderScreenConfirm(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + height - 1),
                             ST7305_COLOR_WHITE);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "屏幕显示是否正常？", 0, width),
                static_cast<int16_t>(yOffset + 52), "屏幕显示是否正常？");

  constexpr int16_t kOptionTop = 79;
  constexpr int16_t kOptionBottom = 119;
  constexpr int16_t kOptionWidth = 96;
  constexpr int16_t kOptionGap = 32;
  const int16_t leftX = static_cast<int16_t>((width - kOptionWidth * 2 - kOptionGap) / 2);
  const int16_t rightX = static_cast<int16_t>(leftX + kOptionWidth + kOptionGap);
  const int16_t selectedX = confirmScreenNormal_ ? leftX : rightX;
  canvas.drawFilledRectangle(selectedX, static_cast<int16_t>(yOffset + kOptionTop),
                             static_cast<int16_t>(selectedX + kOptionWidth - 1),
                             static_cast<int16_t>(yOffset + kOptionBottom), ST7305_COLOR_BLACK);

  const char* labels[2] = {"正常", "异常"};
  const int16_t positions[2] = {leftX, rightX};
  for (uint8_t i = 0; i < 2; ++i) {
    const bool selected = (i == 0) == confirmScreenNormal_;
    text.setFontMode(selected ? 0 : 1);
    text.setBackgroundColor(selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
    text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
    text.drawUTF8(TextUtils::centeredTextXInBox(text, labels[i], positions[i], kOptionWidth),
                  static_cast<int16_t>(yOffset + 105), labels[i]);
  }

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  text.drawUTF8(93, static_cast<int16_t>(yOffset + 151), "Up/Down选择  OK确认");
}

void DeviceSelfTestPage::selectScreenPattern(uint8_t patternIndex) {
  screenPatternIndex_ = static_cast<uint8_t>(patternIndex % kScreenPatternCount);
  viewedScreenPatterns_ = static_cast<uint8_t>(
      viewedScreenPatterns_ | static_cast<uint8_t>(1U << screenPatternIndex_));
  LogI("SELFTEST/DISPLAY", "pattern=%u viewed=0x%02X", static_cast<unsigned>(screenPatternIndex_ + 1U),
       static_cast<unsigned>(viewedScreenPatterns_));
}

uint8_t DeviceSelfTestPage::buttonMask(const ButtonState& buttons) const {
  uint8_t mask = 0;
  mask |= buttons.left ? 0x01U : 0U;
  mask |= buttons.right ? 0x02U : 0U;
  mask |= buttons.up ? 0x04U : 0U;
  mask |= buttons.down ? 0x08U : 0U;
  mask |= buttons.ok ? 0x10U : 0U;
  return mask;
}

bool DeviceSelfTestPage::updateButtonTest(const ButtonState& buttons) {
  const uint8_t currentMask = buttonMask(buttons);
  if (currentMask == lastButtonMask_) {
    return false;
  }

  const uint8_t newPresses = static_cast<uint8_t>(currentMask & ~lastButtonMask_);
  const uint8_t releases = static_cast<uint8_t>(lastButtonMask_ & ~currentMask);
  pressedButtonMask_ = static_cast<uint8_t>(pressedButtonMask_ | newPresses);
  const uint8_t newlyCompleted = static_cast<uint8_t>(
      releases & pressedButtonMask_ & ~completedButtonMask_);
  completedButtonMask_ = static_cast<uint8_t>(completedButtonMask_ | newlyCompleted);
  lastButtonMask_ = currentMask;

  for (uint8_t index = 0; index < 5U; ++index) {
    const uint8_t bit = static_cast<uint8_t>(1U << index);
    if ((newlyCompleted & bit) != 0U) {
      LogI("SELFTEST/BUTTON", "key=%s completed mask=0x%02X", kButtonNames[index],
           static_cast<unsigned>(completedButtonMask_));
    }
  }

  if (!buttonTestPassed_ && completedButtonMask_ == kAllButtonsCompleted) {
    buttonTestPassed_ = true;
    LogI("SELFTEST/BUTTON", "finish result=passed");
  }
  return true;
}

void DeviceSelfTestPage::renderButtonTest(DisplayMonoTft& display, int16_t yOffset) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + height - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "按键测试", 0, width),
                static_cast<int16_t>(yOffset + 22), "按键测试");

  constexpr int16_t kBoxWidth = 62;
  constexpr int16_t kBoxGap = 10;
  constexpr int16_t kBoxesWidth = kBoxWidth * 5 + kBoxGap * 4;
  constexpr int16_t kBoxTop = 47;
  constexpr int16_t kBoxBottom = 99;
  const int16_t firstX = static_cast<int16_t>((width - kBoxesWidth) / 2);

  for (uint8_t index = 0; index < 5U; ++index) {
    const uint8_t bit = static_cast<uint8_t>(1U << index);
    const bool pressed = (lastButtonMask_ & bit) != 0U;
    const bool completed = (completedButtonMask_ & bit) != 0U;
    const int16_t boxX = static_cast<int16_t>(firstX + index * (kBoxWidth + kBoxGap));
    const int16_t boxRight = static_cast<int16_t>(boxX + kBoxWidth - 1);

    if (pressed) {
      canvas.drawFilledRectangle(boxX, static_cast<int16_t>(yOffset + kBoxTop), boxRight,
                                 static_cast<int16_t>(yOffset + kBoxBottom),
                                 ST7305_COLOR_BLACK);
    } else {
      canvas.drawRectangle(boxX, static_cast<int16_t>(yOffset + kBoxTop), boxRight,
                           static_cast<int16_t>(yOffset + kBoxBottom), ST7305_COLOR_BLACK);
    }

    text.setFontMode(pressed ? 0 : 1);
    text.setBackgroundColor(pressed ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
    text.setForegroundColor(pressed ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
    text.drawUTF8(TextUtils::centeredTextXInBox(text, kButtonNames[index], boxX, kBoxWidth),
                  static_cast<int16_t>(yOffset + 69), kButtonNames[index]);

    const char* status = completed ? "已通过" : "待测试";
    text.drawUTF8(TextUtils::centeredTextXInBox(text, status, boxX, kBoxWidth),
                  static_cast<int16_t>(yOffset + 91), status);
  }

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  const char* instruction = buttonTestPassed_ ? "全部按键通过  按OK返回"
                                              : "依次按下并释放全部按键";
  text.drawUTF8(TextUtils::centeredTextXInBox(text, instruction, 0, width),
                static_cast<int16_t>(yOffset + 128), instruction);
  text.drawUTF8(12, static_cast<int16_t>(yOffset + height - 10), "长按Left取消");
}

void DeviceSelfTestPage::renderIicTest(DisplayMonoTft& display, int16_t yOffset,
                                       const IicScanService& iicScanService) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const IicScanService::State state = iicScanService.state();
  const IicBusScanner::Result& result = iicScanService.result();

  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + height - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "IIC设备测试", 0, width),
                static_cast<int16_t>(yOffset + 22), "IIC设备测试");

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  char statusText[64];
  if (state == IicScanService::State::Powering) {
    snprintf(statusText, sizeof(statusText), "正在开启传感器电源...");
  } else if (state == IicScanService::State::Settling) {
    snprintf(statusText, sizeof(statusText), "正在初始化总线...");
  } else if (state == IicScanService::State::Scanning) {
    snprintf(statusText, sizeof(statusText), "正在扫描 0x%02X / 0x77",
             iicScanService.currentAddress());
  } else if (state == IicScanService::State::Scanned) {
    snprintf(statusText, sizeof(statusText), "扫描完成：发现 %u 个设备", result.deviceCount);
  } else if (state == IicScanService::State::Error) {
    snprintf(statusText, sizeof(statusText), "总线初始化失败");
  } else {
    snprintf(statusText, sizeof(statusText), "按OK重新扫描");
  }
  text.drawUTF8(18, static_cast<int16_t>(yOffset + 55), statusText);

  text.drawUTF8(18, static_cast<int16_t>(yOffset + 82), "发现地址：");
  constexpr uint8_t kVisibleAddressCount = 10;
  const uint8_t visibleCount = result.deviceCount < kVisibleAddressCount
                                   ? result.deviceCount
                                   : kVisibleAddressCount;
  for (uint8_t index = 0; index < visibleCount; ++index) {
    char addressText[8];
    snprintf(addressText, sizeof(addressText), "0x%02X", result.addresses[index]);
    const uint8_t column = static_cast<uint8_t>(index % 5U);
    const uint8_t row = static_cast<uint8_t>(index / 5U);
    text.drawUTF8(static_cast<int16_t>(18 + column * 70),
                  static_cast<int16_t>(yOffset + 106 + row * 23), addressText);
  }

  if (result.deviceCount == 0U && state == IicScanService::State::Scanned) {
    text.drawUTF8(18, static_cast<int16_t>(yOffset + 106), "未发现响应设备");
  }

  text.drawUTF8(12, static_cast<int16_t>(yOffset + height - 9), "Left返回  OK重新扫描");
  char errorText[24];
  snprintf(errorText, sizeof(errorText), "错误:%u", result.errorCount);
  const int16_t errorWidth = text.getUTF8Width(errorText);
  text.drawUTF8(static_cast<int16_t>(width - errorWidth - 10),
                static_cast<int16_t>(yOffset + height - 9), errorText);
}

void DeviceSelfTestPage::renderRtcTest(DisplayMonoTft& display, int16_t yOffset,
                                       const RtcTestService& rtcTestService) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  const RtcTestService::State state = rtcTestService.state();
  const RtcDriver::Diagnostics& diag = rtcTestService.diagnostics();
  const RtcDriver::DateTime& time = rtcTestService.latestTime();

  canvas.drawFilledRectangle(0, yOffset, width - 1, static_cast<int16_t>(yOffset + height - 1),
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1,
                             static_cast<int16_t>(yOffset + kHeaderHeight - 1),
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "RTC测试", 0, width),
                static_cast<int16_t>(yOffset + 22), "RTC测试");

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);
  const char* stateText = "按OK重新测试";
  if (state == RtcTestService::State::Waiting) {
    stateText = "正在等待时间递增...";
  } else if (state == RtcTestService::State::Passed) {
    stateText = "测试通过：走时与省电配置正常";
  } else if (state == RtcTestService::State::Warning) {
    stateText = "走时正常：存在省电配置建议";
  } else if (state == RtcTestService::State::Failed) {
    switch (rtcTestService.error()) {
      case RtcTestService::Error::Unavailable:
        stateText = "测试失败：RTC设备不可用";
        break;
      case RtcTestService::Error::RegisterRead:
        stateText = "测试失败：配置寄存器读取失败";
        break;
      case RtcTestService::Error::FirstTimeRead:
      case RtcTestService::Error::SecondTimeRead:
        stateText = "测试失败：时间寄存器读取失败";
        break;
      case RtcTestService::Error::InvalidTime:
        stateText = "测试失败：日期或时间字段无效";
        break;
      case RtcTestService::Error::NotAdvancing:
        stateText = "测试失败：RTC时间未递增";
        break;
      default:
        stateText = "测试失败：未知错误";
        break;
    }
  }
  text.drawUTF8(16, static_cast<int16_t>(yOffset + 51), stateText);

  char line[96];
  snprintf(line, sizeof(line), "时间：%02u:%02u:%02u  日期：20%02u-%02u-%02u",
           time.hour, time.minute, time.second, time.year, time.month, time.day);
  if (rtcPageIndex_ == 0U) {
    text.drawUTF8(16, static_cast<int16_t>(yOffset + 83), line);
    text.drawUTF8(16, static_cast<int16_t>(yOffset + 112), "连续读取两次，间隔约1.1秒");
    text.drawUTF8(16, static_cast<int16_t>(yOffset + 138), "检查日期范围与时间递增");
    text.drawUTF8(16, static_cast<int16_t>(yOffset + 160), "Up/Down翻页  1/2");
    return;
  }

  snprintf(line, sizeof(line), "Control1:0x%02X  Control2:0x%02X  OS:%s",
           diag.control1, diag.control2, diag.oscillatorStopped ? "异常" : "正常");
  text.drawUTF8(16, static_cast<int16_t>(yOffset + 76), line);

  snprintf(line, sizeof(line), "Offset:0x%02X  Timer:0x%02X  建议项:%u",
           diag.offset, diag.timerMode,
           static_cast<unsigned>(__builtin_popcount(rtcTestService.warnings())));
  text.drawUTF8(16, static_cast<int16_t>(yOffset + 105), line);

  const uint16_t warnings = rtcTestService.warnings();
  const char* advice = "配置：CLKOUT关闭 / Timer关闭 / Offset低功耗";
  if ((warnings & RtcTestService::ClockStopped) != 0U) {
    advice = "异常：STOP置位，RTC时钟已停止";
  } else if ((warnings & RtcTestService::OscillatorStopped) != 0U) {
    advice = "异常：OS置位，时间可信度不足";
  } else if ((warnings & RtcTestService::ClockOutputEnabled) != 0U) {
    advice = "建议：关闭CLKOUT以降低功耗";
  } else if ((warnings & RtcTestService::TimerEnabled) != 0U ||
             (warnings & RtcTestService::PeriodicInterruptEnabled) != 0U) {
    advice = "建议：确认定时器和周期中断是否需要";
  } else if ((warnings & RtcTestService::FastOffsetMode) != 0U) {
    advice = "建议：Offset使用Normal低功耗模式";
  } else if ((warnings & RtcTestService::TimerClockNotLowPower) != 0U) {
    advice = "建议：闲置Timer时钟选择1/60Hz";
  } else if ((warnings & RtcTestService::CorrectionInterruptEnabled) != 0U) {
    advice = "建议：不需要校准中断时关闭CIE";
  } else if ((warnings & RtcTestService::ExternalTestEnabled) != 0U) {
    advice = "异常：EXT_TEST外部测试模式已开启";
  }
  text.drawUTF8(16, static_cast<int16_t>(yOffset + 133), advice);
  text.drawUTF8(16, static_cast<int16_t>(yOffset + 160), "Up/Down翻页  2/2");
}

void DeviceSelfTestPage::renderSdTest(DisplayMonoTft& d,int16_t y,const SdCardService&s)const{
  auto&c=d.canvas();auto&t=d.text();int16_t w=d.width(),h=d.height();c.drawFilledRectangle(0,y,w-1,y+h-1,ST7305_COLOR_WHITE);c.drawFilledRectangle(0,y,w-1,y+27,ST7305_COLOR_BLACK);t.setFont(u8g2_font_7x14_tf);t.setFontMode(0);t.setBackgroundColor(ST7305_COLOR_BLACK);t.setForegroundColor(ST7305_COLOR_WHITE);int16_t tw=t.getUTF8Width("SD卡测试");t.drawUTF8((w-tw)/2,y+22,"SD卡测试");t.setFontMode(1);t.setBackgroundColor(ST7305_COLOR_WHITE);t.setForegroundColor(ST7305_COLOR_BLACK);const char*st=s.status()==SdCardService::Status::Ready?(s.rootReadable()?"只读测试通过":"根目录读取失败"):(s.status()==SdCardService::Status::NotInserted?"未检测到SD卡":"SD卡需要重新插入");t.drawUTF8(24,y+57,st);char line[64];snprintf(line,sizeof(line),"总容量：%llu MB",static_cast<unsigned long long>(s.totalBytes()/(1024ULL*1024ULL)));t.drawUTF8(24,y+88,line);snprintf(line,sizeof(line),"可用容量：%llu MB",static_cast<unsigned long long>(s.freeBytes()/(1024ULL*1024ULL)));t.drawUTF8(24,y+116,line);t.drawUTF8(24,y+151,"默认只读  OK重试  Left返回");}
void DeviceSelfTestPage::renderImuTest(DisplayMonoTft& display, int16_t yOffset,
                                       const ImuTestService& service) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + height - 1,
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + 27, ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "IMU测试", 0, width), yOffset + 22, "IMU测试");
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  if (imuPageIndex_ == 0U) {
    const char* status = service.state() == ImuTestService::State::Failed
                             ? "测试失败，请按OK重试"
                         : service.state() == ImuTestService::State::Powering
                             ? "正在开启传感器..."
                         : service.hasPassed() ? "测试通过，正在持续检测"
                                               : "持续检测中，请轻晃设备...";
    text.drawUTF8(18, yOffset + 57, status);
    char line[64];
    snprintf(line, sizeof(line), "地址：0x%02X", service.address());
    text.drawUTF8(18, yOffset + 91, line);
    snprintf(line, sizeof(line), "芯片标识：0x%02X", service.whoAmI());
    text.drawUTF8(18, yOffset + 122, line);
    text.drawUTF8(18, yOffset + 158, "Up/Down翻页  1/3  Left退出");
    return;
  }

  const ImuDriver::Sample& sample = service.latest();
  char line[80];
  if (imuPageIndex_ == 1U) {
    snprintf(line, sizeof(line), "加速度 X:%d", sample.ax);
    text.drawUTF8(18, yOffset + 51, line);
    snprintf(line, sizeof(line), "加速度 Y:%d", sample.ay);
    text.drawUTF8(18, yOffset + 76, line);
    snprintf(line, sizeof(line), "加速度 Z:%d", sample.az);
    text.drawUTF8(18, yOffset + 101, line);
    snprintf(line, sizeof(line), "陀螺仪 X:%d  Y:%d  Z:%d", sample.gx, sample.gy, sample.gz);
    text.drawUTF8(18, yOffset + 129, line);
    text.drawUTF8(18, yOffset + 158, "Up/Down翻页  2/3  Left退出");
    return;
  }

  // 小方块水平仪：用加速度计 ax/ay 做重力投影映射到屏幕偏移（8g 量程 ≈ 4096 LSB/g）。
  // 校准后以校准值为零位，未校准时以 0 为零位。左右倾由 ay 驱动，上下倾由 ax 驱动。
  constexpr int16_t kSquareSize = 32;
  constexpr int16_t kSquareHalf = kSquareSize / 2;
  constexpr int16_t kMaxTravelX = 96;
  constexpr int16_t kMaxTravelY = 36;
  const int16_t arenaLeft = 16;
  const int16_t arenaRight = static_cast<int16_t>(width - 16);
  const int16_t arenaTop = static_cast<int16_t>(yOffset + 40);
  const int16_t arenaBottom = static_cast<int16_t>(yOffset + height - 22);
  const int16_t centerX = static_cast<int16_t>(width / 2);
  const int16_t centerY = static_cast<int16_t>((arenaTop + arenaBottom) / 2);

  const int32_t relAx = static_cast<int32_t>(sample.ax) - static_cast<int32_t>(service.calAx());
  const int32_t relAy = static_cast<int32_t>(sample.ay) - static_cast<int32_t>(service.calAy());
  int32_t dx = relAy * kMaxTravelX / 4096;
  int32_t dy = -(relAx * kMaxTravelY) / 4096;
  if (dx > kMaxTravelX) dx = kMaxTravelX;
  if (dx < -kMaxTravelX) dx = -kMaxTravelX;
  if (dy > kMaxTravelY) dy = kMaxTravelY;
  if (dy < -kMaxTravelY) dy = -kMaxTravelY;

  int16_t squareX = static_cast<int16_t>(centerX + dx - kSquareHalf);
  int16_t squareY = static_cast<int16_t>(centerY + dy - kSquareHalf);
  if (squareX < arenaLeft) squareX = arenaLeft;
  if (squareX > arenaRight - kSquareSize) squareX = arenaRight - kSquareSize;
  if (squareY < arenaTop) squareY = arenaTop;
  if (squareY > arenaBottom - kSquareSize) squareY = arenaBottom - kSquareSize;

  canvas.drawRectangle(static_cast<int16_t>(centerX - kSquareHalf - 6),
                       static_cast<int16_t>(centerY - kSquareHalf - 6),
                       static_cast<int16_t>(centerX + kSquareHalf + 6),
                       static_cast<int16_t>(centerY + kSquareHalf + 6), ST7305_COLOR_BLACK);
  canvas.drawFilledRectangle(squareX, squareY, static_cast<int16_t>(squareX + kSquareSize - 1),
                             static_cast<int16_t>(squareY + kSquareSize - 1), ST7305_COLOR_BLACK);

  text.drawUTF8(18, yOffset + 57, service.calibrated() ? "已校准  倾转设备查看方框移动"
                                                       : "未校准  放正设备后按OK校准");
  text.drawUTF8(18, yOffset + 158, "Up/Down翻页  3/3  Left退出  OK校准");
}

void DeviceSelfTestPage::renderPowerTest(DisplayMonoTft& display, int16_t yOffset,
                                         const PowerDiagnosticService& service) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());
  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + height - 1,
                             ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + 27, ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "电源测试", 0, width), yOffset + 22, "电源测试");
  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  const Max17048Driver::Reading& reading = service.reading();
  const char* result = service.state() == PowerDiagnosticService::State::Failed
                           ? "MAX17048通信失败"
                       : service.state() == PowerDiagnosticService::State::Warning
                           ? "数据已读取，部分数值异常"
                           : "MAX17048检测通过";
  char line[80];
  if (powerPageIndex_ == 0U) {
    text.drawUTF8(16, yOffset + 51, result);
    text.drawUTF8(16, yOffset + 78, "规格：3.8V  800mAh  满充4.35V");
    snprintf(line, sizeof(line), "电量：%u.%02u%%  约%u mAh",
             reading.stateOfChargeX100 / 100U, reading.stateOfChargeX100 % 100U,
             service.estimatedRemainingMah());
    text.drawUTF8(16, yOffset + 105, line);
    const char* voltageText = service.voltageState() == PowerDiagnosticService::VoltageState::Low
                                  ? "低压"
                              : service.voltageState() == PowerDiagnosticService::VoltageState::High
                                  ? "过压"
                                  : "正常";
    snprintf(line, sizeof(line), "电压：%u mV  %s", reading.voltageMv, voltageText);
    text.drawUTF8(16, yOffset + 132, line);
    text.drawUTF8(16, yOffset + 159, "Up/Down翻页  1/2  Left返回");
    return;
  }

  const int32_t rate = reading.chargeRateX100;
  const uint32_t rateMagnitude = static_cast<uint32_t>(rate < 0 ? -rate : rate);
  snprintf(line, sizeof(line), "变化率：%s%lu.%02lu%%/h", rate >= 0 ? "+" : "-",
           static_cast<unsigned long>(rateMagnitude / 100U),
           static_cast<unsigned long>(rateMagnitude % 100U));
  text.drawUTF8(16, yOffset + 51, line);
  const int32_t currentX10 = service.estimatedCurrentMaX10();
  const uint32_t currentMagnitude =
      static_cast<uint32_t>(currentX10 < 0 ? -currentX10 : currentX10);
  snprintf(line, sizeof(line), "估算电流：%s%lu.%01lu mA", currentX10 >= 0 ? "+" : "-",
           static_cast<unsigned long>(currentMagnitude / 10U),
           static_cast<unsigned long>(currentMagnitude % 10U));
  text.drawUTF8(16, yOffset + 78, line);
  snprintf(line, sizeof(line), "版本：0x%04X  状态：0x%04X", reading.version,
           reading.status);
  text.drawUTF8(16, yOffset + 105, line);
  snprintf(line, sizeof(line), "音频LDO：%s  传感器LDO：%s",
           service.audioLdoEnabled() ? "开启" : "关闭",
           service.sensorLdoEnabled() ? "开启" : "关闭");
  text.drawUTF8(16, yOffset + 132, line);
  text.drawUTF8(16, yOffset + 159, "Up/Down翻页  2/2  OK重测  Left返回");
}

void DeviceSelfTestPage::renderAudioTest(DisplayMonoTft& display, int16_t yOffset,
                                         const AudioTestService& service) const {
  auto& canvas = display.canvas();
  auto& text = display.text();
  const int16_t width = static_cast<int16_t>(display.width());
  const int16_t height = static_cast<int16_t>(display.height());

  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + height - 1, ST7305_COLOR_WHITE);
  canvas.drawFilledRectangle(0, yOffset, width - 1, yOffset + kHeaderHeight - 1,
                             ST7305_COLOR_BLACK);
  text.setFont(u8g2_font_7x14_tf);
  text.setFontMode(0);
  text.setBackgroundColor(ST7305_COLOR_BLACK);
  text.setForegroundColor(ST7305_COLOR_WHITE);
  text.drawUTF8(TextUtils::centeredTextXInBox(text, "音频测试", 0, width), yOffset + 22,
                "音频测试");

  text.setFontMode(1);
  text.setBackgroundColor(ST7305_COLOR_WHITE);
  text.setForegroundColor(ST7305_COLOR_BLACK);

  const char* status = "按OK播放1kHz提示音";
  switch (service.state()) {
    case AudioTestService::State::Powering:
      status = "正在开启音频...";
      break;
    case AudioTestService::State::Playing:
      status = "正在播放提示音...";
      break;
    case AudioTestService::State::WaitingConfirm:
      status = "提示音已播放";
      break;
    case AudioTestService::State::Passed:
      status = "测试通过：已确认听到";
      break;
    case AudioTestService::State::Failed:
      switch (service.error()) {
        case AudioTestService::Error::MusicBusy:
          status = "音乐播放中，请先退出";
          break;
        case AudioTestService::Error::DacPower:
          status = "音频供电或静音控制失败";
          break;
        case AudioTestService::Error::ToneInit:
          status = "音频硬件初始化失败";
          break;
        case AudioTestService::Error::ToneWrite:
          status = "音频数据写入失败";
          break;
        case AudioTestService::Error::NotHeard:
          status = "未确认听到提示音";
          break;
        case AudioTestService::Error::Timeout:
          status = "测试超时";
          break;
        default:
          status = "测试失败";
          break;
      }
      break;
    default:
      break;
  }
  text.drawUTF8(18, yOffset + 55, status);

  text.drawUTF8(18, yOffset + 82,
                service.headphonesInserted() ? "输出：耳机已插入" : "输出：扬声器");

  const bool waiting = service.state() == AudioTestService::State::WaitingConfirm;
  if (!waiting) {
    text.drawUTF8(18, yOffset + 109, "提示音：1kHz  500ms  固定低音量");
    if (service.state() == AudioTestService::State::Passed ||
        service.state() == AudioTestService::State::Failed) {
      char durationText[32];
      const uint32_t durationMs = service.lastDurationMs();
      snprintf(durationText, sizeof(durationText), "用时：%lu.%01lu 秒",
               static_cast<unsigned long>(durationMs / 1000U),
               static_cast<unsigned long>((durationMs % 1000U) / 100U));
      text.drawUTF8(18, yOffset + 136, durationText);
    }
  } else {
    text.drawUTF8(18, yOffset + 109, "听到提示音了吗？");

    constexpr int16_t kOptionWidth = 96;
    constexpr int16_t kOptionGap = 32;
    constexpr int16_t kOptionTop = 120;
    constexpr int16_t kOptionBottom = 144;
    const int16_t leftX = static_cast<int16_t>((width - kOptionWidth * 2 - kOptionGap) / 2);
    const int16_t rightX = static_cast<int16_t>(leftX + kOptionWidth + kOptionGap);
    const int16_t selectedX = service.heardSelected() ? leftX : rightX;
    canvas.drawFilledRectangle(selectedX, yOffset + kOptionTop,
                               static_cast<int16_t>(selectedX + kOptionWidth - 1),
                               yOffset + kOptionBottom, ST7305_COLOR_BLACK);
    const char* labels[2] = {"是", "否"};
    const int16_t positions[2] = {leftX, rightX};
    for (uint8_t index = 0; index < 2; ++index) {
      const bool selected = (index == 0) == service.heardSelected();
      text.setFontMode(selected ? 0 : 1);
      text.setBackgroundColor(selected ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
      text.setForegroundColor(selected ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK);
      text.drawUTF8(TextUtils::centeredTextXInBox(text, labels[index], positions[index],
                                                  kOptionWidth),
                    yOffset + 138, labels[index]);
    }
    text.setFontMode(1);
    text.setBackgroundColor(ST7305_COLOR_WHITE);
    text.setForegroundColor(ST7305_COLOR_BLACK);
  }

  const char* footer = "Left返回  OK播放";
  if (service.state() == AudioTestService::State::Powering ||
      service.state() == AudioTestService::State::Playing) {
    footer = "Left停止并返回";
  } else if (waiting) {
    footer = "Up/Down选择  OK确认";
  } else if (service.state() == AudioTestService::State::Passed ||
             service.state() == AudioTestService::State::Failed) {
    footer = "Left返回  OK重测";
  }
  text.drawUTF8(18, yOffset + height - 9, footer);
}
