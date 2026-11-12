#pragma once

#include <stddef.h>

namespace OtaReleaseNotes {
// 可选日志字段；支持 JSON 转义，超长内容在完整 UTF-8 字符处截断。
// 无字段或格式无效时返回 false，调用方继续使用原来的短摘要。
bool parse(const char* json, char* out, size_t capacity);

// 读取任意顶层字符串字段。与 parse 使用同一套 JSON 转义和 UTF-8 截断规则，
// 供 manifest 中允许截断的展示字段（release_note）使用；关键字段仍走严格解析。
bool parseField(const char* json, const char* key, char* out, size_t capacity);
}
