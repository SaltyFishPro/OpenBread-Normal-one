# 设备管理网页

`index.html` 是设备实际提供的网页源文件。修改后运行
`python tools/device_admin/build.py` 生成
`src/services/DeviceAdminPageHtml.h`，随后执行完整 `pio run`。

## 页面结构

四个分页固定在底部：概览、文件、设置、更新。登录使用设备屏幕上的六位验证码，
令牌保存在 `sessionStorage`，之后的接口请求都带 `X-Admin-Token`。

文件页是仿文件管理器的形态，但只使用固件已实现的接口：

| 界面元素 | 接口 |
| --- | --- |
| 存储环、已用/可用 | `/api/overview` 的 `total` / `used` |
| 列表、网格、排序、类型筛选、名称筛选 | `/api/files`，排序与筛选全部在浏览器完成 |
| 进入文件夹、返回上级 | 重新请求 `/api/files` 并换 `path` |
| 下载单个文件 | `/api/download`，浏览器另存为 |
| 重命名 | `/api/file-action` `op=rename` |
| 删除（含文件夹） | `/api/file-action` `op=delete` |
| 新建文件夹 | `/api/file-action` `op=mkdir` |
| 上传与发送进度 | `/api/upload?path=` + `XMLHttpRequest` 进度事件 |

`/api/files` 最多返回 100 项，页面在列表底部提示“仅显示前 100 项”，不做静默截断。
类型筛选里的数量是**当前文件夹**的统计，不会扫描整张卡。

## 设备名称

「设置 → 设备名称」写入 `DeviceSettings`，设备「关于设备」页面直接显示这个名称，
不再使用写死的默认名；热点与蓝牙名称不受影响。

名称上限 32 字节（一个汉字占 3 字节），页面按 UTF-8 字节校验，超出时直接提示、不发请求。
名称含非 ASCII 字符时，「关于设备」页会打开完整 20 号字库渲染；纯英文名称继续使用常驻
字库，不产生 SD 读取。超宽名称按字符边界截断并补省略号，不会越出名称格。

## 交互约定

- 点击文件夹进入，点击文件打开操作弹层；长按或右键进入多选。
- 多选时下载只对单个文件开放，重命名只对单个项目开放，删除支持多选。
- 所有二次确认都是页面内的弹层，默认动作是取消；不使用 `prompt()` / `confirm()`，
  避免安卓 WebView 与热点登录页屏蔽原生对话框。
- 上传期间锁定分页切换和文件操作，弹层提供取消；取消会中断 XHR，
  设备侧收到中断后删除临时文件。
- 发送 100% 只代表数据已写给设备，必须等设备响应成功才提示上传完成。
- 文件操作全部由服务端再次校验路径与重名，前端的名称检查只是提前提示。

## 选择器与脚本约束

`upload_test.cjs` 会检查以下约束，改动页面后必须仍然通过：

- `<input id="file" accept="*/*">` 且全页没有 `capture=`，保证可以选择任意文件。
- 只有一个 `<script>` 块；`function transfer(` 之后紧跟 `async function upload()`，
  测试脚本按这两个锚点截取上传逻辑。
- 不出现 `prompt(` / `confirm(`；文件操作只使用 `mkdir` / `rename` / `delete`。
- 上传目标必须 `encodeURIComponent`，中文文件名不能写坏查询串。

安卓系统选择器与热点登录 WebView 的能力由系统决定，网页无法强制打开 DocumentsUI。
如果登录窗口仅提供拍摄或照片，保持连接设备热点，在 Chrome 或系统完整浏览器中访问
`http://192.168.4.1` 再上传。

## 本地验证

```powershell
node tools/device_admin/upload_test.cjs   # 脚本语法、接口约束、上传进度语义
node tools/device_admin/ui_check.cjs      # 真实浏览器点完整条流程（需要 Chrome 或 Edge）
```

`ui_check.cjs` 会自己启动 `mock_server.cjs`（按 `DeviceAdminService` 的语义实现同一套接口）
和一次无头浏览器，跑 21 项断言：登录失败与成功、概览、列表与网格、多选、
上传进度、新建/重命名/删除、空目录、设置回填、在线更新状态、令牌失效回退，
并检查控制台没有脚本异常。截图写到 `tmp/device_admin_ui/shots/`。

`mock_server.cjs` 只是离线联调工具，不参与固件构建；改动接口时需要同步它，
否则离线验证会与设备行为不一致。`MOCK_PORT` / `DEVTOOLS_PORT` / `CHROME` 可用环境变量覆盖。

## 体积

页面内嵌在固件的 `PROGMEM` 中，约 57 KB，占 7 MB 应用分区不到 1%，
页面通过热点一次性推送，加载时会从 flash 直接写出，不占运行内存。
