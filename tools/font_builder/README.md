# SD 点阵字库工具

电脑端导入 TTF / OTF / TTC / OTC 字体，默认生成字体包含的全部可独立绘制的 Unicode 字符，也可以输入自定义字符集，一次生成最多三个像素字号的单色 BIN 字库。工具位于 `tools/font_builder/`，可独立使用。固件已添加按需读取器与「SD字库测试」页面，接入说明见 [SD 点阵字库](../../docs/SD_BITMAP_FONTS.md)。

“字体全部字符”以选中字体（TTC / OTC 的当前集合索引）的 Unicode cmap 实际覆盖为准，包含基本区、扩展区和私用区中的有效映射，忽略控制符、格式字符和代理码点。不会为没有字形的 Unicode 编号生成假字形，也不会将字体集合里的所有字体自动合并。变体选择序列、连字及无独立 Unicode 映射的字形不在此单字格式范围内。

## 启动窗口

需要 Python 3.10+（包含 Tkinter，推荐 python.org Windows 安装包）。在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/font_builder/run.ps1
```

首次启动会在本工具目录创建独立 `.venv` 并安装 Pillow、fontTools。不会使用或修改 PlatformIO 的 Python 环境。

也可以手动启动：

```powershell
python -m venv tools/font_builder/.venv
& tools/font_builder/.venv/Scripts/python.exe -m pip install -r tools/font_builder/requirements.txt
& tools/font_builder/.venv/Scripts/python.exe tools/font_builder/font_builder_gui.py
```

1. 浏览导入字体。TTC / OTC 的“集合索引”从 0 开始，用于选择集合内的字体。
2. 输入字号，例如 `16,20,24`，最多三个，范围 6–128 px。
3. 默认选择“字体全部字符”，不需要粘贴字符集；生成完成会显示字符总数与 U+FFFF 以上字符数量。
4. 如需裁剪，选择“自定义字符集”，粘贴文字或导入 UTF-8 TXT。工具去重、按 Unicode 排序；忽略换行、制表符、控制符和格式字符，不做字符归一化。此模式可额外加入可打印 ASCII（U+0020–U+007E）。
5. 点击“生成预览”，在各字号标签中查看 384×168 原始像素或 2 倍最近邻放大结果。此操作只使用临时目录。
6. 确认后点击“导出 BIN 与预览”。默认输出到本工具的 `output/`；存在同名文件时窗口会询问覆盖。

“生成预览”“导出 BIN 与预览”和“打开输出目录”位于固定的底部操作栏。缩小窗口时字符输入区和预览区随剩余空间伸缩；高缩放下左侧设置可通过滚动条访问。生成进度保持单行，避免长路径挤出按钮。更新工具后请关闭旧窗口并重新运行启动命令。

默认阈值 128。降低阈值可保留更多边缘像素、使笔画更粗；提高则更细。预览文字独立于导出字符集，不会自动增加进字库；预览中没有的字显示空框，并在清单中列出。

自定义模式下，字体不包含请求字符时默认终止，避免把缺字方框误当成有效字形。勾选“跳过缺字”后允许导出，其明细写入 `missing.txt`。全部模式直接使用字体的有效映射，因此没有“补充 ASCII”或“跳过缺字”选项。全角空格、普通空格等有进位但无墨迹的字符会正确保留。

## 输出

```text
output/
  font-16.bin
  font-20.bin
  font-24.bin
  manifest.json
  charset.txt
  missing.txt
  preview-16.png
  preview-20.png
  preview-24.png
  preview.png
```

- BIN：包含独立文件头、排序索引、基线度量和 1 bit 点阵；每份均可单独使用。
- `manifest.json`：源字体名称及 SHA256、集合索引、生成模式、基本区/扩展区字符数、阈值、字号、行高、最大单字点阵字节数、文件大小和各 BIN SHA256。
- `charset.txt`：实际成功生成的去重字符集。
- `missing.txt`：被显式跳过的缺字（为空表示没有）。
- `preview-*.png`：**重新打开导出的 BIN、解码位图后**绘制的 384×168 单色画布；不依赖原字体重新绘图。
- `preview.png`：三个字号的对比图，画布使用 2 倍最近邻放大，标题列出文件大小和行高。

上面以 `16,20,24` 为导出示例。当前设备测试页使用 **`14,20,24`**，请生成对应字号，将 `font-14.bin`、`font-20.bin`、`font-24.bin` 放到 SD 卡根目录的 **`Font/`**。进入「设置 → 设备自检 → SD字库测试」预览；清单可一同保存，但当前设备读取器不解析清单。其他页面仍使用内置字体，复制文件不会自动替换全机字体。

导出会替换同名字库、预览及清单，其他文件保留。重新导出不同字号后，旧字号文件可能还在目录中，**以本次 `manifest.json` 的 `fonts` 列表为准**。建议每个字体使用单独输出目录；输入字体与输出目录必须分开。生成时先在临时子目录验证，再逐文件替换，清单最后发布；整个目录的替换不保证掉电原子性，建议先在本地生成，再复制到 SD 卡。

`output/`、`.venv/` 和项目 `tmp/` 不纳入 Git。工具不附带第三方字体文件，生成字库的使用和分发遵循源字体许可证。

## 命令行

生成字体全部字符、三个字号（路径包含空格时加引号）：

```powershell
& tools/font_builder/.venv/Scripts/python.exe -X utf8 tools/font_builder/font_builder.py build `
  --font C:/Windows/Fonts/msyh.ttc --font-index 0 --all-glyphs `
  --sizes 16,20,24 --output tmp/font-builder-all
