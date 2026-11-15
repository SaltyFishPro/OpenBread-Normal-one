#include "AdminFilePath.h"
#include <stdint.h>
#include <cstring>

namespace AdminFilePath {
bool valid(const char* path, size_t maxBytes) {
  if (!path || path[0] != '/' || maxBytes == 0) return false;
  if (path[1] == '\0') return true;
  size_t componentStart = 1;
  for (size_t i = 1; i <= maxBytes; ++i) {
    const unsigned char c = static_cast<unsigned char>(path[i]);
    if (!c || c == '/') {
      const size_t length = i - componentStart;
      if (!length || (length == 1 && path[componentStart] == '.') ||
          (length == 2 && path[componentStart] == '.' && path[componentStart + 1] == '.')) return false;
      // FAT may normalize trailing spaces or dots to another existing name.
      if (path[i - 1] == '.' || path[i - 1] == ' ') return false;
      if (!c) return true;
      componentStart = i + 1;
    } else if (c < 0x20 || c == 0x7f || c == '\\' || c == ':') return false;
  }
  return false;
}
bool isFontResource(const char* path) {
  if (!valid(path)) return false;
  constexpr char prefix[] = "/Font";
  for (size_t i = 0; i < sizeof(prefix) - 1; ++i) {
    unsigned char c = static_cast<unsigned char>(path[i]);
    if (!c) return false;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    unsigned char p = static_cast<unsigned char>(prefix[i]);
    if (p >= 'A' && p <= 'Z') p += 'a' - 'A';
    if (c != p) return false;
  }
  return path[sizeof(prefix) - 1] == '\0' || path[sizeof(prefix) - 1] == '/';
}
}
