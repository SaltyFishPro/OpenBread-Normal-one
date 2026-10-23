# GIF to C Array Tool

Convert animated GIF to 1-bit C arrays compatible with this project (`PROGMEM` style).
By default, export a matching `.h`/`.cpp` pair: the header declares the array and
the source defines it once, so multiple pages share the same Flash data.

## 1) Install

```bash
pip install -r tools/gif_to_c/requirements.txt
```

## 2) Basic Usage

```bash
python tools/gif_to_c/gif_to_c.py input.gif -o out_icon.h -n wifi
```

This generates `out_icon.h` and `out_icon.cpp`, exposing:
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
- `--rle` encode run/value pairs compatible with the project's `IconBitmap` decoder
- `--header-only` use legacy single-file output; avoid for assets referenced by multiple `.cpp` files
- `--frame-step` keep every Nth GIF frame to reduce flash usage
- `--frame-delay` set the generated animation frame interval in milliseconds

Main menu icons use 100x100 frames (1300 decoded bytes, within the icon cache):

```powershell
python tools/gif_to_c/gif_to_c.py 'F:/下载/system-outline-1327-api-morph-select.gif' -o src/ui/assets/main_menu/api_stats.h -n api_stats -W 100 -H 100 --resize lanczos --dedupe --rle
```

The generated animation uses a 42ms frame interval by default; use `--frame-delay`
when frame sampling needs to preserve a longer animation cycle. GIF per-frame
delays are not preserved automatically.

## 5) Integration

1. Generate a header and matching source:
   ```bash
   python tools/gif_to_c/gif_to_c.py in.gif -o src/my_icon.h -n my_icon -W 32 -H 32 --dedupe
   ```
2. Keep both files under `src/`; PlatformIO compiles the `.cpp` automatically.
3. Include only the `.h` from pages and reference `*_frames` like existing assets.
   Do not include the `.cpp` or copy the pixel array into each page.

The header preserves dimensions, frame interval, frame count and complete array
dimensions, so `sizeof` and the existing `IconBitmap::Anim` initializers still work.
Re-export replaces the matching generated pair; an existing unrelated `.cpp` is
not overwritten. Single-file export requires `--header-only` and must not leave an
old companion `.cpp` in the same location. A hand-written host preview build must
also compile the asset `.cpp` when it uses the shared header.

Current shared resources:

- `src/ui/assets/main_menu/icons8-clock.h` / `.cpp` — main menu and focus clock menu.
- `src/ui/assets/games/icons8-tapthewoodenfish.h` / `.cpp` — games menu and wooden fish page.

Single-use resources may retain their existing header definitions; this change
does not re-encode or sample any existing animation frames.

## 6) GUI 模式（推荐）

```bash
python tools/gif_to_c/gif_to_c_gui.py
```

GUI 功能：
- 选择输入 GIF / 输出头文件，自动同时生成同名 `.cpp`（两个文件请一起保留）
- 设置宽高、阈值、背景、缩放算法
- 勾选反色 / 连续帧去重
- 预览首帧效果
- 一键转换导出

## 7) Export verification

```powershell
python tools/gif_to_c/verify.py
```

Requires Pillow and `uv` (the script uses Zig 0.13.0 as a host C++ compiler).
Checks raw/RLE pixel equivalence, complete array dimensions and timing metadata,
links two independent consumers and compares their array addresses, and exercises
GIF/image export, re-export, legacy output and protection of unrelated sources.
Generated checks stay in `tmp/animation-export-test/`.
