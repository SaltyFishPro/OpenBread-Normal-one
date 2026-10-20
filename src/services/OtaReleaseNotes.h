#pragma once

#include <stddef.h>

namespace OtaReleaseNotes {
// 可选日志字段；支持 JSON 转义，超长内容在完整 UTF-8 字符处截断。
// 无字段或格式无效时返回 false，调用方继续使用原来的短摘要。
bool parse(const char* json, char* out, size_t capacity);
}
