#pragma once
#include <stddef.h>

// Validate an already-decoded absolute SD path before any filesystem operation.
namespace AdminFilePath {
bool valid(const char* path, size_t maxBytes = 240);
bool isFontResource(const char* path);
}
