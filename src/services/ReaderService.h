#pragma once

#include <stddef.h>
#include <stdint.h>

class SdCardService;

class ReaderService {
public:
  static constexpr uint8_t kBookCount = 8;
  static constexpr uint8_t kMaxWordLen = 64;
  static constexpr uint8_t kMaxPhoneticLen = 48;
  static constexpr uint16_t kMaxMeaningLen = 512;
  static constexpr uint32_t kMaxIndexedRecords = 20000;

  enum class State : uint8_t {
    Idle,
    Ready,
    SdMissing,
    DirMissing,
    FileMissing,
    Empty,
    Error
  };

  struct Entry {
    char word[kMaxWordLen] = {0};
    char phonetic[kMaxPhoneticLen] = {0};
    char meaning[kMaxMeaningLen] = {0};
  };

  bool begin();
  void reset();
  bool openBook(uint8_t bookIndex, SdCardService& sd);
  bool move(int8_t delta);
  bool consumeChanged();

  State state() const;
  const Entry& entry() const;
  uint8_t bookIndex() const;
  const char* bookLabelZh() const;
  const char* bookLabelEn() const;
  uint32_t currentIndex() const;
  uint32_t recordCount() const;
  bool hasPrevious() const;
  const char* errorText() const;

private:
  enum class FindResult : uint8_t {
    Found,
    DirMissing,
    FileMissing
  };

  FindResult findBookPath(uint8_t bookIndex, char* out, size_t outSize);
  bool tryExactPath(const char* path, char* out, size_t outSize) const;
  FindResult scanBookDir(uint8_t bookIndex, char* out, size_t outSize) const;
  bool buildIndex();
  bool appendRecordOffset(uint32_t offset);
  bool loadRecordAt(uint32_t index, Entry& out) const;
  bool loadRecordFromOffset(uint32_t offset, Entry& out) const;
  bool parseLine(char* line, Entry& out) const;
  void clearCache();
  void setState(State state, const char* error);
  void markChanged();

  State state_ = State::Idle;
  Entry entry_;
  uint8_t bookIndex_ = 0;
  uint32_t currentIndex_ = 0;
  char currentPath_[96] = {0};
  char errorText_[48] = {0};
  uint32_t* recordOffsets_ = nullptr;
  uint32_t recordCount_ = 0;
  uint32_t recordCapacity_ = 0;
  bool changed_ = false;
};
