# OpenBread Normal One

OpenBread 是一个基于 ESP32-S3 的 2.9 寸单色反射屏便携设备固件项目。  
当前仓库是**全新重构版本**，架构按 `App Core -> Services -> BSP -> UI` 分层实现。

## 项目状态

项目仍在开发中，当前可用的是：
- 主菜单纵向滚动（`Up/Down`）
- 主菜单 <-> 子菜单的分段转场动画
- 基础页面路由（Home / Section / Detail）

尚未完成的是：
- Section/Detail 的真实业务内容接入（目前为占位）
- Music / Reader / Alarm / Remote / Settings 的完整功能链路

## 执行表（Roadmap）

| 阶段 | 模块 | 当前状态 | 下一次更新会做什么 | 目标结果 |
|---|---|---|---|---|
| P0 | UI 过渡动画 | 进行中 | 继续微调主菜单->子菜单与返回动画节奏，清理边缘闪烁/错位 | 转场稳定、节奏统一 |
| P0 | Section/Detail 页面 | 未完成 | 把占位页替换为真实子菜单列表与详情结构 | 可进入真实业务流程 |
| P1 | Settings | 未完成 | 接入设置项模型、读写接口与页面交互 | 设置可保存、重启可恢复 |
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

### 常用命令

```bash
pio run
pio run -t upload
pio device monitor
```

## 说明

- 本仓库历史已重写为新工程基线，旧版本仅作为历史参考，不再维护。
- 后续每次功能推进会同步更新本 README 的“执行表”。
