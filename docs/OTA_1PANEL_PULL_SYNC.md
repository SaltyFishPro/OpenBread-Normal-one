# OTA Server Pull Sync (1Panel)

This is the mode you requested:

- You manually publish a GitHub Release.
- 1Panel server automatically pulls latest Release firmware.
- Server computes `sha256` and `size` automatically.
- Server rewrites `manifest.json` automatically.

The generated manifest keeps the required `release_note` summary and also includes
an optional `release_notes` UTF-8 string (up to 767 bytes) for the device's scrolling
System Update page. Older firmware ignores the optional field.

## Script

Use this script from this repo:

- `tools/ota/pull_release_to_manifest.py`

## Release Convention (Important)

1. Release asset must include firmware bin named:
- `firmware-<version>.bin`

Example:
- `firmware-0.1.2.bin`

2. Release body (or title/tag) must include build number:
- `build: 3`

This build is used for OTA version decision.

## 1Panel Scheduled Task Example

Add a scheduled task (for example every 5 minutes):

```bash
python3 /opt/openbread-ota/scripts/pull_release_to_manifest.py \
  --repo SaltyFishPro/OpenBread-Normal-one \
  --product openbread-normal-one \
  --channel stable \
  --output-dir /opt/openbread-ota/data/ota/openbread-normal-one \
  --public-base-url https://ota.openbread.net/ota/openbread-normal-one \
  --asset-regex '^firmware-.*\.bin$'
```

If your repo is private or to avoid GitHub API rate limits:

```bash
export GITHUB_TOKEN=ghp_xxx
python3 /opt/openbread-ota/scripts/pull_release_to_manifest.py ...
```

## Suggested Server Layout

```txt
/opt/openbread-ota/
  data/
    ota/
      openbread-normal-one/
        manifest.json
        firmware-0.1.1.bin
        firmware-0.1.2.bin
  scripts/
    pull_release_to_manifest.py
```

## Setup Steps

1. Copy script to server:
- `/opt/openbread-ota/scripts/pull_release_to_manifest.py`

2. Make script executable (optional):

```bash
chmod +x /opt/openbread-ota/scripts/pull_release_to_manifest.py
```

3. Create 1Panel scheduled task with the command above.

4. Publish a new GitHub Release manually.

5. Check task log for:
- `[OTA_SYNC] manifest updated ...`

6. Verify URL:
- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

## Failure Diagnostics

- `Cannot find build number...`
  - Add `build: <int>` in release body/title/tag.
- `No asset matched regex...`
  - Ensure release asset name matches `firmware-*.bin`.
- `HTTP error 403/404`
  - Check repo visibility, token, owner/repo name.

## Note

This flow is server-pull based and does not require GitHub Actions deployment.

## 发布闭环（当前固件流程）

当前设备协议以 `src/app/FirmwareInfo.h` 和 `src/services/OtaService.cpp` 为准：使用 `firmware_url`、`release_note`，并可选使用 `release_notes` 长日志；设备只接受匹配的 `product=openbread-normal-one`、`channel=stable` 和 HTTPS 固件地址。`docs/OTA.md` 中早期示例的 `bin_url` 不适用于当前设备。

1. 读取 GitHub 最新正式 Release 与线上 `manifest.json`，选择更大的 build。同步修改 `FirmwareInfo.h` 的 `kVersion` 与 `kBuild`；仅改 Release 标题不能更新固件内的版本。
2. 更新 `CHANGELOG.md`，执行 `pio run -e esp32s3` 与相关回归检查。OTA 附件必须来自 `.pio/build/esp32s3/firmware.bin`，不能使用带 bootloader / 分区表的合并烧录镜像；当前 OTA 应用分区上限为 7,340,032 bytes。
3. 将最终源码提交并推送，发布标签 `v<version>` 必须指向这次提交。先创建草稿 Release，再上传 `firmware-<version>.bin`，确认附件完整后发布为正式版，供服务器 `/releases/latest` 拉取。
4. Release 正文第一行写简短更新摘要，服务器会将它保存为 `release_note`；后续单独写 `build: <整数>`，必须与 `FirmwareInfo.h` 一致。记录本地固件大小和 SHA256，发布后下载 GitHub 附件核对。
5. 等待 1Panel 定时任务同步，访问 `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`，核对产品、通道、版本、build、下载地址、大小与 SHA256。随后从清单指定的 HTTPS 地址完整下载固件，与本地和 GitHub 附件进行一致性核对；清单可访问不等于发布已同步。
6. 设备进入「设置 → OTA更新」，按 OK 检查更新，再按 OK 下载。下载校验完成后按 OK 打开应用确认弹窗，选择「应用」并重启。USB 串口已开启时，可记录 `[OTA]` / `[ERR][OTA]` 日志；串口采集不要切换 DTR/RTS，以免意外复位。
7. 重启后核对本地版本和「上次应用结果」，再次检查应显示已是最新版本。当前 `post-apply verify success` 日志记录预期与实际 build/version，可作为实机闭环证据。没有完成这一步时，只能标记发布链路通过、实机 OTA 待验收。

发布验证记录与下载产物可放在 Git 忽略的 `tmp/ota-release-<version>/`。记录提交 SHA、标签、Release URL、清单、固件大小/哈希、编译 RAM/Flash、检查时间，以及设备端验收结果；不记录登录令牌或 WiFi 密码。
