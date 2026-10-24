# 内置字库裁剪与 SD 字体迁移验证

本工具验证 SD 字体迁移后的页面行为。当前固件已移除 `chinese_font_all`；只保留七段数字和 U8g2 拉丁/数字字体，中文页面内容由 SD NBFT 字体提供。

本工具不生成任何内置中文字库。外部日志、词条、歌曲名和 WiFi 信息在 SD 可用时走完整 NBFT，未知字符有占位框；纯数字与基础英文沿用现有 U8g2。固定中文 UI 现在由独立的 SD 字形缓存提供，见 [固定 UI 验证](../ui_sd_font_preview/README.md)。

新增或更改固定文案后运行：

```powershell
python tools/ui_sd_font_preview/prepare_codepoints.py
```

将生成的 `src/services/UiFontCodepoints.h` 与文案一起提交。它只保存码点，点阵运行时从 SD 读取；PlatformIO 构建前自动 `--check`，名单过期或超出缓存预算会停止构建。

集成验证（仓库根目录，先运行一次阅读页工具准备词库样本）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reader_font_preview/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/sd_font_migration/build.ps1
```

需要 Python、uv。工具使用 `tools/font_builder/output/` 内当前真实 14/20/24 字库，主机替代 SD/Arduino，编译生产页面、字库服务、U8g2 与 ST7305 驱动。有/无 PSRAM 两种模式分别检查：

- 保留的英文/数字字体可用；未提供的中文和扩展字符不会错误映射为其他字形。
- 268 条答案按 20 号字可完整放入两行；答案、三种历史焦点、24 号阅读标题/音标、14 号日志与外部 UTF-8 文字逐像素匹配 Python 独立 NBFT 解码。
- 等待/揭晓、返回历史、自动/手动日志滚动、弹窗中断、分步排版、静止时不读卡、字体关闭回退。
- 主字号与第二字号共存，第二字号不重复申请 PSRAM；阅读重试前关闭第二句柄，退出和缓存模式切换释放资源。

输出为 `tmp/sd-font-migration/sd-font-stages-3-5.png`，使用真实 8064-byte 帧缓冲，按驱动旋转映射生成 384×168 PGM 后无插值放大。界面中的版本和远程说明属于主机测试数据，不修改固件版本或发布 OTA。

答案、系统更新、SD 测试、SD转USB、电脑看板预览分别覆盖原页面的状态/导航，不替代实机验收。本工具刻意不安装外层固定 UI 缓存，用于检查内容字体与原基础字体回退；固定中文和内容字体共存由 `ui_sd_font_preview` 验证。只有固定缓存和内容字体均未就绪时，中文固定文案才可能为空。完整范围、资源与设备步骤见 [SD 字库说明](../../docs/SD_BITMAP_FONTS.md)。工具不运行完整 `UiManager`；其页面切换、USB 交接、休眠和音频并发另经代码审查，并需在设备上验收。
