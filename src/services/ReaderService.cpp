#include "ReaderService.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "SdCardService.h"

namespace {
constexpr const char* kWordsDir = "/words";
constexpr size_t kLineBufferSize = 768;
constexpr uint32_t kInitialIndexCapacity = 256;

const char* const kBookLabelsZh[ReaderService::kBookCount] = {
    "初中", "高中", "四级", "六级", "考研", "托福", "SAT", "我的"};

const char* const kBookAliases[ReaderService::kBookCount][4] = {
    {"初中", "junior", "middle", "cz"},
    {"高中", "senior", "high", "gz"},
    {"四级", "cet4", "cet-4", "level4"},
    {"六级", "cet6", "cet-6", "level6"},
    {"考研", "npee", "postgraduate", "kaoyan"},
    {"托福", "toefl", "ibt", "tuofu"},
    {"sat", "SAT", "scholastic", "sat"},
    {"我的", "mine", "my", "custom"},
};

void copyString(char* dst, size_t dstSize, const char* src) {
  if (dst == nullptr || dstSize == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  std::snprintf(dst, dstSize, "%s", src);
}

bool containsIgnoreCase(const char* text, const char* needle) {
  if (text == nullptr || needle == nullptr || needle[0] == '\0') {
    return false;
  }

  const size_t needleLen = std::strlen(needle);
  for (const char* p = text; *p != '\0'; ++p) {
    size_t i = 0;
    while (i < needleLen && p[i] != '\0' &&
           std::tolower(static_cast<unsigned char>(p[i])) ==
               std::tolower(static_cast<unsigned char>(needle[i]))) {
      ++i;
    }
    if (i == needleLen) {
      return true;
    }
  }
  return false;
}

bool endsWithIgnoreCase(const char* text, const char* suffix) {
  if (text == nullptr || suffix == nullptr) {
    return false;
  }
  const size_t textLen = std::strlen(text);
  const size_t suffixLen = std::strlen(suffix);
  if (suffixLen > textLen) {
    return false;
  }
  return containsIgnoreCase(text + textLen - suffixLen, suffix);
}

void trimInPlace(char* text) {
  if (text == nullptr) {
    return;
  }

  char* start = text;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
    ++start;
  }
  if (start != text) {
    std::memmove(text, start, std::strlen(start) + 1);
  }

  size_t len = std::strlen(text);
  while (len > 0) {
    const char c = text[len - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    text[--len] = '\0';
  }
}

void stripUtf8Bom(char* text) {
  if (text == nullptr) {
    return;
  }
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
  if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
    std::memmove(text, text + 3, std::strlen(text + 3) + 1);
  }
}

bool readRawLine(File& file, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return false;
  }

  if (!file.available()) {
    return false;
  }

  const size_t len = file.readBytesUntil('\n', out, outSize - 1);
  out[len] = '\0';
  if (len == outSize - 1 && file.available()) {
    int next = file.peek();
    while (next >= 0 && next != '\n') {
      (void)file.read();
      if (!file.available()) {
        break;
      }
      next = file.peek();
    }
    if (file.available() && file.peek() == '\n') {
      (void)file.read();
    }
  }

  stripUtf8Bom(out);
  trimInPlace(out);
  return true;
}

bool readDataLine(File& file, char* out, size_t outSize, uint32_t* lineOffset = nullptr) {
  if (out == nullptr || outSize == 0) {
    return false;
  }

  while (file.available()) {
    const uint32_t offset = static_cast<uint32_t>(file.position());
    if (!readRawLine(file, out, outSize)) {
      return false;
    }
    stripUtf8Bom(out);
    trimInPlace(out);
    if (out[0] == '\0' || out[0] == '#') {
      continue;
    }
    if (lineOffset != nullptr) {
      *lineOffset = offset;
    }
    return true;
  }

  return false;
}
}  // namespace

bool ReaderService::begin() {
  reset();
  return true;
}

void ReaderService::reset() {
  state_ = State::Idle;
  entry_ = Entry{};
  bookIndex_ = 0;
  currentIndex_ = 0;
  currentPath_[0] = '\0';
  errorText_[0] = '\0';
  clearCache();
  markChanged();
}

