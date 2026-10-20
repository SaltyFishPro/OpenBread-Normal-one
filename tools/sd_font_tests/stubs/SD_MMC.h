#pragma once
#include "FS.h"

struct TestSdMmc {
  File open(const char* path, const char* mode) {
    if (TestFs::unavailable || std::string(mode) != FILE_READ) return File{};
    return File(TestFs::root + path);
  }
};
inline TestSdMmc SD_MMC;
