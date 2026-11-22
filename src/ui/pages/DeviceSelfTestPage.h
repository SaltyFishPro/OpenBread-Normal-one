#pragma once

#include <stdint.h>
#include "SdFontTestPage.h"

class DisplayMonoTft;
class IicScanService;
class RtcTestService;
class SdCardService;
class ImuTestService;
class PowerDiagnosticService;
class AudioTestService;
class MotorTestService;
class PeripheralPower;
class SdBitmapFont;

class DeviceSelfTestPage {
public:
  static constexpr uint8_t kItemCount = 12;

  struct ButtonState {
    bool left;
    bool right;
    bool up;
    bool down;
    bool ok;
  };

  void reset();
  bool handleInput(bool leftEdge, bool leftLongEdge, bool upEdge, bool downEdge, bool okEdge,
                   const ButtonState& buttons, uint32_t nowMs, IicScanService& iicScanService,
                   RtcTestService& rtcTestService, SdCardService& sdCardService,
                   ImuTestService& imuTestService, PowerDiagnosticService& powerDiagnosticService,
                   AudioTestService& audioTestService, MotorTestService& motorTestService,
                   PeripheralPower& peripheralPower, SdBitmapFont& font, bool musicBusy);
  void render(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs,
              const IicScanService& iicScanService, const RtcTestService& rtcTestService,
              const SdCardService& sdCardService, const ImuTestService& imuTestService,
              const PowerDiagnosticService& powerDiagnosticService,
              const AudioTestService& audioTestService, const MotorTestService& motorTestService,
              SdBitmapFont& font, SdBitmapFont* ui20 = nullptr);
  bool needsAnimationFrame() const;

private:
  enum class View : uint8_t {
    List,
    ScreenIntro,
    ScreenPattern,
    ScreenConfirm,
    ButtonTest,
    IicTest,
    RtcTest, SdTest, ImuTest, PowerTest, AudioTest, MotorTest, FontTest
  };

  enum class ScreenResult : uint8_t {
    NotRun,
    Passed,
    Failed
  };

  void renderModern(DisplayMonoTft&, int16_t, SdBitmapFont*, const IicScanService&,
                    const RtcTestService&, const SdCardService&, const ImuTestService&,
                    const PowerDiagnosticService&, const AudioTestService&, const MotorTestService&) const;
  void renderList(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs,
                  SdBitmapFont* ui20);
  void renderScreenIntro(DisplayMonoTft& display, int16_t yOffset) const;
  void renderScreenPattern(DisplayMonoTft& display, int16_t yOffset, uint32_t nowMs);
  void renderScreenConfirm(DisplayMonoTft& display, int16_t yOffset) const;
  void renderButtonTest(DisplayMonoTft& display, int16_t yOffset) const;
  void renderIicTest(DisplayMonoTft& display, int16_t yOffset,
                     const IicScanService& iicScanService) const;
  void renderRtcTest(DisplayMonoTft& display, int16_t yOffset,
                     const RtcTestService& rtcTestService) const;
  void renderSdTest(DisplayMonoTft&,int16_t,const SdCardService&)const;
  void renderImuTest(DisplayMonoTft&,int16_t,const ImuTestService&)const;
  void renderPowerTest(DisplayMonoTft&, int16_t, const PowerDiagnosticService&) const;
  void renderAudioTest(DisplayMonoTft& display, int16_t yOffset,
                       const AudioTestService& audioTestService) const;
  void selectScreenPattern(uint8_t patternIndex);
  uint8_t buttonMask(const ButtonState& buttons) const;
  bool updateButtonTest(const ButtonState& buttons);

  View view_ = View::List;
  uint8_t focusIndex_ = 0;
  int16_t focusFromY_ = 40;
  uint32_t focusStartMs_ = 0;
  bool focusAnimating_ = false;
  uint8_t screenPatternIndex_ = 0;
  uint8_t viewedScreenPatterns_ = 0;
  bool confirmScreenNormal_ = true;
  ScreenResult screenResult_ = ScreenResult::NotRun;
  uint8_t lastButtonMask_ = 0;
  uint8_t pressedButtonMask_ = 0;
  uint8_t completedButtonMask_ = 0;
  bool buttonTestPassed_ = false;
  uint32_t fpsWindowStartMs_ = 0;
  uint16_t fpsFrameCount_ = 0;
  uint16_t displayedFps_ = 0;
  uint8_t rtcPageIndex_ = 0;
  uint8_t imuPageIndex_ = 0;
  uint8_t powerPageIndex_ = 0;
  SdFontTestPage fontTestPage_;
};
