from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
s=(root/'tools/home_menu_preview/admin_hotspot_qr.svg').read_text(encoding='utf-8')
size=29; rows=bytearray(size*4)
for x,y in re.findall(r'M(\d+),(\d+)H',s):
 x,y=int(x)-4,int(y)-4
 if 0<=x<size and 0<=y<size: rows[y*4+x//8] |= 0x80>>(x%8)
values=', '.join(str(x) for x in bytes([size])+rows)
(root/'tools/admin_framebuffer/stubs/qrcode.h').write_text('''#pragma once
#include <cstdint>
#include <cstring>
typedef const uint8_t* esp_qrcode_handle_t;
typedef int esp_err_t;
enum {ESP_OK=0, ESP_FAIL=-1, ESP_QRCODE_ECC_LOW=0};
struct esp_qrcode_config_t {void (*display_func)(esp_qrcode_handle_t); int max_qrcode_version; int qrcode_ecc_level;};
inline int esp_qrcode_get_size(esp_qrcode_handle_t qr) {return qr[0];}
inline bool esp_qrcode_get_module(esp_qrcode_handle_t qr,int x,int y) {return (qr[1+y*4+x/8] & (0x80>>(x%8)))!=0;}
inline int esp_qrcode_generate(esp_qrcode_config_t* config,const char* text) {
 if (strcmp(text,"WIFI:T:nopass;S:OpenBread-Manage-1A2B;;")) return ESP_FAIL;
 static const uint8_t qr[]={'''+values+'''};
 config->display_func(qr); return ESP_OK;
}
''',encoding='utf-8')
