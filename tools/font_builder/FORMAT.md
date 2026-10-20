# NBFT v1 点阵字库二进制格式

所有整数为 **Little Endian**。按字段解码，不要假设 C/C++ 结构体自动满足打包、对齐或大小端要求。所有文件偏移和长度以 byte 为单位。

## 64-byte 文件头

| 偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | char[4] | magic | ASCII `NBFT` |
| 4 | uint16 | version | 1 |
| 6 | uint16 | header_size | 64 |
| 8 | uint16 | record_size | 24 |
| 10 | uint16 | pixel_size | 像素字号，6–128 |
| 12 | int16 | ascent | 正数，行顶部到基线 |
| 14 | int16 | descent | 非负，基线到行底部 |
| 16 | int16 | line_gap | 非负，额外行间距 |
| 18 | uint16 | flags | 1：逐行、行首 MSB、1 bit 点阵；其他位保留为 0 |
| 20 | uint32 | glyph_count | 1–1112064（Unicode scalar 数量上限） |
| 24 | uint32 | index_offset | 64 |
| 28 | uint32 | bitmap_offset | `64 + glyph_count * 24` |
| 32 | uint32 | file_size | 文件完整长度 |
| 36 | uint32 | payload_crc32 | 从 offset 64 到文件末尾，CRC-32/ISO-HDLC（与 Python `zlib.crc32` 一致） |
| 40 | uint8[24] | reserved | 全 0 |

行高为 `ascent + descent + line_gap`。CRC32 覆盖索引和位图；头部需逐项验证。清单中的 SHA256 覆盖整个文件，可检测头部改动，但不提供签名认证。

所有文件偏移均为 uint32，单个文件不超过 4,294,967,295 bytes。v1 布局没有变化；移除了旧版原型读取器人为设置的 65536 字符条数限制，读取新大字库需使用更新后的工具。旧 BIN 仍可读取。

## 24-byte 字符索引记录

按 Unicode code point 升序，不能重复。支持 Unicode scalar（包括 U+FFFF 以上）；不是 UTF-8 字节或 UTF-16 单码元。

| 记录内偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | uint32 | codepoint | Unicode scalar |
| 4 | uint32 | bitmap_relative_offset | 相对整个 bitmap 区起点的偏移 |
| 8 | uint16 | width | 裁剪后的墨迹宽度 |
| 10 | uint16 | height | 裁剪后的墨迹高度 |
| 12 | int16 | x_offset | 墨迹左侧相对当前笔位置，可负 |
| 14 | int16 | y_offset | 墨迹顶部相对基线；向下为正，通常为负 |
| 16 | int16 | advance | 下一个字符笔位置的水平进位，非负整数 |
| 18 | uint16 | flags | v1 固定 0，未压缩 |
| 20 | uint32 | bitmap_length | `ceil(width / 8) * height` |

索引的位图偏移按记录顺序连续，不留间隙。无墨迹字符（例如空格）宽、高、长度均为 0，保留 `advance`。v1 原型限制单字宽、高不超过 1024。

## 点阵

每行 `stride = (width + 7) / 8`，整除取整，行之间不共享字节。像素 `(x,y)` 位于：

```text
byte_offset = bitmap_offset + bitmap_relative_offset + y * stride + x / 8
mask        = 0x80 >> (x % 8)
ink         = (byte & mask) != 0
```

1 表示文字墨迹，0 表示透明背景，行末未使用位必须为 0。绘制位置：

```text
draw_x = pen_x + x_offset
draw_y = baseline_y + y_offset
pen_x += advance
next_baseline_y += ascent + descent + line_gap
```

因此不能假定每个字都是 `字号×字号` 或固定宽度。必须依据字形边界进行裁剪，避免负偏移或下降部越界。

## SD 读取流程

1. 打开 BIN，检查文件头与文件长度；安装或首次使用时流式校验 CRC32 / 清单 SHA256。
2. 在排序索引中二分查找 codepoint；记录位置为 `64 + index * 24`。
3. 根据位图偏移读取单字，解码后画入帧缓冲。缓存常用字形或索引块，减少 SD 随机访问。
4. 查询不到字符时使用设备端备用字形；不把别的字体文件偏移误用为当前字库索引。

`font_bin.py` 的 `FileBitmapFont` 演示了电脑端按文件二分查询，不在内存中建立完整索引；完整 CRC 使用 4096-byte 分块读取，只在打开时执行一次。它处理已由导出工具完整验证的文件，对实际查询到的记录额外检查范围；不替代整体索引排序和连续性检查。

固件 `SdBitmapFont` 已实现此格式的按需读取、分步 CRC32、固定索引块及字形缓存，`SdBitmapText` 使用四字节 UTF-8 解码和 uint32 码点。测试页读取 `/Font/font-14.bin`、`font-20.bin`、`font-24.bin`；每次打开校验文件头、长度和完整 payload CRC，查询时检查所读记录与位图范围，仍依赖导出工具保证整个索引的排序和连续性。挂载、退出和硬件验证边界见 [SD 点阵字库](../../docs/SD_BITMAP_FONTS.md)。
