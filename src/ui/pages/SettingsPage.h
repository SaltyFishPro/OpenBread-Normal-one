#pragma once

#include <stdint.h>

#include "../IconBitmap.h"

class DisplayMonoTft;
class WifiProvisionService;
class OtaService;
class TimeService;

class SettingsPage {
public:
  struct MenuItem {
    const char* labelZh;
    IconBitmap::Anim icon;
  };

  enum class PopupKind : uint8_t {
    None,
    RestartConfirm
  };

  static constexpr uint8_t kHomeIndex = 0;
  static constexpr uint8_t kRestartItemIndex = 0;
  static constexpr uint8_t kOtaItemIndex = 1;
  static constexpr uint8_t kWifiProvisionItemIndex = 2;
  static constexpr uint8_t kTimeCalibrationItemIndex = 3;
  static constexpr uint8_t kDeviceSelfTestItemIndex = 4;
  static constexpr uint8_t kAboutDeviceItemIndex = 6;

  PopupKind popupForSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isAboutDeviceSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isOtaSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isWifiProvisionSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isDeviceSelfTestSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  uint8_t detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                         bool okEdge,
                         uint32_t nowMs, WifiProvisionService& wifi,
                         OtaService& ota) const;
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                        WifiProvisionService& wifi, OtaService& ota, TimeService& time) const;
  const MenuItem* menuItems() const;
  uint8_t menuItemCount() const;
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, uint8_t detailPageIndex,
                    int16_t yOffset, DisplayMonoTft& display, const char* deviceIdText,
                    const char* flashTotalText,
                    const char* sdStatusText,
                    const WifiProvisionService& wifi, const OtaService& ota,
                    const TimeService& time) const;

  const char* popupTitle(PopupKind kind) const;
  const char* popupPrimaryLabel(PopupKind kind) const;
  const char* popupSecondaryLabel(PopupKind kind) const;
};
