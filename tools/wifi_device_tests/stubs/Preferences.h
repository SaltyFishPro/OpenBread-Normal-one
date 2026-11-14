#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>

namespace TestPrefs {
inline std::map<std::string,std::vector<uint8_t>> entries;
inline bool failWrite = false;
}
class Preferences {
public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  size_t getBytesLength(const char* key) { return TestPrefs::entries[key].size(); }
  size_t getBytes(const char* key, void* out, size_t size) {
    const auto& bytes=TestPrefs::entries[key];
    if(bytes.size()!=size)return 0;
    memcpy(out,bytes.data(),size);return size;
  }
  size_t putBytes(const char* key,const void* data,size_t size) {
    if(TestPrefs::failWrite)return 0;
    const auto* bytes=static_cast<const uint8_t*>(data);
    TestPrefs::entries[key]=std::vector<uint8_t>(bytes,bytes+size);return size;
  }
  size_t getString(const char* key,char* out,size_t capacity) {
    const auto& bytes=TestPrefs::entries[key];
    const size_t size=bytes.size()<capacity?bytes.size():capacity-1;
    if(size)memcpy(out,bytes.data(),size);out[size]=0;return size;
  }
  bool remove(const char* key) { return TestPrefs::entries.erase(key)!=0; }
};
