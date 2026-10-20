#pragma once

#include <stdint.h>
#include "../bsp/SdFontFile.h"

// NBFT v1: no full-file/index allocation. One open size, bounded index/glyph caches.
class SdBitmapFont {
public:
  enum class State : uint8_t { Closed, Checking, Ready, Error };
  enum class Error : uint8_t { None, OpenFailed, Header, SizeMismatch, Read, Crc, Glyph };

  struct Metrics {
    uint16_t pixelSize = 0;
    int16_t ascent = 0;
    int16_t descent = 0;
    int16_t lineGap = 0;
    uint32_t glyphCount = 0;
    uint32_t lineHeight() const { return ascent + descent + lineGap; }
  };

  struct Glyph {
    uint32_t codepoint = 0;
    uint32_t session = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    int16_t xOffset = 0;
    int16_t yOffset = 0;
    int16_t advance = 0;
  };

  bool open(uint16_t pixelSize);
  void end();
  void tick();
  bool consumeChanged();
  bool glyph(uint32_t codepoint, Glyph& out);
  bool readRow(const Glyph& glyph, uint16_t row, uint8_t* data, size_t capacity);
  State state() const { return state_; }
  Error error() const { return error_; }
  const Metrics& metrics() const { return metrics_; }

private:
  static constexpr uint8_t kCacheSlots = 8;
  static constexpr uint16_t kGlyphCacheBytes = 128;
  static constexpr uint8_t kIndexBlockRecords = 16;
  static constexpr uint8_t kRecordBytes = 24;
  struct CacheSlot {
    Glyph glyph;
    uint8_t bitmap[kGlyphCacheBytes];
    bool valid = false;
    bool found = false;
  };

  bool readRecord(uint32_t index, Glyph& glyph);
  bool fail(Error error);
  bool validateRow(const Glyph& glyph, const uint8_t* data) const;
  void clearCache();

  SdFontFile file_;
  Metrics metrics_;
  State state_ = State::Closed;
  Error error_ = Error::None;
  bool changed_ = false;
  uint32_t bitmapOffset_ = 0;
  uint32_t verifyOffset_ = 0;
  uint32_t checksum_ = 0xFFFFFFFFU;
  uint32_t expectedChecksum_ = 0;
  uint32_t session_ = 0;
  uint32_t indexBlockStart_ = UINT32_MAX;
  uint8_t indexBlock_[kIndexBlockRecords * kRecordBytes];
  CacheSlot cache_[kCacheSlots];
  uint8_t nextSlot_ = 0;
};
