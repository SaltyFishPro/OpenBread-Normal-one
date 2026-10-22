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

固件 `SdBitmapFont` 已实现此格式的按需读取、分步 CRC32、固定索引块及字形缓存，`SdBitmapText` 使用四字节 UTF-8 解码和 uint32 码点。测试页读取 `/Font/font-14.bin`、`font-20.bin`、`font-24.bin`；每次打开检查文件头与长度，首次使用完整校验 payload CRC，同次会话内可复用完整文件头匹配的校验结果。查询时检查所读记录与位图范围，仍依赖导出工具保证整个索引的排序和连续性。调用方在卸载、换卡或写入前须 `end()` 清除会话记录；挂载、退出和硬件验证边界见 [SD 点阵字库](../../docs/SD_BITMAP_FONTS.md)。

## NBCI v1 常用字索引

可选伴随文件名为 `font-字号.common.bin`，不改变 NBFT v1 字库。所有整数小端；位置指向 NBFT 的 **24-byte 字形记录**，不直接指向点阵。电脑端实现与验证见 [font_common_index.py](font_common_index.py)。当前固件在「SD字库测试」中读取此文件，最多接收 7500 条；其他页面尚未迁移。

### 96-byte 文件头

| 偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | char[4] | magic | ASCII `NBCI` |
| 4 | uint16 | version | 1 |
| 6 | uint16 | header_size | 96 |
| 8 | uint16 | record_size | 8 |
| 10 | uint16 | pixel_size | 必须与绑定字库一致 |
| 12 | uint32 | entry_count | 成功收录的常用字符数，至少 1 |
| 16 | uint32 | file_size | `96 + entry_count * 8` |
| 20 | uint32 | payload_crc32 | 从 offset 96 到末尾的 CRC-32/ISO-HDLC |
| 24 | uint8[8] | reserved | 全 0 |
| 32 | uint8[64] | font_header | 绑定 NBFT 的完整文件头，须逐字节匹配 |

NBFT 文件头内含字号、文件长度、全部字形数、度量和 payload CRC32，可拒绝混用不同导出版本的索引。NBCI payload CRC 保护条目；源 NBFT 的完整校验仍须独立完成，不能用索引校验代替。导出清单同时记录字库和索引的 SHA256。

### 8-byte 常用字记录

| 记录内偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | uint32 | codepoint | Unicode scalar，升序、去重，支持 U+FFFF 以上 |
| 4 | uint32 | record_offset | NBFT 文件绝对位置：`64 + glyph_index * 24` |

逐条检查偏移落在 NBFT 索引区并按 24-byte 对齐；偏移随码点严格递增，且目标记录的码点与 NBCI 码点相同。电脑端导出时核对每个目标记录；设备加载时检查顺序与范围，在实际查询时核对目标码点，避免启动时再次随机读遍全部常用字。目标不符即停用可选索引并回退完整字库。未在本次字库中收录的 TXT 字符写入 `common-missing.txt`，不生成伪造位置。`common-codepoints.txt` 是便于人工核对的 UTF-8 报告，不供运行时搜索。

7500 条记录正文占 60,000 bytes，加头共 60,096 bytes。设备仅将当前字号的记录加载到外部 RAM，在内存中查码点，命中后通过现有索引块缓存定位 NBFT 记录及点阵，未命中再查完整字库。总预算 128KiB，当前固定分配 125,792 bytes（含常用字正文区、256 字形槽和 64 索引块），不另申请整份字库。NBCI 分步加载，每 tick 最多 32KiB，4ms 后让出；数据完整且 CRC 通过后才启用。索引缺失、错误、超过 7500 条或外部 RAM 不足时保留普通查找。切换字号重新读取和校验 NBCI，复用同一缓存分配；退出/重试关闭文件并释放缓存。
