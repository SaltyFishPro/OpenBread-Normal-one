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

  // 需要二次确认的操作：设置列表项与详情页动作共用同一套文案与弹窗。
  enum class ConfirmKind : uint8_t {
    None,
    Restart,
    FactoryReset,
    OtaApply,
    WifiStart,
  };

  static constexpr uint8_t kHomeIndex = 0;
  static constexpr uint8_t kRestartItemIndex = 0;
  static constexpr uint8_t kOtaItemIndex = 1;
  static constexpr uint8_t kWifiProvisionItemIndex = 2;
  static constexpr uint8_t kTimeCalibrationItemIndex = 3;
  static constexpr uint8_t kDeviceSelfTestItemIndex = 4;
  static constexpr uint8_t kUsbSerialItemIndex = 5;
  static constexpr uint8_t kFactoryResetItemIndex = 6;
  static constexpr uint8_t kAboutDeviceItemIndex = 7;

  ConfirmKind popupForSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  // 详情页按 OK 前是否需要确认（目前只有"应用固件"和"开始配网"）。
  ConfirmKind detailConfirmFor(uint8_t homeFocus, uint8_t sectionFocus, const OtaService& ota,
                               const WifiProvisionService& wifi) const;
  bool isAboutDeviceSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isOtaSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isWifiProvisionSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isDeviceSelfTestSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isUsbSerialSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
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

  const char* confirmTitle(ConfirmKind kind) const;
  // 左侧安全项（默认选中）。
  const char* confirmPrimaryLabel(ConfirmKind kind) const;
  // 右侧执行项。
  const char* confirmDangerLabel(ConfirmKind kind) const;
};