```

`--all-glyphs` 不能与 `--charset` / `--charset-file` 混用。自定义裁剪示例：

```powershell
& tools/font_builder/.venv/Scripts/python.exe tools/font_builder/font_builder.py build `
  --font C:/Windows/Fonts/msyh.ttc --font-index 0 `
  --charset-file tools/font_builder/example_charset.txt `
  --sizes 16,20,24 --include-ascii `
  --output tmp/font-builder-demo
```

也可使用 `--charset "这里输入字符ABC123"`，或与 `--charset-file` 合并。`--skip-missing` 显式跳过缺字，`--threshold 128` 调整笔画，`--preview-text "电脑状态看板 12:30"` 指定预览文字。CLI 覆盖同名输出，不额外询问。

校验 BIN 并脱离原字体生成预览：

```powershell
& tools/font_builder/.venv/Scripts/python.exe tools/font_builder/font_builder.py inspect `
  tmp/font-builder-demo/font-20.bin --text "电脑状态看板 12:30" `
  --output tmp/font-builder-demo/readback.png
```

二进制定义见 [FORMAT.md](FORMAT.md)，标准库独立读取器为 [font_bin.py](font_bin.py)。

## ESP32 读取大字库的方式

字库大小不决定设备必须占用多少 RAM。NBFT 的字符索引已排序，当前固件使用 Unicode **32-bit** 码点二分查找，读取所需字形；不能用 `uint16_t` 保存扩展区码点，也不能把 UTF-8 字节直接当字符编号。

- 10 万字符的字库，一次查询最多约 17 次索引记录读取，再读取一个字形；不需要扫描整份文件。
- 24×24 单色字形的纯点阵是 72 bytes，此外有字形度量与 SD 驱动缓冲等开销；实际字形尺寸不一定等于字号，应依据清单中的 `max_bitmap_bytes` 规划缓冲。
- 大字库的主要问题是 SD 随机访问延迟。固件使用固定容量的索引块和字形缓存，不整体加载字库或字符索引。具体刷新速度需实机测量。
- 完整 CRC 校验会顺序读取文件头之后的全部数据。当前固件每次打开、切换字号或重试时分步校验，校验完成后才显示字形；绘制单字时不重复校验整份文件。

工具附带 `FileBitmapFont` 作为**电脑端按需读取参考**：打开时用 4096-byte 分块流式计算 CRC，查字时只读取索引记录与单个点阵，不整体加载字库或字符索引。它不是 ESP32 固件实现，也不替代导出时的完整结构验证。

```powershell
& tools/font_builder/.venv/Scripts/python.exe -X utf8 tools/font_builder/font_builder.py inspect `
  tmp/font-builder-all/font-24.bin --seek --text "电脑状态看板 12:30" `
  --output tmp/font-builder-all/seek-preview.png
```

## 当前取舍与验证

本机使用微软雅黑 `msyh.ttc`、阈值 128，生成 **6763 个 GB2312 汉字 + 95 个可打印 ASCII**（不包含额外中文标点）的实测体积如下。字体或字符集变化后体积也会变化。

| 字号 | BIN 大小 |
| --- | --- |
| 16 px | 358,786 bytes |
| 20 px | 534,687 bytes |
| 24 px | 613,164 bytes |
| 合计 | 1,506,637 bytes，约 1.44 MiB |

同一字体使用“全部字符”模式，实际导出 **29903 个字符（其中 U+FFFF 以上 196 个）**，排除 2 个控制/格式映射：

| 字号 | BIN 大小 | 最大单字纯点阵 |
| --- | --- | --- |
| 16 px | 1,580,937 bytes | 51 bytes |
| 20 px | 2,275,842 bytes | 63 bytes |
| 24 px | 2,686,650 bytes | 100 bytes |
| 合计 | 6,543,429 bytes，约 6.24 MiB | 不需要同时加载三份字库 |

这些是本机字体文件的实测数据，不代表所有 Unicode 字符都已覆盖。

- 首版不压缩点阵，使用按行打包的 1 bit 数据，方便通过 SD 卡定位读取。索引每字符 24 bytes，因此小字号或超大字符集的索引开销也需计算。
- 已去掉每份字库 65536 字符的原型限制，索引条数使用 uint32；可覆盖所有 Unicode scalar 值，单文件偏移仍受 NBFT v1 的 4 GiB 上限约束。
- 字号是字体的像素 em 大小，墨迹高度和总行高取决于字体。保留逐字宽度、基线偏移、上升部、下降部；重音及高字形会扩展行框。
- 面向中文、英文、数字和符号；逐 Unicode 字符处理，不做字距配对、连字、阿拉伯文塑形、双向排版或彩色 Emoji。组合字符可保存独立字形，但不是复杂文本排版引擎。
- 24-bit / RGB / 灰度和彩色字形不会被直接保存，最终均为白底黑字预览和单色点阵。不采用抖动，避免小字产生散点。
- 设备读取器及测试页已完成完整编译和主机帧缓冲核对，实机读取速度、按键响应和热拔卡表现仍待验证。现阶段保留内置字体，固件体积尚未因此减少；全局字体迁移需另行检查布局。

有针对性的格式回归（字形还原、跨字节补位、空格、基线、缺字、确定性和损坏输入）：

```powershell
& tools/font_builder/.venv/Scripts/python.exe tools/font_builder/verify.py `
  --font C:/Windows/Fonts/msyh.ttc
```

默认还验证 65537 条扩展区索引和按需读取。附加 `--all-glyphs` 会以所选字体生成全部字符、三个字号，比较按需读取与整体读取结果；附加 `--gui` 可自动检查窗口构建、模式切换、异步预览/导出和三个字号标签页（需可用 Tk 显示环境）。
