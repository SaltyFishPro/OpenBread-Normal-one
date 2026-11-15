from pathlib import Path
root = Path(__file__).resolve().parents[2]
html = (root / 'tools/device_admin/index.html').read_text(encoding='utf-8-sig')
# 固定 LF：避免 Windows 下 write_text 把 \n 变成 \r\n，让页面字符串凭空多出 1 字节/行
target = root / 'src/services/DeviceAdminPageHtml.h'
with open(target, 'w', encoding='utf-8', newline='\n') as out:
    out.write('#pragma once\n#include <Arduino.h>\nconst char kDeviceAdminPage[] PROGMEM = R"OBHTML(' + html + ')OBHTML";\n')
