# 固定 UI 常用字包 ui-common-v2

`common3500.txt` 来自用户指定的
[elephantnose/characters](https://github.com/elephantnose/characters/blob/master/3500%E5%B8%B8%E7%94%A8%E6%B1%89%E5%AD%97.txt)。
下载日期 2026-11-16，原始 UTF-8 文件 SHA-256：
`21e94a29b6c63de1e0635d5757ffa2d3097dc653261ea3b0183baeb451bad48c`。
此副本只保留汉字并按 Unicode 升序去重，共 3500 字，不包含字体点阵。

`ui-common-v2-extra.txt` 保存当前固定界面需要的 114 个非汉字码点：
ASCII、标点、箭头（含 `›`、`←`）、全角空格和占位符。与 3500 汉字并集为
**3614 个码点**；当前字体提供 3613 个字形，U+FFFD 使用缺字占位行为。

`ui-common-v1-extra.txt` 是已发布的 111 码点版本，保留原样不再修改：
旧 SD 卡和旧固件仍可能携带按它生成的子集。固件对旧包保留补齐与回退路径，
因此 v1 包在 14 号由完整字库补齐、20/24 号回退完整字体；v2 包覆盖全部
当前固定 UI 码点，14/20/24 三个字号都直接使用常驻子集，不再回退。

UI 源码扫描只更新固件的需求清单 `UiFontCodepoints.h`，不改变已发布的包。
新增界面字符且需要继续零回退时，按同样方式发布下一个版本清单（v3），
不要把已发布的 v2 清单改成“当前固件需要的全部码点”。

动态歌词、歌曲名、阅读和更新说明维持完整字库。

服务器统一使用 `/opt/openbread-ota/data/ota/openbread-normal-one/Font/`，更新后清理 CDN 缓存并核对固件清单哈希。
旧固件仍依赖其发布时的包哈希与字数；替换 `font-*.ui.bin` 后旧固件无法下载新子集，需先升级固件。
