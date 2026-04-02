# OpenBread OTA 实施大纲（分步执行）

本文基于当前仓库结构（`UiManager + SettingsPage + WifiProvisionService`）制定，目标是先安全完成“检查更新”，再上线“下载并升级”。

## 0. 先冻结 OTA 协议（先做，不写代码）

### 0.1 Manifest 固定字段
- `product`：必须等于 `openbread-normal-one`
- `channel`：`stable / beta`
- `version`：展示用版本字符串，如 `0.1.1`
- `build`：整型单调递增版本号（设备端主判定字段）
- `firmware_url`：完整 HTTPS 下载地址
- `sha256`：固件文件 SHA-256（64 hex）
- `size`：字节数
- `release_note`：更新说明

### 0.2 设备端判定规则
- 先校验 `product`、`channel`、字段完整性，不通过直接拒绝升级。
- 版本比较优先用 `build`，`version` 仅用于 UI 展示。
- 仅允许 `remote.build > local.build` 时出现“可更新”。

### 0.3 失败策略
- Manifest 解析失败、字段缺失、类型错误、下载 URL 非 HTTPS：进入 `Error` 状态，不触发升级。
- SHA256 不匹配：升级失败并回到 `Idle`，保留旧版本。

## 1. 第一阶段：只做 OTA 检查（不下载，不重启）

### 1.1 模块归属
- 新增 `src/services/OtaService.h/.cpp`（业务逻辑，不做绘制）。
- `UiManager` 仅负责输入路由与调用 `OtaService::tick()`。
- `SettingsPage` 只负责 OTA 详情页文本与状态展示。

### 1.2 OtaService 最小状态机
- `Idle`
- `Checking`
- `UpToDate`
- `UpdateAvailable`
- `Error`

### 1.3 OtaService 对外接口（建议）
- `bool begin(uint32_t localBuild, const char* localVersion);`
- `void tick(uint32_t nowMs);`
- `bool requestCheck();`  // 用户在 OTA 页面按 OK 触发
- `State state() const;`
- `const ManifestInfo& manifest() const;`
- `Error error() const;`
- `bool consumeChanged();`

### 1.4 网络与功耗策略
- 仅当用户进入 OTA 详情并按 OK 才允许联网检查。
- 检查结束立即断网（沿用你现有按需联网思路，不常驻 WiFi）。

## 2. 第二阶段：下载 + SHA256 校验 + 本地完整性检查

### 2.1 状态机扩展
- `Downloading`
- `Verifying`
- `ReadyToApply`（可选：用于“确认升级”）
- `Applying`

### 2.2 关键实现点
- 使用 `HTTPClient` + `Update` 分块写入 OTA 分区，避免整包缓存进 RAM。
- 每次写入更新下载进度（字节级），供 UI 显示进度条。
- 下载完成后执行 SHA256 比对，必须与 Manifest 一致。

### 2.3 安全兜底
- `size` 超过 `board_upload.maximum_size` 直接拒绝（当前为 7340032）。
- 下载中断/超时要可恢复到 `Idle/Error`，并清理会话资源。
- 不允许在后台静默升级，必须由用户明确触发。

## 3. 第三阶段：真正升级与重启收口

### 3.1 升级触发
- OTA 页面显示“可更新”后，用户再次 OK 才执行升级。
- `Left` 始终为返回，不改变按键语义。

### 3.2 升级后验证
- 重启后读取本地 `build/version`，校验是否已切换到新版本。
- 若仍为旧版本，记录错误码并提示升级失败。

## 4. UI 接入步骤（按现有交互规则）

### 4.1 Settings 菜单
- 现有 OTA 菜单项（index=2）接入 OTA 详情页内容。
- 文案状态建议：`未检查 / 检查中 / 可更新 / 已最新 / 失败`。

### 4.2 按键一致性
- `Up/Down`：仅翻页或焦点移动。
- `OK`：检查更新 / 确认升级。
- `Left`：返回上一级，必要时取消进行中的 OTA 会话。

## 5. 日志规范（硬件相关必须）

- 统一前缀：`[OTA]`、错误用 `[ERR][OTA]`。
- 日志建议点：
- 开始检查 URL
- HTTP 返回码
- Manifest 关键字段（build/size/hash 前 8 位）
- 下载进度（建议 5% 或 10% 节点打印）
- SHA256 校验结果
- `Update.end()` 结果与错误码

并通过宏开关控制，例如：
- `#ifndef OB_OTA_LOG_ENABLED`
- `#define OB_OTA_LOG_ENABLED 1`
- `#endif`

## 6. 与你当前服务端 JSON 的对齐建议

你给出的 Manifest：
- `product=openbread-normal-one`
- `channel=stable`
- `version=0.1.1`
- `build=2`
- `firmware_url=.../firmware-0.1.1.bin`
- `sha256=2bbe...2a59`
- `size=2638272`

可直接使用。设备端需要补充的仅是：
- 本地 `build/version/channel` 常量
- JSON 字段严格校验
- SHA256 全量比对

## 7. 建议执行顺序（你可以按这个节奏推进）

1. 先落地 `OtaService` 第一阶段（只检查更新）。
2. 接入 `UiManager` 与 `SettingsPage` OTA 详情状态展示。
3. 完成编译与 UI 交互回归（按键语义不变）。
4. 再加下载与 SHA256 校验。
5. 最后接入升级确认与重启验证。
