#pragma once
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
 static const uint8_t qr[]={29, 254, 225, 187, 248, 130, 182, 106, 8, 186, 29, 162, 232, 186, 213, 202, 232, 186, 29, 82, 232, 130, 56, 18, 8, 254, 170, 171, 248, 0, 200, 120, 0, 183, 72, 242, 88, 249, 71, 171, 176, 75, 54, 232, 240, 208, 166, 140, 64, 115, 169, 245, 96, 224, 199, 22, 0, 43, 18, 57, 120, 0, 69, 138, 192, 191, 145, 109, 232, 116, 77, 207, 8, 150, 177, 132, 128, 49, 0, 239, 56, 110, 156, 255, 184, 0, 206, 184, 160, 254, 212, 218, 144, 130, 241, 40, 144, 186, 83, 79, 160, 186, 139, 144, 200, 186, 160, 109, 104, 130, 111, 39, 80, 254, 228, 58, 144};
 config->display_func(qr); return ESP_OK;
}
