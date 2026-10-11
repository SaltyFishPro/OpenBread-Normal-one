# 电脑状态看板协议回归验证

在固件项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/computer_status_protocol_tests/build.ps1
```

工具使用 Zig 0.13.0 编译当前 `ComputerStatusProtocol.cpp`，通过仅用于测试的 Arduino 串口替身输入真实协议包，并验证设备输出。测试端的容器不进入固件构建。

验证内容：HELLO_ACK 和每个 ACK/NAK 的长度与 CRC、8064 bytes 的完整帧、重复数据块、完整帧 CRC 错误拒绝、显示完成后的重复 FRAME_END。可检测 ACK 校验码偏移写错或越界导致的无效确认包。

串口替身模拟 256 bytes RX 队列，整包先突发送入，再由 `tick()` 读取；超过容量会丢字节并使测试失败。数据块使用 HELLO_ACK 返回的负载大小，当前为 240 bytes（236 bytes 像素），整个包为 255 bytes。另验证较小主机上限、噪声后的魔数恢复、坏 CRC 后恢复，以及残缺包等待 251 ms 后的重连和重发；时间由测试时钟控制，无需实际等待。

该验证不能代替实际 USB 驱动、屏幕方向和按键返回测试。固件编译仍须另行执行 `pio run -e esp32s3`。
