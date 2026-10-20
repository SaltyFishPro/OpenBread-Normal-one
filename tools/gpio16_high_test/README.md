# GPIO16 常高最小测试固件

用于单独检查板载 USB 切换电路。入口只有两次 GPIO 操作：预设 GPIO16 输出值为 1，再设为输出，随后返回，应用任务结束。没有循环、定时器、串口日志或页面，也不调用 Arduino 初始化和 Normal One 主项目。平台只复用 Arduino 包中预编译的 ESP-IDF 库；芯片 ROM、bootloader 和 RTOS 的基础启动仍会执行，GPIO16 在应用入口开始后拉高，并非通电瞬间就能由固件保证电平。

构建使用 ESP32-S3 通用板配置及 16MB Flash；关闭应用 USB 自动启动，使用不自动初始化 PSRAM 的 qio_qspi 库配置。主项目的 N16R8 PSRAM 设置不参与本实验。GPIO16 是实际芯片编号，无外设总线初始化；按板级约定，HIGH 切至 USB-to-SD 芯片。测试不初始化、不挂载 SD 卡。

在仓库根目录执行：

```powershell
pio run -d tools/gpio16_high_test -e gpio16_high
pio run -d tools/gpio16_high_test -e gpio16_high -t upload --upload-port COM6
```

COM6 为本次识别到的 ESP32-S3 下载端口，后续应以 `pio device list` 为准。编译结果位于 `.pio/build/gpio16_high/firmware.bin`（相对于本工具目录），请通过 PlatformIO 上传以写入正确的 bootloader、现有分区表及启动选择。不会整片擦除 Flash；运行固件会被本实验替换。

切换后 ESP32 的 USB 下载串口可能消失，电脑可能枚举为读卡器。检查 GPIO16 对地电压是否接近 3.3V，以及读卡器是否被识别。恢复主固件时，按住 BOOT（Left），短按复位后松开 BOOT，再用主项目烧录。屏幕可能保留上次画面，本测试不会更新显示；不能据此判断是否运行。

本测试按用户要求不输出应用日志，使用引脚电压和电脑 USB 枚举验收。编译、烧录校验与实际电平/读卡识别分别记录，避免把编译成功当作硬件验证。

## 2026-10-20 构建检查

- `pio run -d tools/gpio16_high_test -e gpio16_high` 通过；RAM 18,760 bytes（5.7%），Flash 221,703 bytes（3.0%），应用 BIN 为 222,112 bytes。
- 反汇编确认 `app_main` 仅调用 `gpio_set_level(16, 1)` 和 `gpio_set_direction(16, GPIO_MODE_OUTPUT)` 后返回；符号表中不存在 `initArduino`、`loopTask`、`psramInit`、`UiManager` 或 WiFi 初始化入口。
- esptool 镜像检查通过，目标 ESP32-S3、16MB Flash，校验和与镜像哈希有效。构建日志位于仓库根目录 `tmp/gpio16-high-build-20261020.log`。
- 首次上传因 Windows COM6 错误 31 无法配置端口，未开始写入；以重新连接后的烧录校验结果为准。
- 用户重新插入并进入下载模式后，再次向 COM6 上传返回 `SUCCESS`（10.94 秒）。上传期间 PlatformIO 的输出线程遇到 GBK 进度字符编码异常，完整逐段校验输出未被日志捕获；后续已用 UTF-8 环境读取诊断结果。上传日志位于 `tmp/gpio16-high-upload-retry-20261020.log`。
- 烧录后 Windows 仍列出 COM6，但额外执行 `verify-flash` 时打开端口返回错误 31，未完成独立回读校验；当时未枚举到读卡器。实际 GPIO16 电平与读卡器识别仍待用户实测，不将端口不可用当作 GPIO 高电平的证据。
- 烧录工具自动识别物理 Flash 为 32MB，本测试构建按原项目 16MB 地址范围使用，写入均在该范围内。应用 BIN 的 SHA256：`30fd43d9215baa996bae8ae3763552a356181ff823ad61a83726b7a6b1256b60`。
- 2026-10-21：用户确认最小常高固件实机验证通过，并开始通过读卡方式复制 SD 字库。此结论针对最小 GPIO16 测试，不代表主菜单切换、退出恢复和 SD 字库读取已全部验收。
