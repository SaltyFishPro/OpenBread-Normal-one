#pragma once

#include "SdBitmapFont.h"
#include "../bsp/SdFontFile.h"

// Fixed UI glyphs copied from NBFT 14px. After loading, no SD handle or I/O is
// needed, including while USB owns the card or the device wakes without a card.
class UiFontCache {
public:
  enum class State : uint8_t { Idle, Loading, Ready, Error };
  struct Glyph {
    uint16_t width, height;
    int16_t xOffset, yOffset, advance;
    uint16_t length;
    uint8_t bitmap[64];
  };
  static constexpr size_t kBudgetBytes = 64U * 1024U;
  bool begin();  // Caller owns/mounts SD. Does not delay for CRC or glyph loading.
  void tick();
  void cancelLoad();
  void end();
  bool consumeChanged();
  State state() const { return state_; }
  const Glyph* glyph(uint32_t codepoint) const;
  size_t cacheBytes() const { return memory_.size(); }
private:
  void fail(const char* reason);
  bool beginSubset();
  void tickSubset();
  void finishSubset();
  void fallbackToFullFont(const char* reason);
  void freeSubset();
  SdBitmapFont source_;
  SdFontFile subsetFile_;
  SdFontCacheBuffer memory_;
  SdFontCacheBuffer subsetMemory_;
  Glyph* glyphs_ = nullptr;
  uint8_t* subsetData_ = nullptr;
  uint32_t subsetSize_ = 0;
  uint32_t subsetLoaded_ = 0;
  uint32_t subsetCrc_ = 0xFFFFFFFFU;
  uint32_t subsetExpectedCrc_ = 0;
  uint32_t subsetParsed_ = 0;
  uint32_t subsetPrevCp_ = 0;
  uint16_t subsetCount_ = 0;
  bool subsetActive_ = false;
  bool subsetCrcOk_ = false;
  size_t next_ = 0;
  size_t subsetCursor_ = 0;
  uint32_t startedMs_ = 0;
  State state_ = State::Idle;
  bool changed_ = false;
};
