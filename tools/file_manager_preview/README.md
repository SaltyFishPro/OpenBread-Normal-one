# 文件管理页设计预览（ES 风格）

> 已落地：这套设计已实现在 `tools/device_admin/index.html`（真实接口、随固件发布）。
> 本目录只保留最初的设计稿与交互说明，不再参与任何构建，也不应作为实现依据。

本目录是**设计稿**，不是固件资源：不参与 `tools/device_admin/build.py`，不参与 `pio run`，
没有任何 `fetch` 指向真实设备，列表与容量都是页面内的演示数据。

真实页面仍然是 `tools/device_admin/index.html`。要不要采纳、以及怎么并进去，由所有者决定。

## 打开方式

直接用浏览器打开 `index.html`，或按状态打开：

| 地址 | 状态 |
| --- | --- |
| `index.html` | 列表视图 |
| `index.html?demo=select` | 多选模式（已选 2 项） |
| `index.html?demo=grid` | 网格视图 |
| `index.html?demo=upload` | 上传进度弹层 |
| `index.html?demo=empty` | 空文件夹 |

手机上看效果最接近实机（页面按 390 CSS 宽设计，640px 以上居中显示）。

## 交互与现有接口的对应关系

预览里所有操作都控制在**现有固件已具备的接口**内，因此不改固件即可落地：

| UI 元素 | 现有接口 |
| --- | --- |
| 存储环、已用/可用 | `/api/overview` 的 `total` / `used` |
| 列表与网格、排序、筛选 | `/api/files` 返回的 `name` / `dir` / `size`，排序与筛选在浏览器侧完成 |
| 打开文件夹、返回上级 | 重新请求 `/api/files` 换 `path` |
| 下载（单个或批量逐项） | `/api/download` |
| 重命名 | `/api/file-action` `op=rename` |
| 删除（含文件夹） | `/api/file-action` `op=delete` |
| 新建文件夹 | `/api/file-action` `op=mkdir` |
| 上传、上传进度 | `/api/upload` + `XMLHttpRequest` 进度事件 |
| 登录与超时关站 | 沿用 `/api/login`、`/api/close`、`adminTimeoutSeconds` |

## 要对齐 ES 还需要新增的接口

这些在预览里都是灰显或只弹提示，没有假装能用：

- 复制 / 移动：`/api/file-action` 增加 `op=copy`、`op=move`（需要目标目录校验与同名冲突策略）。
- 新建空文本文件：`op=touch`。
- 按类型聚合（分类入口显示真实数量）：需要一次全卡扫描或维护索引，代价与耗时都必须先评估。
- 图片/视频缩略图：`/api/thumb`，涉及 JPEG 解码与内存峰值，不建议第一版做。

## 体积提醒

预览稿未压缩，`index.html` 约 36.4 KB；现有 `tools/device_admin/index.html` 约 15.4 KB。
页面整包通过热点一次性推送，体积增长只影响首屏传输时间（约几十毫秒级）与固件 flash（7 MB 分区内可忽略）。
如果在意体积，可以只保留「列表 + 多选 + 上传」，去掉分类入口和网格视图，回到 20 KB 左右。

## 不改代码的说明

新增文件只有本目录的 `index.html` 与 `README.md`，没有改动 `src/`、`platformio.ini`、
分区表或 `src/services/DeviceAdminPageHtml.h`，因此没有执行 `pio run`，固件行为与资源占用不变。