bool ReaderService::openBook(uint8_t bookIndex, SdCardService& sd) {
  if (bookIndex >= kBookCount) {
    setState(State::Error, "book index invalid");
    return false;
  }

  bookIndex_ = bookIndex;
  currentIndex_ = 0;
  entry_ = Entry{};
  currentPath_[0] = '\0';
  clearCache();

  if (sd.refresh() != SdCardService::Status::Ready) {
    setState(State::SdMissing, "sd not ready");
    return false;
  }

  const FindResult found = findBookPath(bookIndex, currentPath_, sizeof(currentPath_));
  if (found == FindResult::DirMissing) {
    setState(State::DirMissing, "words dir missing");
    return false;
  }
  if (found == FindResult::FileMissing) {
    setState(State::FileMissing, "word file missing");
    return false;
  }

  if (!buildIndex() || recordCount_ == 0) {
    setState(State::Empty, "word file empty");
    return false;
  }

  if (!loadRecordAt(0, entry_)) {
    setState(State::Error, "word first read failed");
    return false;
  }

  setState(State::Ready, "");
  return true;
}

bool ReaderService::move(int8_t delta) {
  if (state_ != State::Ready || currentPath_[0] == '\0') {
    return false;
  }

  if (delta == 0) {
    return false;
  }

  const uint32_t nextIndex =
      delta < 0
          ? (currentIndex_ == 0 ? 0 : static_cast<uint32_t>(currentIndex_ - 1U))
          : static_cast<uint32_t>(currentIndex_ + 1U);
  if (nextIndex == currentIndex_) {
    return false;
  }
  if (recordCount_ > 0 && nextIndex >= recordCount_) {
    return false;
  }

  Entry next{};
  if (!loadRecordAt(nextIndex, next)) {
    return false;
  }

  entry_ = next;
  currentIndex_ = nextIndex;
  markChanged();
  return true;
}

bool ReaderService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

ReaderService::State ReaderService::state() const { return state_; }

const ReaderService::Entry& ReaderService::entry() const { return entry_; }

uint8_t ReaderService::bookIndex() const { return bookIndex_; }

const char* ReaderService::bookLabelZh() const {
  return bookIndex_ < kBookCount ? kBookLabelsZh[bookIndex_] : "";
}

uint32_t ReaderService::currentIndex() const { return currentIndex_; }

uint32_t ReaderService::recordCount() const { return recordCount_; }

bool ReaderService::hasPrevious() const { return currentIndex_ > 0; }

const char* ReaderService::errorText() const { return errorText_; }

ReaderService::FindResult ReaderService::findBookPath(uint8_t bookIndex, char* out,
                                                      size_t outSize) {
  if (bookIndex >= kBookCount) {
    return FindResult::FileMissing;
  }

  char path[96];
  for (const char* alias : kBookAliases[bookIndex]) {
    if (alias == nullptr || alias[0] == '\0') {
      continue;
    }
    std::snprintf(path, sizeof(path), "%s/%s.txt", kWordsDir, alias);
    if (tryExactPath(path, out, outSize)) {
      return FindResult::Found;
    }
  }

  return scanBookDir(bookIndex, out, outSize);
}

bool ReaderService::tryExactPath(const char* path, char* out, size_t outSize) const {
  File file = SD_MMC.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return false;
  }

  file.close();
  copyString(out, outSize, path);
  return true;
}

ReaderService::FindResult ReaderService::scanBookDir(uint8_t bookIndex, char* out,
                                                     size_t outSize) const {
  File root = SD_MMC.open(kWordsDir);
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    return FindResult::DirMissing;
  }

  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }

    const bool isFile = !entry.isDirectory();
    const char* name = entry.name();
    bool matched = false;
    if (isFile && endsWithIgnoreCase(name, ".txt")) {
      for (const char* alias : kBookAliases[bookIndex]) {
        if (containsIgnoreCase(name, alias)) {
          matched = true;
          break;
        }
      }
    }

    if (matched) {
      char path[96];
      if (name[0] == '/') {
        copyString(path, sizeof(path), name);
      } else {
        std::snprintf(path, sizeof(path), "%s/%s", kWordsDir, name);
      }
      entry.close();
      root.close();
      copyString(out, outSize, path);
      return FindResult::Found;
    }

    entry.close();
  }

  root.close();
  return FindResult::FileMissing;
}

bool ReaderService::buildIndex() {
  recordCount_ = 0;

  File file = SD_MMC.open(currentPath_, FILE_READ);
  if (!file) {
    return false;
  }

  char line[kLineBufferSize];
  while (file.available()) {
    uint32_t lineStartOffset = 0;
    if (!readDataLine(file, line, sizeof(line), &lineStartOffset)) {
      break;
    }
    if (!appendRecordOffset(lineStartOffset)) {
      file.close();
      return false;
    }
  }

  file.close();
  return true;
}

