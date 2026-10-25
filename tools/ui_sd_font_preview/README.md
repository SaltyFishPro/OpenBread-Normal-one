# 固定 UI 的 SD 字体与帧缓冲验证

工具使用实际导出的 NBFT 14/20/24 字库，编译生产 `UiFontCache`、`SdUiText`、页面与 ST7305 驱动。只替代文件系统、时间、PSRAM 和硬件服务值，不重写页面布局。生成图来自真实 8064-byte 帧缓冲，按驱动旋转映射回逻辑像素。

在仓库根目录运行（需要 Python、uv，uv 自动提供 Zig 0.13 和 Pillow）：

```powershell
python tools/ui_sd_font_preview/prepare_codepoints.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ui_sd_font_preview/build.ps1
```

输入为 `tools/font_builder/output/font-14.bin`、`font-20.bin`、`font-24.bin`，存在时同时复制 `font-14.common.bin`，并由 `font_ui_subset` 按当前 `UiFontCodepoints.h` 生成 `font-14.ui.bin`；另建一张不含子集的 `validfull` 卡用于回退路径测试。测试文件和输出只写入忽略目录 `tmp/ui-sd-font/`，不改 SD 卡。`prepare.py` 准备无文件/坏 CRC/有效字库和单条阅读样本。`test.cpp` 运行生产代码，`compare.py` 使用电脑端独立 NBFT 解析核对缓存记录、正向/反白文字、横竖屏裁剪、底栏边界及离线日志中英文混排换行。

输出：

- `ui-sd-font-pages.png`：主菜单、USB、弹窗、时间、专注、WiFi/蓝牙、看板等待、答案、阅读和更新页合览。
- 每个页面的 `.pgm`：原始逻辑像素；同名 `.png` 为三倍无插值放大。
- `home-loading.png`、`home-unavailable.png`：不依赖中文字体的加载/缺失提示。
- `home-sleep.png`、`home-sleep-transition.png`：浅睡时钟与扫描线过渡中间帧；测试还逐像素核对原菜单、奇偶位移及最终时钟画面。
- `glyphs.txt`：从 SD 提取的当前固定 UI 码点度量与点阵，对照真实 NBFT。源字体缺失 U+FFFD，运行时用框表示。

检查包含缺文件、CRC 损坏、无 PSRAM/分配失败/连续块不足、加载中取消、重试、120 秒超时、内存和句柄释放；加载完成后禁用 SD，绘制 13 个主菜单入口及其他固定页面，确认读卡和分配次数不增加。检查纯 ASCII 与原 U8g2 像素/测宽一致，内层内容字体作用域结束会恢复固定 UI，状态提示消失后静态图层无残留。检查子集快速路径只做少量顺序读卡、804 条记录与完整字库逐字节一致、子集缺失或损坏时自动回退完整字库路径。固定缓存 61,104 bytes（64KiB 上限），与首次提取临时索引缓存或内容缓存共存峰值 186,896 bytes，不是全系统内存峰值。

慢卡测试模拟每次文件读取耗时 10ms，从接近 uint32 计时回绕处开始，通过生产 `tick()` 加载超过 20 秒直至完成。当前样本模拟耗时 35,500ms，带缓存路径连同 CRC/common 加载读取 3,550 次，旧的内部单索引块路径仅提取阶段即读取 10,539 次；数字只用于回归与读卡开销比较，不代表真实 SD 耗时或倍速。测试还核对取消、超时和完成后临时缓存全部释放；完整管理器的自动休眠限制仍须代码审查与实机验证。

本工具不运行完整 `UiManager`，也不模拟 SD_MMC 时序、GPIO16 路由、休眠或真实音频。管理器取消/交接路径另做代码审查，首次加载时间、物理方向、按键延迟和音乐并发必须实机验收。设备文件准备与步骤见 [SD 字库说明](../../docs/SD_BITMAP_FONTS.md)。

固定文案变更后重新生成码点名单：

```powershell
python tools/ui_sd_font_preview/prepare_codepoints.py
```

将 `src/services/UiFontCodepoints.h` 与代码一起提交。名单保守提取 `src/**/*.cpp`、`src/**/*.h` 双引号字符串中的非 ASCII 字符，并包含可打印 ASCII 和缺字框；注释中的双引号也可能增加少量字符。只生成码点，不生成内置点阵。PlatformIO 通过 `prebuild.py` 校验，名单过期或超过 64KiB 预算会停止构建。当前无需重新导出字体；加入源字体没有的字时仍显示占位框。若以后使用 Unicode 转义或动态生成固定中文，须扩展提取逻辑和测试，不能假定自动覆盖。
