# OpenBread Normal One

OpenBread 是一个基于 ESP32-S3 的 2.9 寸单色反射屏便携设备固件项目。  
当前仓库是**全新重构版本**，架构按 `App Core -> Services -> BSP -> UI` 分层实现。

## 项目状态

项目仍在开发中，当前可用的是：
- 主菜单纵向滚动（`Up/Down`）
- 主菜单 <-> 子菜单的分段转场动画
- 基础页面路由（Home / Section / Detail）
- Settings 子菜单图标联动（Focus item 对应右侧大图标）
- Settings 弹窗（Popup）：`重启设备` 确认窗口（Yes/No）
- Language 切换（中文 / English，保持 U8G2 font 不变）
- 关于设备（About Device）两页详情（含 Device ID、Flash total capacity）
- 子菜单 <-> 详情页分段转场动画（Section <-> Detail）
- WiFi 配网（Portal/连接/状态反馈）
- OTA 检查更新（manifest）
- OTA 下载与 SHA256 校验
- OTA 应用重启与重启后版本核验（post-apply verify）

尚未完成的是：
- Section/Detail 的真实业务内容接入（目前为占位）
- Music / Reader / Alarm / Remote / Settings 的完整功能链路

## OTA 当前能力（已实现）

- 协议字段：`product/channel/version/build/firmware_url/sha256/size/release_note`
- 升级判定：以 `build` 为主（`remote.build > local.build`）
- 状态流：
  - `Idle -> Checking -> UpdateAvailable/UpToDate`
  - `UpdateAvailable -> Downloading -> Verifying -> ReadyToApply`
  - `ReadyToApply -> Applying -> reboot`
- 安全约束：
  - URL 必须 `https`
  - `size` 必须有效且不超过分区上限
  - `sha256` 必须 64 hex 且下载后全量比对
- 日志前缀：`[OTA]` / `[ERR][OTA]`
- 启动后核验：对比重启前目标版本，输出 post-apply verify 结果

## OTA 发布与服务器同步（推荐）

- 设备固件版本由 `src/app/FirmwareInfo.h` 管理（`kVersion/kBuild`）
- 服务器 manifest 与设备版本信息必须一致
- 推荐使用 1Panel 计划任务在服务器侧拉取 GitHub 最新 Release 并更新 manifest（你手动发布 Release，服务器自动同步）
- 发布最小规则：
  - Release 资产包含 `firmware-*.bin`
  - Release 文本包含 `build: <int>`

## 执行表（Roadmap）

| 阶段 | 模块 | 当前状态 | 下一次更新会做什么 | 目标结果 |
|---|---|---|---|---|
| P0 | UI 过渡动画 | 进行中 | 继续微调主菜单->子菜单与返回动画节奏，清理边缘闪烁/错位 | 转场稳定、节奏统一 |
| P0 | Section/Detail 页面 | 进行中 | 扩展更多真实 Detail 内容并完善分页/数据读取 | 可进入真实业务流程 |
| P1 | Settings | 进行中 | 继续补齐设置项持久化（Language、Device 信息、存储状态） | 设置可保存、重启可恢复 |
| P1 | Reader | 未完成 | 建立分页、书签与文本加载流程 | 可连续翻页阅读 |
| P1 | Music | 未完成 | 建立曲库索引、播放状态与 UI 联动 | 可播放/暂停/切歌 |
| P2 | Alarm | 未完成 | 接 RTC 与闹钟计划管理，打通提示链路 | 可创建并触发闹钟 |
| P2 | Remote | 未完成 | 建立遥控命令封装与发送接口 | 可发送基础遥控指令 |
| P2 | 稳定性与测试 | 未完成 | 增加关键路径回归与长时间运行测试 | 长时间稳定运行 |

## 目录概览

```txt
src/
  app/        # 状态机、调度、事件流
  bsp/        # 硬件驱动抽象
  services/   # 业务服务层
  ui/         # 页面、渲染、主题
  data/       # 模型与存储占位
```

详细设计请见 [`PROJECT_OUTLINE.md`](PROJECT_OUTLINE.md)。

## 本地开发

### 环境
- PlatformIO
- ESP32-S3 开发板
- 约定：统一使用 PIO CLI（`pio`）命令执行构建/烧录/监视

### 常用命令

```bash
pio run
pio run -t upload
pio device monitor
```

## 说明

- 本仓库历史已重写为新工程基线，旧版本仅作为历史参考，不再维护。
- 后续每次功能推进会同步更新本 README 的“执行表”。
