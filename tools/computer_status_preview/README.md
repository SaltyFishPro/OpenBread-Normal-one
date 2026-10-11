# 电脑状态看板等待页真实帧缓冲预览

在固件根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/computer_status_preview/build.ps1
```

工具复用答案之书预览的 Arduino/SPI 替身与中文字库提取脚本，编译当前 `ComputerStatusPage.cpp`、`DetailHeader.cpp`、`IconBitmap.cpp`、ST7305 驱动和 U8g2。USB 图标来自主菜单的同一资源。捕获真实 8064 bytes 绘制缓冲，再按生产旋转映射导出 384×168 像素。

输出位于 Git 忽略的 `tmp/computer-status-render/`，包含首次与再次进入的转场半屏、动画开始/中段/结束、已连接等待首帧的原尺寸 PNG 与 3 倍整数放大 PNG。合览为 `computer-status-waiting-framebuffers.png`；`intro-sample.gif` 仅由三个时刻组成，供观察图标变化，不代表设备实际播放帧率。

同时检查动画结束后停止请求重绘、连接状态变化触发重绘和重新进入重置动画。目视核对标题、状态条、操作说明、图标与页脚的留白、字形和裁剪；实机另验证进入/返回转场、Left 返回、握手状态、首个完整帧替换及屏幕刷新效果。预览不替代硬件验收。
