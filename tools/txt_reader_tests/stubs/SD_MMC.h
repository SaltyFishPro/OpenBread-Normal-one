#pragma once
#include "FS.h"
struct TestSdMmc {
  File open(const char* path, const char* = FILE_READ) {
    return TestFs::unavailable ? File{} : File(TestFs::root + path);
  }
  bool exists(const char* path) { return std::filesystem::exists(TestFs::root+path); }
  bool mkdir(const char* path) {
    return !TestFs::unavailable && std::filesystem::create_directories(TestFs::root+path);
  }
};
inline TestSdMmc SD_MMC;
