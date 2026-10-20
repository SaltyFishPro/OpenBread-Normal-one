#include "OtaReleaseNotes.h"

#include <stdint.h>
#include <string.h>

namespace {
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool codeUnit(const char*& p, uint32_t& value) {
  value = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    const int digit = hex(*p);
    if (digit < 0) return false;
    value = value * 16U + static_cast<uint32_t>(digit);
    ++p;
  }
  return true;
}

bool readString(const char*& p, char* out, size_t capacity) {
  if (*p++ != '"') return false;
  size_t used = 0;
  bool full = false;
  while (*p && *p != '"') {
    char bytes[4];
    uint8_t length = 1;
    const uint8_t first = static_cast<uint8_t>(*p++);
    if (first == '\\') {
      const char escaped = *p++;
      switch (escaped) {
        case '"': bytes[0] = '"'; break;
        case '\\': bytes[0] = '\\'; break;
        case '/': bytes[0] = '/'; break;
        case 'n': case 'r': bytes[0] = '\n'; break;
        case 't': case 'b': case 'f': bytes[0] = ' '; break;
        case 'u': {
          uint32_t cp;
          if (!codeUnit(p, cp)) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (p[0] != '\\' || p[1] != 'u') return false;
            p += 2;
            uint32_t low;
            if (!codeUnit(p, low) || low < 0xDC00 || low > 0xDFFF) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10U) + low - 0xDC00;
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
          if (cp == 0) return false;
          if (cp < 0x80) bytes[0] = static_cast<char>(cp);
          else if (cp < 0x800) {
            length = 2;
            bytes[0] = static_cast<char>(0xC0 | (cp >> 6U));
            bytes[1] = static_cast<char>(0x80 | (cp & 0x3F));
          } else if (cp < 0x10000) {
            length = 3;
            bytes[0] = static_cast<char>(0xE0 | (cp >> 12U));
            bytes[1] = static_cast<char>(0x80 | ((cp >> 6U) & 0x3F));
            bytes[2] = static_cast<char>(0x80 | (cp & 0x3F));
          } else {
            length = 4;
            bytes[0] = static_cast<char>(0xF0 | (cp >> 18U));
            bytes[1] = static_cast<char>(0x80 | ((cp >> 12U) & 0x3F));
            bytes[2] = static_cast<char>(0x80 | ((cp >> 6U) & 0x3F));
            bytes[3] = static_cast<char>(0x80 | (cp & 0x3F));
          }
          break;
        }
        default: return false;
      }
    } else {
      if (first < 0x20 || (first >= 0x80 && first < 0xC2) || first > 0xF4) return false;
      length = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
      bytes[0] = static_cast<char>(first);
      for (uint8_t i = 1; i < length; ++i) {
        const uint8_t next = static_cast<uint8_t>(*p);
        if ((next & 0xC0) != 0x80) return false;
        bytes[i] = *p++;
      }
      if ((first == 0xE0 && static_cast<uint8_t>(bytes[1]) < 0xA0) ||
          (first == 0xED && static_cast<uint8_t>(bytes[1]) >= 0xA0) ||
          (first == 0xF0 && static_cast<uint8_t>(bytes[1]) < 0x90) ||
          (first == 0xF4 && static_cast<uint8_t>(bytes[1]) >= 0x90)) return false;
    }
    if (!full && used + length < capacity) {
      if (out) memcpy(out + used, bytes, length);
      used += length;
    } else full = true;
  }
  if (*p != '"') return false;
  ++p;
  if (out && capacity) out[used] = '\0';
  return true;
}

void skipWhitespace(const char*& p) {
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
}
}  // namespace

bool OtaReleaseNotes::parse(const char* json, char* out, size_t capacity) {
  if (!json || !out || !capacity) return false;
  out[0] = '\0';
  const char* p = json;
  int depth = 0;
  while (*p) {
    if (*p == '{' || *p == '[') { ++depth; ++p; continue; }
    if (*p == '}' || *p == ']') { --depth; ++p; continue; }
    if (*p != '"') { ++p; continue; }
    char key[32] = {};
    if (!readString(p, key, sizeof(key))) return false;
    skipWhitespace(p);
    if (depth != 1 || *p != ':' || strcmp(key, "release_notes") != 0) continue;
    ++p;
    skipWhitespace(p);
    if (*p != '"' || !readString(p, out, capacity)) {
      out[0] = '\0';
      return false;
    }
    return out[0] != '\0';
  }
  return false;
}
