# 首次开机字体安装

首次开机和每次重启时，设备检查 SD 卡根目录下的 `/Font`。必需文件为 `font-14.bin`、`font-14.ui.bin`、`font-20.bin`、`font-20.ui.bin`、`font-24.bin`、`font-24.ui.bin`。缺卡时提示插卡；文件不齐或文件头、大小不匹配时，按 OK 开启配网热点。连接热点并在 `192.168.4.1` 填入可上网的 WiFi 后，设备按顺序下载缺失文件。每份文件经过 SHA-256 校验后才安装；全部完成后关闭 WiFi、重启并重新检查。

开机检查仅读取文件头和大小，不扫描约 9.6 MB 的完整字库。下载过程对每份文件做完整 SHA-256。已经存在且头部、大小正常，但正文被外部工具篡改的文件不会在每次开机时被发现；遇到这种情况，应从 SD 卡删除对应文件，再按 OK 重新下载。

## 1Panel 上传

现有 OTA 静态服务将宿主机 `/opt/openbread-ota/data` 映射为 `ota.openbread.net` 的网站根目录，见 [OTA 服务说明](OTA.md)。当前固件使用已上传的目录：

```text
/opt/openbread-ota/data/ota/openbread-normal-one/Font/
```

从本仓库 `tools/font_builder/output/Font/` 上传下面六个文件到这个目录。也可以上传 `tools/font_builder/output/normal-one-fonts-wifi.zip` 并在上一级目录解压；ZIP 内文件位于 `Font/` 中。不要把 `Font` 文件夹再嵌套一层，也不要改文件名。

| 文件 | 大小（字节） | SHA-256 |
|---|---:|---|
| `font-14.bin` | 2,269,533 | `4dd04cceb3efb9a307cdc54e03d2bf69a3b2fd1e069feffbbaabb5ca23c60884` |
| `font-14.ui.bin` | 39,972 | `d526a05746da567d9e3416dce82e340806f6a18fa218d6885bdaf43f7107b15c` |
| `font-20.bin` | 3,342,737 | `2ce6aedd66c52fc34887e66c30e538d8e1b506e749ddb1f1ec354665c0ed528a` |
| `font-20.ui.bin` | 61,042 | `db554f0b985b8ac65370e99c383f5fbef2c822e8a21cce3cb52704ced6e883f9` |
| `font-24.bin` | 4,004,358 | `143a68bb1557b23b24d64953743cfb64bf81a9dffef95cc185a067c1d09cbb1b` |
| `font-24.ui.bin` | 70,020 | `3f9b10ea9aa65734bf8e4c3c4c6bfaaeb782b196608781c1b0f7c208f55fd22a` |

公开地址格式为 `https://ota.openbread.net/ota/openbread-normal-one/Font/<文件名>`。在 1Panel 服务器上核对文件：

```sh
cd /opt/openbread-ota/data/ota/openbread-normal-one/Font
sha256sum font-14.bin font-14.ui.bin font-20.bin font-20.ui.bin font-24.bin font-24.ui.bin
```

再从外网逐份访问公开 HTTPS 地址，确认返回 `200`，下载大小和 SHA-256 与表格一致。本次在原目录更新三份 UI 子集后，需清除对应 URL 的 CDN 缓存，再核对哈希；完整字库没有变化。今后更换字体包可改用新版本目录并同步更新固件的 `FontBundle.h`，避免缓存旧文件与固件哈希不一致。

这些 NBFT 文件由 `SourceHanSansOLD-Normal-2.otf` 生成。仓库未包含该源字体的授权文件；公开上传前，请确认你有权重新分发生成的字形及所需的许可声明。

## 实机检查

1. 不插 SD 卡开机，确认显示“请插入SD卡”，且没有自动开启 WiFi。
2. 插入空卡并按 OK，确认显示“需要下载字体包”；再按 OK，连接屏幕上的热点并打开 `192.168.4.1`。
3. 配网后观察下载进度；完成时确认关闭 WiFi、重启、复查并进入 XMB 主菜单。
4. 分别测试网络中断、空间不足和下载中拔卡；确认错误提示、临时文件清理及再次按 OK 后可重试。

串口使用 `[SETUP]` 前缀报告状态切换、文件安装与失败阶段。字体下载和完整性校验只在用户确认安装后运行；未触发时无线保持关闭。
