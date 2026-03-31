#pragma once

#include <stdint.h>

#include "../IconBitmap.h"
#include "HomePage.h"

class DisplayMonoTft;
class WifiProvisionService;

class SettingsPage {
public:
  struct MenuItem {
    const char* labelZh;
    const char* labelEn;
    IconBitmap::Anim icon;
  };

  enum class PopupKind : uint8_t {
    None,
    RestartConfirm,
    LanguageSelect
  };

  static constexpr uint8_t kHomeIndex = 0;
  static constexpr uint8_t kRestartItemIndex = 0;
  static constexpr uint8_t kLanguageItemIndex = 1;
  static constexpr uint8_t kWifiProvisionItemIndex = 3;
  static constexpr uint8_t kAboutDeviceItemIndex = 5;

  PopupKind popupForSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isAboutDeviceSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isWifiProvisionSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  uint8_t detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                         uint32_t nowMs, WifiProvisionService& wifi) const;
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                        WifiProvisionService& wifi) const;
  const MenuItem* menuItems() const;
  uint8_t menuItemCount() const;
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                    int16_t yOffset, DisplayMonoTft& display, HomePage::Language language,
                    const char* deviceIdText, const char* flashTotalText,
                    const char* sdStatusText,
                    const WifiProvisionService& wifi) const;

  const char* popupTitle(PopupKind kind, HomePage::Language language) const;
  const char* popupPrimaryLabel(PopupKind kind, HomePage::Language language) const;
  const char* popupSecondaryLabel(PopupKind kind, HomePage::Language language) const;
};
