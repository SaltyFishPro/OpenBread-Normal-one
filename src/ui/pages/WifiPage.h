#pragma once

#include "../../services/SdBitmapFont.h"
#include "../../services/WifiProvisionService.h"
#include "../SdBitmapText.h"

class DisplayMonoTft;
class TimeService;

// Owns only page focus, confirmation and typography; service owns networking.
class WifiPage {
public:
  void prepareEnter(const WifiProvisionService& wifi);
  void beginFonts(SdBitmapFont& ui20, SdBitmapFont& ui24);
  bool needsFullFont14(const WifiProvisionService& wifi) const;
  bool needsFullFont20(const WifiProvisionService& wifi) const;
  void endFonts();
  bool fontsLoading() const;
  bool update(const WifiProvisionService& wifi);
  // Called only after UiManager has presented the busy frame.
  bool afterPresent(uint32_t nowMs, WifiProvisionService& wifi);
  bool renderConfirmationFocusOnly(DisplayMonoTft& display);
  // False only when Left requests leaving the page for Settings.
  bool handleInput(bool left, bool up, bool down, bool ok, uint32_t nowMs,
                   WifiProvisionService& wifi, TimeService& time);
  void render(DisplayMonoTft& display, int16_t yOffset, const WifiProvisionService& wifi,
              const TimeService& time, SdBitmapFont* fullFont, SdBitmapFont* networkFont);

private:
  enum class View : uint8_t { List, Detail, Replace, Starting, Session };
  enum class Confirm : uint8_t { None, Start, Replace, Reconfigure, Forget };
  void showList(const WifiProvisionService& wifi);
  void addNetwork(const WifiProvisionService& wifi);
  void activateRecord(const WifiProvisionService& wifi);
  void executeConfirmation(WifiProvisionService& wifi);
  void renderConfirmationButtons(DisplayMonoTft& display, int16_t yOffset);
  int16_t textWidth(DisplayMonoTft& display, SdBitmapFont* font, const char* value);
  void text(DisplayMonoTft& display, SdBitmapFont* font, int16_t x, int16_t baseline,
            const char* value, int16_t width, uint16_t color);

  View view_ = View::List;
  Confirm confirm_ = Confirm::None;
  Confirm pendingPortal_ = Confirm::None;
  Confirm renderedConfirm_ = Confirm::None;
  uint8_t focus_ = 0, record_ = 0, action_ = 0, choice_ = 0;
  WifiProvisionService::State previousState_ = WifiProvisionService::State::Idle;
  SdBitmapFont* ui20_ = nullptr;
  SdBitmapFont* ui24_ = nullptr;
  bool focusOnly_ = false;
  bool startingPresented_ = false;
  bool fontsStarted_ = false;
  bool storageError_ = false;
};
