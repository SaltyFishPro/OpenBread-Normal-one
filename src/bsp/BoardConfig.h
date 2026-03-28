#pragma once

namespace BoardConfig {
constexpr uint32_t kUiTickMs = 33;
constexpr uint32_t kInputTickMs = 10;
constexpr uint32_t kImuTickMs = 20;
constexpr uint32_t kAlarmTickMs = 1000;

constexpr int kPinDc = 2;
constexpr int kPinRst = 46;
constexpr int kPinCs = 1;
constexpr int kPinSclk = 3;
constexpr int kPinSdin = 4;

constexpr int kPinSdMiso = 10;
constexpr int kPinSdClk = 11;
constexpr int kPinSdMosi = 12;
constexpr int kPinSdCs = 13;

constexpr int kPinIicSda = 47;
constexpr int kPinIicScl = 48;

constexpr int kPinBtnLeft = 0;
constexpr int kPinBtnRight = 14;
constexpr int kPinBtnUp = 17;
constexpr int kPinBtnDown = 21;
constexpr int kPinBtnOk = 18;

constexpr int kDisplayRawWidth = 168;
constexpr int kDisplayRawHeight = 384;
constexpr uint8_t kDisplayRotation = 1;  // 0/1/2/3 => 0/90/180/270 degrees
}
