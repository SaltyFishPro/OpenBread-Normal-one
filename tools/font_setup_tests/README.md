# 字体下载失败重试回归

在仓库根目录执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/font_setup_tests/build.ps1`。
需要 Python、uv 及 `tools/font_builder/output/font-14.bin`。

编译生产 FontSetupService、WifiProvisionService、FirstBootPage 和 ST7305 驱动，
使用模拟 SD/HTTP/WiFi 测试 HTTP 503、默认原网络重试、重复失败、无自动重试、
重新配网、跳过已安装文件、取消与无线关闭。密码不输出。
输出 `tmp/font-retry-{default,reconfigure,left}.pgm` 用于检查内置字体和焦点布局。
模拟 HTTP 故意始终返回 503，不验证真实 TLS、SD 写入或 SHA-256。
需配合 `tools/wifi_device_tests/build.ps1` 和完整 `pio run`，并在设备验证下载流程。
