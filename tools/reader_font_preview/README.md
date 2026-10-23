# 阅读页 SD 字库试点与真实帧预览

编译生产 `ReaderPage`、`ReaderService`、`SdBitmapFont`、`SdBitmapText`、文件/缓存封装、ST7305 驱动和 U8g2，读取实际导出的 14 号 BIN 与可选常用字索引。从 8064-byte 帧缓冲按驱动旋转映射导出 384×168 图片，使用独立 Python NBFT 读取器核对分页与释义像素。

## 运行

需要 Python 3.10+、`uv`；合览标题使用 Windows 微软雅黑。先从 [字体工具](../font_builder/README.md) 导出文件，然后在仓库根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reader_font_preview/build.ps1
```

默认读取 `tools/font_builder/output/font-14.bin`，同目录若有 `font-14.common.bin` 则一并测试。也可指定 `-FontDirectory 'D:/MyFonts/Font'`。仅在 `tmp/reader-sd-font/` 创建测试卡、词库、错误文件和图片，不修改原字库、不写入真实 SD、不烧录。

## 输出和检查

- `reader-sd-font-framebuffers.png`：短释义、长释义前两页与 Unicode 示例合览；只对真实帧做最近邻缩放。
- `valid-word-*-page-*.png`：每个单词的各页原尺寸图。
- `valid-checking.png`、`layout-pending.png`、`error-*.png`、`no-card.png`：等待、排版及异常图。
- `valid/words/junior.txt`：可另存为 SD 卡 `/words/mine.txt` 的测试词库；不要覆盖自己的已有词库。内容包含短/长释义、扩展区字符、缺字、长英文、空释义与 511-byte 字段边界。

自动检查生产词库解析、左右切词、Up/Down 翻页/边界、切词重置、长按返回语义、CRC/排版中取消和切词、OK 重试、无卡/缺失/CRC 损坏/字号错误/行高不适用、绘制中 I/O 错误清屏、退出后无读取、句柄与外部缓存释放。挂载替身拒绝在字体句柄尚未关闭时刷新 SD。

有 PSRAM/无 PSRAM 分别运行。当前导出字体每种模式完成 20 组释义区域逐像素比较，同时检查独立分页结果、标题与底栏不被正文覆盖、实心/空心箭头状态、翻页往返及带/不带常用字索引一致。稳定页面的 `updateMeaning()` 不再查字或读卡；绘制本身遇到字形缓存未命中仍可能读卡。

## 验证边界

只替换物理 SD 挂载、Arduino/SPI 和堆分配，词库与字库读取、排版和绘制均使用生产代码。PSRAM 替身记录 125,792-byte 峰值并检查释放；这不是设备实际空闲内存或 SD 速度测量。主机不运行完整 `UiManager`，Left 长短按路由、GPIO16 交接和休眠另做代码检查并需实机验收。

完整固件仍须 `pio run -e esp32s3`。实机操作与边界见 [SD 字库文档](../../docs/SD_BITMAP_FONTS.md#阅读页试点2026-10-24)。
