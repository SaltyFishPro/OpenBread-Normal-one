#include "MusicLyrics.h"
#include "LrcParser.h"
#include <SD_MMC.h>
#include <cstring>
#include "../log/Log.h"

void MusicLyrics::end() {
  if (file_) file_.close();
  file_ = File(); memory_.end(); entries_ = nullptr;
  inputLength_ = count_ = 0; remaining_ = 0; overflow_ = false; state_ = State::Empty;
}
void MusicLyrics::begin(const char* audioPath) {
  end();
  if (!audioPath || !*audioPath) return;
  char path[128];
  const size_t length = strlen(audioPath);
  if (length >= sizeof(path) - 5) { LogW("LYRICS", "path too long"); return; }
  memcpy(path, audioPath, length + 1);
  char* slash = strrchr(path, '/'); char* extension = strrchr(path, '.');
  if (extension && (!slash || extension > slash)) *extension = '\0';
  strcat(path, ".lrc");
  file_ = SD_MMC.open(path, FILE_READ);
  if (!file_) { LogI("LYRICS", "not found path=%s", path); return; }
  if (file_.isDirectory() || file_.size() > 65536U || !memory_.allocate(sizeof(Entry) * kMaxLines)) {
    file_.close(); file_ = File(); state_ = State::Error;
    LogW("LYRICS", "invalid size or PSRAM unavailable path=%s", path); return;
  }
  entries_ = static_cast<Entry*>(memory_.data());
  remaining_ = file_.size();
  state_ = State::Loading;
  LogI("LYRICS", "loading path=%s bytes=%lu", path, static_cast<unsigned long>(file_.size()));
}
void MusicLyrics::tick() {
  if (state_ != State::Loading) return;
  uint8_t block[256];
  const size_t length = remaining_ < sizeof(block) ? remaining_ : sizeof(block);
  if (length && file_.read(block, length) != length) {
    end(); state_ = State::Error; LogW("LYRICS", "read failed"); return;
  }
  remaining_ -= length;
  for (size_t i = 0; i < length; ++i) {
    const char c = static_cast<char>(block[i]);
    if (c == '\n') { parseLine(); inputLength_ = 0; overflow_ = false; }
    else if (c != '\r') {
      if (inputLength_ + 1 < sizeof(input_)) input_[inputLength_++] = c;
      else overflow_ = true;
    }
  }
  if (remaining_) return;
  if (inputLength_) parseLine();
  file_.close(); file_ = File();
  if (!count_) { memory_.end(); entries_ = nullptr; state_ = State::Empty; }
  else state_ = State::Ready;
  LogI("LYRICS", "loaded lines=%u", count_);
}
void MusicLyrics::parseLine() {
  if (overflow_) return;
  input_[inputLength_] = '\0';
  uint32_t times[LrcParser::kMaxTimestamps]; const char* text = nullptr;
  const size_t count = LrcParser::parse(input_, times, LrcParser::kMaxTimestamps, text);
  if (!count) return;
  size_t length = strlen(text);
  if (length >= kLineBytes) {
    length = kLineBytes - 1;
    while (length && (static_cast<uint8_t>(text[length]) & 0xC0U) == 0x80U) --length;
  }
  for (size_t i = 0; i < count && count_ < kMaxLines; ++i) {
    uint16_t pos = count_;
    while (pos && entries_[pos - 1].time > times[i]) { entries_[pos] = entries_[pos - 1]; --pos; }
    entries_[pos].time = times[i]; memcpy(entries_[pos].text, text, length);
    entries_[pos].text[length] = '\0'; ++count_;
  }
}
int MusicLyrics::current(uint32_t ms) const {
  if (state_ != State::Ready || !count_ || ms < entries_[0].time) return -1;
  uint16_t low = 0, high = count_;
  while (low < high) { const uint16_t mid = low + (high - low) / 2;
    if (entries_[mid].time <= ms) low = mid + 1; else high = mid; }
  return static_cast<int>(low) - 1;
}
const char* MusicLyrics::line(int index) const {
  return state_ == State::Ready && index >= 0 && index < count_ ? entries_[index].text : "";
}
