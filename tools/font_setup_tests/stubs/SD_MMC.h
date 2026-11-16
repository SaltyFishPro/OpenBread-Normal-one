#pragma once
#include "FS.h"
struct SetupSd {
  File open(const char* p,const char* mode=FILE_READ){return File(p,strcmp(mode,FILE_WRITE)==0);}
  bool exists(const char* p){return SetupFs::files.count(p);}
  bool mkdir(const char*){return true;}
  bool remove(const char* p){SetupFs::files.erase(p);return true;}
  bool rename(const char* a,const char* b){if(!exists(a))return false;SetupFs::files[b]=SetupFs::files[a];remove(a);return true;}
};
inline SetupSd SD_MMC;
