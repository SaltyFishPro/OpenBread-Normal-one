# OpenBread Normal One

一个基于 ESP32-S3 的便携设备固件项目，屏幕是 2.9 寸单色反射屏。目标是做一台电池供电、低功耗的小工具，围绕音乐、阅读、闹钟、遥控和设置这几个功能展开。

## 当前进度

固件已经能稳定跑起来，下面这些是已经接通的：

- 主菜单纵向滚动，上下键切换，焦点框固定在中间。
- 主菜单、子菜单、详情页三层路由，进出场动画统一。
- 设置页的重启确认弹窗、设备信息（设备 ID、Flash 容量、SD 状态）。
- WiFi 配网和 OTA 全流程：检查更新、下载、SHA256 校验、应用重启、重启后核验。
- 设备自检页，覆盖屏幕、按键、I2C、RTC、SD、IMU 和电量。
- 主界面 20 秒无操作自动进入 light sleep，屏幕保留画面降功耗，按任意键唤醒。

还没做完的是音乐、阅读、闹钟、遥控的完整业务链路，子菜单和详情页里有不少还是占位内容。

## 代码结构

代码按 `App -> Services -> BSP` 分层，UI 是独立的表现层：

- `src/app`：状态机、事件、调度相关的骨架。
- `src/bsp`：屏幕、SD、RTC、IMU、DAC、马达、电量计等硬件驱动。
- `src/services`：WiFi 配网、OTA、时间、蓝牙、音乐、阅读、遥控、自检等业务。
- `src/ui`：UiManager 负责路由和转场，pages 目录里是各个页面。

## 编译

使用 PlatformIO，板子是 ESP32-S3（N16R8）：

```bash
pio run
pio run -t upload
pio device monitor
```

固件版本在 `src/app/FirmwareInfo.h` 里维护，OTA 的 manifest 需要跟它保持一致。

## 硬件

主要引脚定义在 `src/bsp/BoardConfig.h`。按键是 Up、Down、OK、Left、Right，屏幕驱动是 ST7305，RTC 是 PCF85063A，IMU 是 QMI8658A，电量计是 MAX17048。
