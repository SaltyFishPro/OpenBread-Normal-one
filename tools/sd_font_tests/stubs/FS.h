#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#define FILE_READ "r"
enum SeekMode { SeekSet };
namespace TestFs {
inline std::string root;
inline size_t reads = 0, bytes = 0, handles = 0;
inline bool unavailable = false;
}

class File {
public:
  File() = default;
  explicit File(const std::string& path) {
    std::FILE* raw = std::fopen(path.c_str(), "rb");
    if (!raw) return;
    ++TestFs::handles;
    file_ = std::shared_ptr<std::FILE>(raw, [](std::FILE* f) { std::fclose(f); --TestFs::handles; });
    std::fseek(raw, 0, SEEK_END);
    size_ = std::ftell(raw);
    std::rewind(raw);
  }
  explicit operator bool() const { return file_ != nullptr; }
  bool isDirectory() const { return false; }
  size_t size() const { return size_; }
  void close() { file_.reset(); size_ = 0; }
  bool seek(uint32_t offset, SeekMode) { return !TestFs::unavailable && file_ && !std::fseek(file_.get(), offset, SEEK_SET); }
  size_t read(uint8_t* out, size_t length) {
    if (TestFs::unavailable || !file_) return 0;
    ++TestFs::reads;
    const size_t count = std::fread(out, 1, length, file_.get());
    TestFs::bytes += count;
    return count;
  }

private:
  std::shared_ptr<std::FILE> file_;
  size_t size_ = 0;
};
