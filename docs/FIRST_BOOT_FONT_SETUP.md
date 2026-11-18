# 首次开机字体安装

首次开机和每次重启时，设备检查 SD 卡根目录下的 `/Font`。必需文件为 `font-14.bin`、`font-14.ui.bin`、`font-20.bin`、`font-20.ui.bin`、`font-24.bin`、`font-24.ui.bin`。缺卡时提示插卡；文件不齐或文件头、大小不匹配时，按 OK 开启配网热点。连接热点并在 `192.168.4.1` 填入可上网的 WiFi 后，设备按顺序下载缺失文件。每份文件经过 SHA-256 校验后才安装；全部完成后关闭 WiFi、重启并重新检查。

开机检查仅读取文件头和大小，不扫描约 9.6 MB 的完整字库。下载过程对每份文件做完整 SHA-256。已经存在且头部、大小正常，但正文被外部工具篡改的文件不会在每次开机时被发现；遇到这种情况，应从 SD 卡删除对应文件，再按 OK 重新下载。

## 1Panel 上传

下载或网络失败后显示“是否重新配置 WiFi？”，默认“否，重试下载”。Up/Down 切换，
OK 执行选项，Left 回到默认选项。选择否会复用本次输入的凭据重新连接，不重新开放热点；
选择是才重新配网。已完成且通过检查的文件跳过，失败文件重新下载，不支持文件内部断点续传。
等待选择时关闭无线并释放 HTTP/临时文件，凭据只保留于现有配网服务；不额外复制或记录密码。
SD 卡、空间和写入错误使用独立提示，OK 检查后优先复用原网络。
没有可复用凭据（例如首次热点启动失败）时，重试仍会开启配网热点。
串口可查 `[SETUP] retry requested` 和 `[WIFI] font setup reconnect`，不输出密码。
主机回归：`tools/font_setup_tests/build.ps1`、`tools/wifi_device_tests/build.ps1`；
实机需检查按钮焦点、断网重试、重新配网、字体校验与完成后的重启。

现有 OTA 静态服务将宿主机 `/opt/openbread-ota/data` 映射为 `ota.openbread.net` 的网站根目录，见 [OTA 服务说明](OTA.md)。字体统一使用以下固定目录：

```text
/opt/openbread-ota/data/ota/openbread-normal-one/Font/
```

从本仓库 `tools/font_builder/output/Font/` 上传下面六个文件到这个目录。不要把 `Font` 文件夹再嵌套一层，也不要改文件名。

| 文件 | 大小（字节） | SHA-256 |
|---|---:|---|
| `font-14.bin` | 2,269,533 | `4dd04cceb3efb9a307cdc54e03d2bf69a3b2fd1e069feffbbaabb5ca23c60884` |
| `font-14.ui.bin` | 183,899 | `c222f8da3df3b754bc60efe502d06f06b51033195e3a27eeb22b1e0f08b17ec7` |
| `font-20.bin` | 3,342,737 | `2ce6aedd66c52fc34887e66c30e538d8e1b506e749ddb1f1ec354665c0ed528a` |
| `font-20.ui.bin` | 286,053 | `6fd218ec0b3612aaba7f6fd9d255fa448d556ed9b0fc707f21c553e3e5051db8` |
| `font-24.bin` | 4,004,358 | `143a68bb1557b23b24d64953743cfb64bf81a9dffef95cc185a067c1d09cbb1b` |
| `font-24.ui.bin` | 325,920 | `a3d7df2d351e0542026c28e711fce29a403a729a95e8f449e753ea7a9efb6491` |

公开地址格式为 `https://ota.openbread.net/ota/openbread-normal-one/Font/<文件名>`，注意 `Font` 的大小写。在 1Panel 服务器上核对文件：

```sh
cd /opt/openbread-ota/data/ota/openbread-normal-one/Font
sha256sum font-14.bin font-14.ui.bin font-20.bin font-20.ui.bin font-24.bin font-24.ui.bin
```

再从外网逐份访问公开 HTTPS 地址，确认返回 `200`，下载大小和 SHA-256 与表格一致。本次在原目录更新三份 UI 子集后，需清除对应 URL 的 CDN 缓存，再核对哈希；完整字库没有变化。今后字体包仍使用该固定目录，更新文件时同步核对固件清单与 CDN 缓存。

这些 NBFT 文件由 `SourceHanSansOLD-Normal-2.otf` 生成。仓库未包含该源字体的授权文件；公开上传前，请确认你有权重新分发生成的字形及所需的许可声明。

## 实机检查

### 下载重试的连接清理

字体请求禁用 HTTP keep-alive。成功、失败和取消时先停止 TLS，随后结束 HTTP 会话，最后由配网服务关闭无线，避免断网重连后访问上次残留的响应。等待用户选择期间不保持无线连接，也不自动循环下载。

`[SETUP] font request url=...` 用于核对真实请求地址；`font transport closed before radio release` 表示 HTTP 会话已清理。404 会额外提示检查字体包目录和 CDN 缓存。404 是服务端文件不可用，重试机制无法代替上传文件。

主机回归覆盖重复 404 后复用原 WiFi、HTTP 初始化失败、长度不符、响应体断开/超时、WiFi 掉线，以及原有重新配网、取消和跳过已安装文件。测试模型检查 TLS 在无线关闭前释放；真实 TLS/lwIP 崩溃是否消失仍需刷机反复重试确认。

1. 不插 SD 卡开机，确认显示“请插入SD卡”，且没有自动开启 WiFi。
2. 插入空卡并按 OK，确认显示“需要下载字体包”；再按 OK，连接屏幕上的热点并打开 `192.168.4.1`。
3. 配网后观察下载进度；完成时确认关闭 WiFi、重启、复查并进入 XMB 主菜单。
4. 分别测试网络中断、空间不足和下载中拔卡；确认错误提示、临时文件清理及再次按 OK 后可重试。

串口使用 `[SETUP]` 前缀报告状态切换、文件安装与失败阶段。字体下载和完整性校验只在用户确认安装后运行；未触发时无线保持关闭。
