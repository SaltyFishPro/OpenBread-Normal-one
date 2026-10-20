# SD 字库读取与真实帧缓冲验证

此工具在电脑上编译生产 `SdFontFile`、`SdBitmapFont`、`SdBitmapText`、`SdFontTestPage`、共享标题、ST7305 驱动及 U8g2，读取真实 NBFT 文件。用独立 Python 读取器产生参考字形和图像，核对 C++ 的读取结果与帧缓冲像素。

## 运行

需要 Python 3.10+、`uv` 和 Windows 的微软雅黑（合览标题使用 `C:/Windows/Fonts/msyh.ttc`）。仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/sd_font_tests/build.ps1
```

先用 [字体工具](../font_builder/README.md) 导出 `font-14.bin`、`font-20.bin`、`font-24.bin` 到默认 `tools/font_builder/output/`，或指定路径：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/sd_font_tests/build.ps1 -FontDirectory 'D:/MyFonts/output'
```

默认数据为用户导出的思源旧字形 Normal，覆盖中文、拉丁字母和“𠮷”。其他字体的缺字会按生产占位框策略比较。脚本通过 `uv` 使用 Zig 0.13.0 编译和 Pillow 比较图片，不修改 PlatformIO 环境。字体与生成文件均不入 Git。

## 输出与覆盖范围

输出位于 `tmp/sd-font-test/`：

- `sd-font-framebuffers.png`：三个字号的测试页合览，各页来自真实 384×168 生产帧缓冲，按最近邻放大。
- `page-14.png`、`page-20.png`、`page-24.png`：单页原尺寸。
- `checking.png`、`error-*.png`、`no-card.png`：等待与异常画面；`render-*` 等图片用于像素核对。
- `test.exe`、参考字形和损坏文件夹：主机测试产物，源字库不受修改。

自动验证包括：

- 三份实际 BIN 的文件头、CRC、字形度量、点阵字节、空格、测宽与扩展区码点。
- 四种旋转、负坐标、裁剪、白色墨迹和缺字框；三字号合计 18 组与 Python 参考图逐像素比较。
- 每 tick 最多 8 KiB 的校验、检查中取消、关闭后不再读文件、缓存命中及缺字命中不重复读卡。
- 65,537 条索引、32 位二分查找、超缓存字形按行读取、旧会话字形拒绝。
- 截断、错误文件头、字号不符、CRC 损坏、字形偏移/补位错误、文件读取失败及重试；文件句柄关闭和字号双向循环。

2026-10-21：上述用例通过，18 组像素比较通过。`sizeof(SdBitmapFont)` 在主机编译为 1,744 bytes，这是主机结构大小，ESP32 资源占用应以完整固件构建为准。

## 验证边界

`FS.h` / `SD_MMC.h` 使用电脑只读文件代替真实 SD；仅物理挂载由 `SdCardService::refresh()` 的测试替身提供。显示缓冲布局、旋转及绘制沿用生产驱动，Arduino/SPI 接口使用主机替身。

工具未运行完整 `UiManager` 或自检父页面，不能用此结果替代实际返回路由、GPIO16 切换、SD_MMC 时序、热拔卡、屏幕残影和功耗验收。父页面退出清理与 SD 交接路径另做代码检查，实机清单见 [SD 点阵字库](../../docs/SD_BITMAP_FONTS.md)。涉及生产代码修改后仍须完整 `pio run`。
