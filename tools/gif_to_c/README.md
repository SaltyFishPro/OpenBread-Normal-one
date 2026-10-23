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
- `--rle` encode run/value pairs; automatically pack variable length frames when this saves Flash
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

The header preserves dimensions, frame interval and explicit frame count.
Always use `XXX_FRAME_COUNT` for animation length. For packed RLE, `xxx_frames`
is a single storage blob (`[1][XXX_FRAME_STORAGE_BYTES]`); `sizeof` measures
storage, not the number of animation frames. Existing `IconBitmap::Anim`
initializers using `&xxx_frames[0][0]` remain compatible.
Re-export replaces the matching generated pair; an existing unrelated `.cpp` is
not overwritten. Single-file export requires `--header-only` and must not leave an
old companion `.cpp` in the same location. A hand-written host preview build must
also compile the asset `.cpp` when it uses the shared header.

Current shared resources:

- `src/ui/assets/main_menu/icons8-clock.h` / `.cpp` — main menu and focus clock menu.
- `src/ui/assets/games/icons8-tapthewoodenfish.h` / `.cpp` — games menu and wooden fish page.

Single-use resources may retain their existing header definitions.

### RLE storage format

- Raw bitmaps and fixed stride RLE remain supported.
- `XXX_FRAME_BYTES == 0x8000U` selects packed RLE (the RLE flag with zero stride).
  The blob starts with `FRAME_COUNT + 1` little-endian `uint16` offsets, followed
  by all RLE frames without padding. Offsets are relative to the payload after
  the table; adjacent offsets give the start and end of each frame.
- The exporter uses packed RLE only if payload plus table is smaller than fixed
  stride storage and the payload fits 65,535 bytes. Otherwise it uses fixed
  stride RLE; each frame must still fit the decoder's 32,767-byte stride limit.
- Table and data remain in `PROGMEM`; lookup reads two offsets without allocating
  RAM or changing the existing icon cache. `Anim` fields and page callers are
  unchanged. This format requires the updated `IconBitmap` decoder.
- `--header-only` changes file organization only; it also supports packed RLE.

The 11 menu animations were migrated by retaining their original RLE run/value
pairs, dimensions, frame order and delay. No source GIF was reprocessed and no
frames were removed. Single-frame resources remain in the existing format.

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
Checks raw/fixed/packed RLE pixels, offset limits and fallback, storage dimensions
and timing metadata; links two independent consumers and compares their array
addresses; exercises GIF/image export, re-export, legacy output and protection
of unrelated sources. `decoder_verify.cpp` also compiles the production decoder
and ST7305 driver, comparing real framebuffers for cache and large-image fallback,
scaling, inversion, clipping, loop boundaries and empty descriptors.
Generated checks stay in `tmp/animation-export-test/`.

## 8) Large RLE drawing verification

```powershell
python tools/gif_to_c/large_decoder_verify.py
```

Requires `uv` and Python; compiles the production `IconBitmap.cpp` and ST7305
driver with the existing Arduino stubs. Covers the current 16 large RLE resources
(15 submenu images and the home background), plus early termination, truncated
input and two synthetic packed frames. Compares 231 real framebuffers against
independently expanded raw bitmaps, including row-spanning runs, scaling with
repeated/skipped source rows, inversion, clipping, transitions and loop wrapping.

`read_counter.h` is injected only into this host build; it counts encoded byte
reads and checks that each draw reads at most one stream prefix (plus four offset
bytes for packed RLE). Fully vertically clipped fixed RLE reads zero bytes. No counting or
logging code enters the firmware. Output buffers and selected PGM images go to
`tmp/large-rle-test/`.

To compare an older decoder, copy its source before editing, then run:

```powershell
python tools/gif_to_c/large_decoder_verify.py --decoder tmp/IconBitmap-before.cpp --output tmp/large-rle-before --allow-rescan
```

Use the same test sources and assets for both versions and compare output `.bin`
files byte for byte. Read counts measure decoding work; device refresh speed
still requires physical verification.
