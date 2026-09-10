#pragma once

namespace BoardConfig {
constexpr uint32_t kUiTickMs = 33;
constexpr uint32_t kInputTickMs = 10;
constexpr uint32_t kImuTickMs = 20;
constexpr uint32_t kAlarmTickMs = 1000;

constexpr int kPinTftTe = 45;
constexpr int kPinRst = 46;
constexpr int kPinDc = 4;
constexpr int kPinCs = 3;
constexpr int kPinSclk = 2;
constexpr int kPinSdin = 1;

constexpr int kPinSdData0 = 10;
constexpr int kPinSdCmd = 12;
constexpr int kPinSdClk = 11;
constexpr int kPinSdCs = -1;

constexpr int kPinIicSda = 47;
constexpr int kPinIicScl = 48;

constexpr int kPinButton1 = 21;
constexpr int kPinButton2 = 17;
constexpr int kPinButton3 = 18;
constexpr int kPinButton4 = 14;
constexpr int kPinBoot = 0;

constexpr int kPinBtnLeft = kPinBoot;
constexpr int kPinBtnRight = kPinButton4;
constexpr int kPinBtnUp = kPinButton2;
constexpr int kPinBtnDown = kPinButton1;
constexpr int kPinBtnOk = kPinButton3;

constexpr int kPinPcmDin = 40;
constexpr int kPinPcmLrck = 39;
constexpr int kPinPcmBck = 41;
constexpr int kPinPcmXsmt = 5;
constexpr int kPinNsCtrl = 13;
constexpr int kPinHpCon = 15;
constexpr int kPinSensorLdoEn = 6;
constexpr int kPinAudioLdoEn = 38;
constexpr int kPinDrvEn = 42;
constexpr int kPinMaxInt = 9;
constexpr int kPinPcfInt = 7;
constexpr int kPinQmiInt = 8;

constexpr int kPinSdMiso = kPinSdData0;
constexpr int kPinSdMosi = kPinSdCmd;

constexpr int kDisplayRawWidth = 168;
constexpr int kDisplayRawHeight = 384;
constexpr uint8_t kDisplayRotation = 1;  // 0/1/2/3 => 0/90/180/270 degrees
}
