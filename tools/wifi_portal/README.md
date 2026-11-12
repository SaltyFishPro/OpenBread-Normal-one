# WiFi 配网网页源文件

`index.html` 是设备端配网页的单一源文件，`build.py` 将它嵌入
`src/services/WifiPortalPage.cpp` 的 PROGMEM 原始字符串，供固件通过
`WebServer::send_P` 从 Flash 输出。

- 网页是移动端优先的纯静态单页，含内联 CSS 与 JS。
- 通过 `/status` 轮询状态（`state/error/portal/ap/target/ip/remaining/hold`），
  通过 `/connect` 提交表单（JSON 响应，400 表示输入无效、409 表示忙碌或热点未开启）。
- 网页内部状态：`portal / connecting / connected / failed / timeout / offline`；
  连接成功且热点关闭后停止轮询。

修改 `index.html` 后在仓库根目录运行：

```powershell
python tools/wifi_portal/build.py
```

再执行固件编译。网页文案不受设备固定中文字库限制，只有设备页文案需检查码点。
