#pragma once
#include <FS.h>
#include "../bsp/SdFontCacheBuffer.h"

// Loaded cooperatively while the music page is active; no persistent card handle.
class MusicLyrics {
public:
  enum class State : uint8_t { Empty, Loading, Ready, Error };
  static constexpr uint16_t kMaxLines = 128;
  static constexpr uint16_t kLineBytes = 96;
  void begin(const char* audioPath);
  void tick();
  void end();
  State state() const { return state_; }
  int current(uint32_t ms) const;
  const char* line(int index) const;
private:
  struct Entry { uint32_t time; char text[kLineBytes]; };
  void parseLine();
  SdFontCacheBuffer memory_;
  Entry* entries_ = nullptr;
  File file_;
  char input_[256] = {};
  uint16_t inputLength_ = 0, count_ = 0;
  uint32_t remaining_ = 0;
  bool overflow_ = false;
  State state_ = State::Empty;
};
