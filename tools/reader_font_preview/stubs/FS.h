#pragma once
#include <Arduino.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#define FILE_READ "r"
enum SeekMode { SeekSet };
namespace TestFs {
inline std::string root, failedPath;
inline size_t reads = 0, bytes = 0, handles = 0, refreshes = 0;
inline uint32_t readLatencyMs = 0;
inline bool unavailable = false;
}

// Physical SD replacement only. ReaderService and SdFontFile use their real parsers.
class File {
  struct Handle {
    std::FILE* file = nullptr;
    std::vector<std::string> entries;
    size_t next = 0;
    ~Handle() { if (file) std::fclose(file); --TestFs::handles; }
  };
public:
  File() = default;
  explicit File(const std::string& path) : path_(path) {
    if (failed() || !std::filesystem::exists(path)) return;
    directory_ = std::filesystem::is_directory(path);
    auto raw = directory_ ? nullptr : std::fopen(path.c_str(), "rb");
    if (!directory_ && !raw) return;
    ++TestFs::handles;
    handle_ = std::make_shared<Handle>();
    handle_->file = raw;
    if (directory_) {
      for (const auto& e : std::filesystem::directory_iterator(path))
        handle_->entries.push_back(e.path().generic_string());
    } else {
      size_ = std::filesystem::file_size(path);
    }
    name_ = std::filesystem::path(path).filename().string();
  }
  explicit operator bool() const { return handle_ != nullptr; }
  bool isDirectory() const { return directory_; }
  const char* name() const { return name_.c_str(); }
  File openNextFile() {
    if (!handle_ || !directory_ || handle_->next >= handle_->entries.size()) return {};
    return File(handle_->entries[handle_->next++]);
  }
  void close() { handle_.reset(); size_ = 0; }
  size_t size() const { return size_; }
  size_t position() const { return readable() ? std::ftell(handle_->file) : size_; }
  bool available() const { return readable() && position() < size_; }
  bool seek(uint32_t offset, SeekMode = SeekSet) {
    return readable() && !std::fseek(handle_->file, offset, SEEK_SET);
  }
  size_t read(uint8_t* data, size_t count) {
    if (!readable()) return 0;
    ++TestFs::reads;
    TestClock::nowMs += TestFs::readLatencyMs;
    const size_t got = std::fread(data, 1, count, handle_->file);
    TestFs::bytes += got;
    return got;
  }
  int read() { uint8_t value; return read(&value, 1) == 1 ? value : -1; }
  int peek() {
    if (!readable()) return -1;
    const int value = std::fgetc(handle_->file);
    if (value >= 0) std::ungetc(value, handle_->file);
    return value;
  }
  size_t readBytesUntil(char delimiter, char* data, size_t capacity) {
    size_t count = 0;
    while (count < capacity) {
      const int value = read();
      if (value < 0 || value == delimiter) break;
      data[count++] = static_cast<char>(value);
    }
    return count;
  }
private:
  bool failed() const {
    return TestFs::unavailable || (!TestFs::failedPath.empty() &&
                                  path_.find(TestFs::failedPath) != std::string::npos);
  }
  bool readable() const { return handle_ && handle_->file && !failed(); }
  std::string path_, name_;
  std::shared_ptr<Handle> handle_;
  size_t size_ = 0;
  bool directory_ = false;
};
