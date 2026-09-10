#pragma once

#include <stdint.h>

#include "HomePage.h"

class BluetoothService;
class DisplayMonoTft;
class RemoteService;

class RemotePage {
public:
  static constexpr uint8_t kHomeIndex = 4;
  static constexpr uint8_t kBluetoothConnectItemIndex = 0;
  static constexpr uint8_t kBluetoothRemoteCamItemIndex = 1;

  bool isBluetoothSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isRemoteCamSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  uint8_t detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge, uint32_t nowMs,
                         BluetoothService& bluetooth, RemoteService& remote) const;
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus,
                        BluetoothService& bluetooth) const;
  void handleSectionExit(uint8_t homeFocus, BluetoothService& bluetooth) const;
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, HomePage::Language language,
                    const BluetoothService& bluetooth, const RemoteService& remote) const;
};
