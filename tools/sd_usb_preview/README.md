# SD卡转USB：预览与验证

在仓库根目录运行（需要 Python 和 `uv`）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/sd_usb_preview/build.ps1
```

工具编译实际 `HomePage`、`SdUsbPage`、共享图标/标题、ST7305 和 U8g2 绘制代码，提取固件当前保留字体，从 8064 字节帧缓冲按当前屏幕旋转映射导出 384×168 图片。Arduino/SPI 使用主机替代实现，GPIO 测试单独编译实际 `SdUsbSwitch.cpp` 并记录引脚操作。

输出位于 `tmp/sd-usb-render/`，合览为 `sd-usb-framebuffers.png`；另有主页、进入转场、开启和未开启状态的原尺寸/3 倍 PNG。主机自检覆盖 13 个菜单项、首尾双向切换、OK 进入请求、静态重复绘制、状态差异、转场偏移绘制，以及 GPIO16 初始化 LOW、进入 HIGH、退出 LOW、重复设置和重新初始化。该工具不运行完整 `UiManager`，SD 文件释放与两个 Left 路径的接线仍需代码审查及实机验证。

## 图标来源与转换

源图：`wired-lineal-1312-micro-sd-card-in-reveal.gif`（原图 256×256、41 帧）。与主菜单资源一致，预处理为 100×100、1-bit、RLE；隔帧采样并去除连续重复帧，最终 20 帧、每帧槽 914 bytes，像素数组合计 18,280 bytes。详情页复用同一资源的完整末帧，显示为 88×88，不另存一份资源。

```powershell
python tools/gif_to_c/gif_to_c.py 'F:/下载/wired-lineal-1312-micro-sd-card-in-reveal.gif' -o src/ui/assets/main_menu/sd_usb.h -n sd_usb -W 100 -H 100 --resize lanczos --dedupe --rle --frame-step 2 --frame-delay 50
```

## 控制与硬件验收

- 主菜单中位于「电脑状态看板」之后；OK 进入，Up/Down 无额外动作，Left 或长按 Left 返回。
- `UiManager::begin` 在 SD 初始化前调用开关初始化，将 GPIO16 配为输出并拉低。
- 完成进入转场后，先清除阅读缓存，再调用现有音乐退出路径停止音频/扫描、关闭文件并卸载 SD_MMC，最后 GPIO16 拉高。
- Left 返回处理在开始退场动画前立即将 GPIO16 拉低；返回后不主动挂载 SD，下次打开音乐/阅读/卡信息时按原流程挂载。
- SD 使用 **SD_MMC 1-bit**，CLK=11、CMD=12、D0=10，并非 SPI；`BoardConfig` 中 MISO/MOSI 是旧别名。假设板载芯片由 GPIO16 HIGH 切至 USB 读卡、LOW 恢复正常 USB，不新增软件 USB MSC 或芯片专用控制协议。
- 开关日志前缀为 `[SDUSB]`，`OB_SD_USB_LOG_ENABLED=0` 可关闭；HIGH 日志在切换前输出，切到读卡器后 ESP32 串口可能不可见。

实机按以下顺序验收：上电默认 LOW → 进入后 HIGH、电脑枚举读卡器 → 读写 SD 文件 → 在电脑安全弹出 → Left 返回后 LOW → 再次进入可用。再检查长按 Left、未插卡时返回，以及退出后音乐/阅读可重新挂载。确认 GPIO16 电平、USB 枚举与文件读写后才可标记硬件验证通过；主机图片不能证明硬件切换或屏幕实际刷新效果。