bool ReaderService::appendRecordOffset(uint32_t offset) {
  if (recordCount_ >= kMaxIndexedRecords) {
    return false;
  }

  if (recordCount_ >= recordCapacity_) {
    uint32_t newCapacity =
        recordCapacity_ == 0 ? kInitialIndexCapacity : static_cast<uint32_t>(recordCapacity_ * 2U);
    if (newCapacity > kMaxIndexedRecords) {
      newCapacity = kMaxIndexedRecords;
    }

    const size_t bytes = static_cast<size_t>(newCapacity) * sizeof(uint32_t);
    uint32_t* nextOffsets = nullptr;
    const bool usePsram = psramFound();
    if (usePsram) {
      nextOffsets = static_cast<uint32_t*>(ps_malloc(bytes));
    }
    if (nextOffsets == nullptr) {
      nextOffsets = static_cast<uint32_t*>(std::malloc(bytes));
    }
    if (nextOffsets == nullptr) {
      return false;
    }

    if (recordOffsets_ != nullptr && recordCount_ > 0) {
      std::memcpy(nextOffsets, recordOffsets_, static_cast<size_t>(recordCount_) * sizeof(uint32_t));
    }
    std::free(recordOffsets_);
    recordOffsets_ = nextOffsets;
    recordCapacity_ = newCapacity;
  }

  recordOffsets_[recordCount_++] = offset;
  return true;
}

bool ReaderService::loadRecordAt(uint32_t index, Entry& out) const {
  if (recordOffsets_ != nullptr && index < recordCount_) {
    return loadRecordFromOffset(recordOffsets_[index], out);
  }

  File file = SD_MMC.open(currentPath_, FILE_READ);
  if (!file) {
    return false;
  }

  char line[kLineBufferSize];
  uint32_t current = 0;
  while (file.available()) {
    if (!readDataLine(file, line, sizeof(line))) {
      break;
    }
    if (current == index) {
      file.close();
      return parseLine(line, out);
    }
    ++current;
  }

  file.close();
  return false;
}

bool ReaderService::loadRecordFromOffset(uint32_t offset, Entry& out) const {
  File file = SD_MMC.open(currentPath_, FILE_READ);
  if (!file) {
    return false;
  }

  if (!file.seek(offset)) {
    file.close();
    return false;
  }

  char line[kLineBufferSize];
  if (!readDataLine(file, line, sizeof(line))) {
    file.close();
    return false;
  }

  file.close();
  return parseLine(line, out);
}

void ReaderService::clearCache() {
  if (recordOffsets_ != nullptr) {
    std::free(recordOffsets_);
    recordOffsets_ = nullptr;
  }
  recordCount_ = 0;
  recordCapacity_ = 0;
}

bool ReaderService::parseLine(char* line, Entry& out) const {
  if (line == nullptr) {
    return false;
  }

  trimInPlace(line);
  if (line[0] == '\0') {
    return false;
  }

  char* word = line;
  char* phonetic = nullptr;
  char* meaning = nullptr;

  char* sep1 = std::strchr(line, '\t');
  char* sep2 = nullptr;
  if (sep1 != nullptr) {
    *sep1 = '\0';
    phonetic = sep1 + 1;
    sep2 = std::strchr(phonetic, '\t');
  } else {
    sep1 = std::strchr(line, ',');
    if (sep1 != nullptr) {
      *sep1 = '\0';
      phonetic = sep1 + 1;
      sep2 = std::strchr(phonetic, ',');
    }
  }

  if (sep2 != nullptr) {
    *sep2 = '\0';
    meaning = sep2 + 1;
  } else if (phonetic != nullptr) {
    meaning = phonetic;
    phonetic = nullptr;
  }

  trimInPlace(word);
  if (phonetic != nullptr) {
    trimInPlace(phonetic);
  }
  if (meaning != nullptr) {
    trimInPlace(meaning);
  }

  if (word[0] == '\0') {
    return false;
  }

  out = Entry{};
  copyString(out.word, sizeof(out.word), word);
  copyString(out.phonetic, sizeof(out.phonetic), phonetic != nullptr ? phonetic : "");
  copyString(out.meaning, sizeof(out.meaning), meaning != nullptr ? meaning : "");
  return true;
}

void ReaderService::setState(State state, const char* error) {
  state_ = state;
  copyString(errorText_, sizeof(errorText_), error);
  markChanged();
}

void ReaderService::markChanged() { changed_ = true; }
