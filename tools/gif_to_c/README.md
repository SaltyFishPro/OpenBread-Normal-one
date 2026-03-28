# GIF to C Array Tool

Convert animated GIF to 1-bit C arrays compatible with this project (`PROGMEM` style).

## 1) Install

```bash
pip install -r tools/gif_to_c/requirements.txt
```

## 2) Basic Usage

```bash
python tools/gif_to_c/gif_to_c.py input.gif -o out_icon.h -n wifi
```

This generates:
- `WIFI_FRAME_WIDTH`
- `WIFI_FRAME_HEIGHT`
- `WIFI_FRAME_BYTES`
- `wifi_frames`
- `WIFI_FRAME_COUNT`

## 3) Typical project command (32x32, remove duplicate frames)

```bash
python tools/gif_to_c/gif_to_c.py your.gif -o src/generated_wifi.h -n wifi -W 32 -H 32 --dedupe
```

## 4) Options

- `-n, --name` base symbol name (required)
- `-W, --width` target width (default `32`)
- `-H, --height` target height (default `32`)
- `-t, --threshold` binarization threshold 0-255 (default `128`)
- `--invert` invert black/white before packing
- `--resize nearest|bilinear|bicubic|lanczos` (default `nearest`)
- `--dedupe` drop consecutive identical frames
- `--bg` background gray for transparent pixels (default `255`)

## 5) Integration

1. Generate header:
   ```bash
   python tools/gif_to_c/gif_to_c.py in.gif -o src/my_icon.h -n my_icon -W 32 -H 32 --dedupe
   ```
2. Include it from `src/icons.h` or paste generated block directly.
3. Reference in `main.cpp` like existing `*_frames`.

## 6) GUI 模式（推荐）

```bash
python tools/gif_to_c/gif_to_c_gui.py
```

GUI 功能：
- 选择输入 GIF / 输出文件
- 设置宽高、阈值、背景、缩放算法
- 勾选反色 / 连续帧去重
- 预览首帧效果
- 一键转换导出
