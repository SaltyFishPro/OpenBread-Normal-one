#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#define FILE_READ "r"
#define FILE_WRITE "w"
namespace SetupFs { inline std::map<std::string,std::vector<uint8_t>> files; }
class File {
  std::string path_;
  std::shared_ptr<size_t> cursor_;
public:
  File()=default;
  File(const std::string& p,bool write):path_(p) {
    if(write) SetupFs::files[p].clear();
    if(SetupFs::files.count(p))cursor_=std::make_shared<size_t>(0);
  }
  explicit operator bool()const{return bool(cursor_);}
  bool isDirectory()const{return false;}
  size_t size()const{return cursor_?SetupFs::files[path_].size():0;}
  size_t read(uint8_t* p,size_t n){if(!cursor_)return 0;auto& f=SetupFs::files[path_];n=std::min(n,f.size()-*cursor_);memcpy(p,f.data()+*cursor_,n);*cursor_+=n;return n;}
  size_t write(const uint8_t* p,size_t n){if(!cursor_)return 0;auto& f=SetupFs::files[path_];f.insert(f.end(),p,p+n);return n;}
  void flush(){}
  void close(){cursor_.reset();}
};
