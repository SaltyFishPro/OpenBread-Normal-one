# OpenBread Normal One

OpenBread Normal One 是面向 ESP32-S3 的便携设备固件，使用 384 × 168 分辨率的 ST7305 单色屏。项目以电池供电为目标，按需启用 SD 卡、音频、振动和无线功能。

## 固件版本

固件版本与 Build 的唯一来源是 [`src/app/FirmwareInfo.h`](src/app/FirmwareInfo.h)。OTA manifest 和发布包应与该文件保持一致。
- 面向用户的版本变化：[`CHANGELOG.md`](CHANGELOG.md)

## 功能概览

- 主界面：XMB 风格菜单、页面转场、浅睡时钟与按键唤醒。
- 设置与设备：时间校准、设备信息、设备自检、WiFi 配网、OTA 系统更新。
- 阅读与工具：单词阅读、SD 卡 TXT 阅读、专注时钟、定时器、答案之书、今日抽签。
- 音乐：SD 卡歌曲列表、播放控制、收藏与 LRC 歌词。
- 无线：蓝牙相关功能、遥控器，以及设备网页管理后台。
- 游戏：2048、赛车、飞机大战、敲木鱼和心动 App。
- 设备工具：电脑状态看板、SD 卡转 USB。

主菜单仍有课程表和 API 统计板入口，部分详情路由尚为通用占位页；阅读中的提词器也不是完整功能。请以 [`docs/PROJECT_OUTLINE.md`](docs/PROJECT_OUTLINE.md) 的状态表为准。

## 代码结构

- `src/app`：固件入口、版本信息和应用级编排。
- `src/bsp`：屏幕、SD、RTC、IMU、音频、电机和电量等硬件访问。
- `src/services`：字体、音乐、阅读、时间、无线、OTA、自检等业务服务。
- `src/ui`：`UiManager` 负责输入路由和页面切换；`ui/pages` 实现页面业务与绘制。
- `docs`：架构、功能和硬件说明。
- `tools`：字体、页面预览、主机回归与资源生成工具。

## 构建

需要 PlatformIO Core 和仓库配置的 ESP32-S3 工具链：

```powershell
pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor
```

代码、构建配置、资源或固件行为发生变化后，必须运行完整构建。涉及硬件时，编译结果不能替代实机验证。

## 硬件信息

目标板为 ESP32-S3 N16R8，屏幕驱动为 ST7305。引脚和外设配置以 [`src/bsp/BoardConfig.h`](src/bsp/BoardConfig.h) 为准；具体功能约束见 [`docs/PROJECT_OUTLINE.md`](docs/PROJECT_OUTLINE.md) 及对应专题文档。

## 文档维护

新增或修改文档时遵循 [`docs/DOCUMENTATION_GUIDE.md`](docs/DOCUMENTATION_GUIDE.md)。功能实现状态、验证证据和消费者更新日志必须分开记录。
