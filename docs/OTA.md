# OpenBread OTA Setup Guide

## Overview

本文档总结了 OpenBread 项目当前 OTA 服务端准备流程，目标是：

- 使用 GitHub 发布固件
- 使用 1Panel + Docker + Cloudflare 域名托管 OTA 清单与固件
- 让设备后续通过固定地址检查版本并执行 OTA 升级

当前已完成的是 OTA 服务端静态分发链路：

- 本地容器可访问 OTA 清单
- 域名可访问 OTA 清单

当前验证成功的地址：

- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

## Recommended OTA Architecture

推荐采用 `manifest + firmware.bin` 模式，而不是让设备直接访问 GitHub 页面。

建议的 OTA 地址结构：

- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`
- `https://ota.openbread.net/ota/openbread-normal-one/firmware-0.1.0.bin`

推荐的产品标识：

- `openbread-normal-one`

推荐的 OTA 清单格式：

```json
{
  "product": "openbread-normal-one",
  "version": "0.1.0",
  "build": 1,
  "bin_url": "https://ota.openbread.net/ota/openbread-normal-one/firmware-0.1.0.bin",
  "sha256": "replace_with_real_sha256",
  "size": 2500000,
  "force": false,
  "release_notes": "Initial OTA test release.",
  "published_at": "2026-04-01T12:00:00Z"
}
```

字段说明：

- `product`：设备产品标识，设备端必须匹配
- `version`：展示用版本号
- `build`：整数版本号，设备端优先用它判断新旧
- `bin_url`：固件下载地址
- `sha256`：固件哈希值
- `size`：固件大小
- `force`：是否强制升级
- `release_notes`：更新说明
- `published_at`：发布时间

## Why Use a Subdomain

最开始考虑的是：

- `https://openbread.net/ota/openbread-normal-one/manifest.json`

最终改为：

- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

原因：

- 根域名 `openbread.net` 已经承载论坛
- OTA 是机器接口，不建议和论坛共用同一站点规则
- 二级域名更利于缓存、日志、限流和后续迁移

## Step 1: Prepare OTA Directory Structure

服务器上建议的目录结构如下：

```txt
/opt/openbread-ota/
  data/
    ota/
      openbread-normal-one/
        manifest.json
        firmware-0.1.0.bin
  compose/
    docker-compose.yml
```

其中最关键的是：

- `manifest.json` 必须位于 `/opt/openbread-ota/data/ota/openbread-normal-one/manifest.json`

如果目录层级不对，即使 Nginx 正常运行，也会返回 `404 Not Found`。

## Step 2: Upload Initial Test Files

在 1Panel 文件管理中创建上述目录后，先上传两个测试文件：

- `manifest.json`
- 一个测试用的 `firmware-0.1.0.bin`

第一阶段不需要是真实固件，先保证静态文件分发链路可访问即可。

## Step 3: Deploy Static File Service with Docker

在 1Panel 中使用“编排”部署一个最简单的 Nginx 静态文件服务。

`/opt/openbread-ota/compose/docker-compose.yml`：

```yaml
services:
  ota-nginx:
    image: nginx:alpine
    container_name: ota-nginx
    restart: always
    ports:
      - "127.0.0.1:8088:80"
    volumes:
      - /opt/openbread-ota/data:/usr/share/nginx/html:ro
```

说明：

- 宿主机目录 `/opt/openbread-ota/data` 被挂载到容器内 `/usr/share/nginx/html`
- 因此 URL `/ota/openbread-normal-one/manifest.json` 对应的容器文件路径必须是：
  - `/usr/share/nginx/html/ota/openbread-normal-one/manifest.json`

## Step 4: Verify Local Container Access

容器启动后，本机先验证：

- `http://127.0.0.1:8088/ota/openbread-normal-one/manifest.json`

如果返回 `404`，优先检查：

- 文件是否真的存在于 `/opt/openbread-ota/data/ota/openbread-normal-one/manifest.json`
- `docker-compose.yml` 是否仍然挂载 `/opt/openbread-ota/data:/usr/share/nginx/html:ro`

排查过程中，Nginx 报出的关键日志如下：

```txt
open() "/usr/share/nginx/html/ota/openbread-normal-one/manifest.json" failed (2: No such file or directory)
```

这类日志说明不是 Nginx 启动失败，而是文件路径不存在。

## Step 5: Bind Domain in 1Panel Website

当本地访问成功后，再进入 1Panel 的“网站”模块绑定域名。

需要注意：

- 应使用“反向代理”网站类型
- 如果进入的是“运行环境 / PHP-FPM / 站点目录”页面，说明进错了创建类型

正确配置应类似：

- 主域名：`ota.openbread.net`
- 反向代理地址：`http://127.0.0.1:8088`

如果当前 1Panel 版本没有单独的反向代理向导，也可以：

- 先创建一个静态站点
- 再手动修改 OpenResty/Nginx 配置，把请求代理到 `127.0.0.1:8088`

## Step 6: Configure Cloudflare DNS

在 Cloudflare 中新增一条 DNS 记录：

- 类型：`A`
- 名称：`ota`
- 值：服务器公网 IP

最终让：

- `ota.openbread.net`

解析到你的服务器。

## Step 7: Configure HTTPS

在 1Panel 网站配置中，为 `ota.openbread.net` 配置 HTTPS 证书。

建议：

- OTA 清单和固件下载必须走 HTTPS
- 不建议 OTA 接口长期暴露在纯 HTTP 下

## Step 8: Verify Final Public Access

最终需要验证三个地址：

1. 本地容器访问：
   - `http://127.0.0.1:8088/ota/openbread-normal-one/manifest.json`
2. 域名 HTTP 访问：
   - `http://ota.openbread.net/ota/openbread-normal-one/manifest.json`
3. 域名 HTTPS 访问：
   - `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

当前项目已确认通过的是：

- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

## Current Result

当前已完成：

- 目录结构建立完成
- Docker 静态文件服务运行完成
- 1Panel 网站绑定完成
- `ota.openbread.net` 域名访问成功

这代表 OTA 服务端的第一阶段已经完成，后续设备端可以开始开发：

- 请求 `manifest.json`
- 解析版本信息
- 比较本地和远端版本

## Recommended Next Steps

后续建议按这个顺序推进：

1. 固定 OTA 协议
   - 不再频繁改 `manifest.json` 字段
2. 在项目中新增 `OtaService`
   - 先只做“检查更新”
3. 在设置页接入 OTA 检查界面
   - 显示当前版本、远端版本、是否可更新
4. 再实现真正的固件下载和 OTA 升级
5. 最后加入 GitHub Release 自动同步和缓存脚本

## Notes for Sharing

如果要向其他人分享这套流程，建议强调以下几点：

- 先让静态 OTA 地址跑通，再写 ESP32 OTA 代码
- 先做“版本检查”，不要一上来就做完整升级
- 路径错误比 Nginx 错误更常见
- 根域名已有业务时，优先使用二级域名承载 OTA
- Docker 能跑不代表文件路径正确，`404` 往往就是目录层级问题

## Summary

这次 OTA 服务端准备流程的核心经验是：

- OTA 的第一步不是设备端，而是稳定的服务端清单地址
- 使用 `ota.openbread.net` 独立承载 OTA 接口比复用根域名更稳
- 通过 1Panel + Docker + Cloudflare，可以低成本完成 OTA 静态分发链路
- 当 `manifest.json` 能稳定通过 HTTPS 访问后，设备端 OTA 才值得正式开始开发
