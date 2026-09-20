#pragma once

#include <stdint.h>

// USB CDC（USB Serial/JTAG）串口开关。
// 关闭后 HWCDC::end() 会释放收发缓冲并把 D+/D- 拉低，主机侧看到设备断开；
// 此后所有日志的 `if (!Serial)` 判断自动为假，写操作也退化为安全空操作。
// 状态写入 NVS，下次上电沿用；开机时按住 OK 键可强制恢复开启。
namespace UsbSerial {

constexpr uint32_t kBaud = 115200;

// 按 NVS 记录决定开机是否开启串口。
// 关闭时必须显式调用 Serial.end()：芯片复位后 USB Serial/JTAG 处于激活状态，
// 仅“不调用 begin”并不会让主机看到设备断开。
bool begin();

// 切换开关并持久化，返回切换后的状态。
bool setEnabled(bool enabled);

bool isEnabled();

}  // namespace UsbSerial
