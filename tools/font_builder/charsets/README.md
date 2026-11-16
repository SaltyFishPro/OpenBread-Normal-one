# 固定 UI 常用字包 ui-common-v1

`common3500.txt` 来自用户指定的
[elephantnose/characters](https://github.com/elephantnose/characters/blob/master/3500%E5%B8%B8%E7%94%A8%E6%B1%89%E5%AD%97.txt)。
下载日期 2026-11-16，原始 UTF-8 文件 SHA-256：
`21e94a29b6c63de1e0635d5757ffa2d3097dc653261ea3b0183baeb451bad48c`。
此副本只保留汉字并按 Unicode 升序去重，共 3500 字，不包含字体点阵。

`ui-common-v1-extra.txt` 固定保存 V0.1.8 菜单所需但上述清单没有的 111 个码点：
ASCII、标点、箭头、占位符和少量汉字。两者并集为 3611 个码点；当前字体实际包含
3610 个字形，U+FFFD 使用缺字占位行为。

发布后不要修改 v1 清单。UI 源码扫描只更新固件的需求清单 `UiFontCodepoints.h`，
不改变已发布的 v1 包。14 号缓存填补缺字，20/24 号缺少所需字形时回退完整字体。
动态歌词、歌曲名、阅读和更新说明维持完整字库。

服务器统一使用 `/opt/openbread-ota/data/ota/openbread-normal-one/Font/`，更新后清理 CDN 缓存并核对固件清单哈希。
旧固件仍依赖其发布时的包哈希与字数；新固件放宽了子集兼容检查，不代表旧固件获得此能力。
