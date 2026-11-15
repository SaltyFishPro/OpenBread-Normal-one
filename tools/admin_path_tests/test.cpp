#include "../../src/services/AdminFilePath.h"
#include <cassert>
#include <string>
#include <cstdio>
int main() {
  using namespace AdminFilePath;
  assert(valid("/") && valid("/music/晴天.mp3") && valid("/Font/font-14.bin"));
  assert(!valid(nullptr) && !valid("") && !valid("relative") && !valid("//music"));
  assert(!valid("/music/../Font") && !valid("/./a") && !valid("/music/"));
  assert(!valid("/music\\file") && !valid("/music/evil:bad") && !valid("/a\nname"));
  assert(!valid("/Font./font-14.bin") && !valid("/Font /font-14.bin"));
  std::string longPath="/"+std::string(241,'a');assert(!valid(longPath.c_str()));
  assert(isFontResource("/Font") && isFontResource("/font/file") && !isFontResource("/FontBackup"));
  std::puts("PASS: SD path traversal, bounds and protected font directory");
}
