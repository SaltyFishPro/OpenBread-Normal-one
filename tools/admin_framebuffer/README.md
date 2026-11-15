# 无线管理设备端缓冲对比

在仓库根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/admin_framebuffer/build.ps1
uv run --with resvg-py --with pillow python tools/admin_framebuffer/export_icons.py
uv run --with pillow python tools/admin_framebuffer/verify.py
uv run --with pillow python tools/admin_framebuffer/compare.py
```

需要 Python、uv；uv 自动提供 Zig 主机编译器。字体输入来自
`tools/font_builder/output/font-{14,20,24}.bin` 和已有 UI 子集。
生成文件在 `tmp/admin-framebuffer/`，不会写入设备或上传固件。

## 数据来源与边界

- 直接编译生产 `DeviceAdminView.cpp`、SD 字体渲染、U8g2 与 ST7305 驱动。
- `.bin` 是驱动的 8064 字节原始帧；`.pgm` 按驱动的实际横屏映射解包为 384×168。
- 覆盖入口、开启中、开启失败、热点开启、关闭确认、管理已关闭、无 SD 卡及重启提示。
- 主机测试检查绘制时 SD 读取为零、没有遗留文件句柄、完全移出屏幕时无坐标回绕、重复绘制稳定。
- `verify.py` 独立解码完整字体文件，逐像素核对关键文本，并核对预览图标和二维码模块。
- QR 主机接口使用预览 SVG 的固定矩阵，仅接受示例热点名。它用于比较尺寸和布局，不能代替 ESP 二维码编码与手机扫码测试。
- 主机不模拟 WiFi、SPI、屏幕刷新时序或按键硬件。主机缓冲通过不代表实机验证完成。

`html-{entry,starting,ready,failed,closed,no-sd}.png` 为浏览器原生 384×168 画面截图，
`before-*.png` 为修改前缓冲，供 `compare.py` 拼图使用。这些临时截图不纳入版本管理。
重新采集时只去除页面外壳的缩放，不调整设备内样式。截图使用微软雅黑和浏览器抗锯齿，
固件使用 SD 点阵字体，因此文字轮廓不能宣称逐像素相同。

固件保持 Left 返回/取消、OK 开启/确认。HTML 的关闭确认弹窗位于模拟屏幕外，
固件使用屏幕底部确认提示，不属于同一画面布局。动态热点名和验证码以设备实际值为准。

修改图标来源后运行 `export_icons.py`，再重新构建缓冲测试。生产图标是静态 1-bit 数组，
运行时无需 SVG 解析、缩放或额外 SD 读取。
