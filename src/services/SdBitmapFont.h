#pragma once

#include <stdint.h>
#include "../bsp/SdFontCacheBuffer.h"
#include "../bsp/SdFontFile.h"

// NBFT v1 + optional NBCI v1: one open size, bounded common-index/glyph caches.
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

  struct CacheStats {
    uint32_t indexHits = 0;
    uint32_t indexMisses = 0;
    uint32_t glyphHits = 0;
    uint32_t glyphMisses = 0;
    uint32_t commonHits = 0;
    uint32_t commonMisses = 0;
  };

  // Reuses verified headers until end(); callers must end before SD ownership changes.
  bool open(uint16_t pixelSize, bool useExternalCache = true);
  void end();
  void tick();
  bool consumeChanged();
  bool glyph(uint32_t codepoint, Glyph& out);
  bool readRow(const Glyph& glyph, uint16_t row, uint8_t* data, size_t capacity);
  State state() const { return state_; }
  Error error() const { return error_; }
  uint32_t session() const { return session_; }
  const Metrics& metrics() const { return metrics_; }
  const CacheStats& cacheStats() const { return cacheStats_; }
  size_t cacheBytes() const { return cacheMemory_.size(); }
  uint32_t commonGlyphCount() const { return commonCount_; }

private:
  enum class CheckPhase : uint8_t { Font, CommonHeader, CommonData };
  static constexpr uint16_t kCacheSlots = 8;
  static constexpr uint16_t kPsramGlyphSlots = 256;
  static constexpr uint16_t kPsramIndexBlocks = 64;
  static constexpr uint16_t kMaxCommonGlyphs = 7500;
  static constexpr uint8_t kCommonRecordBytes = 8;
  static constexpr uint8_t kCommonHeaderBytes = 96;
  static constexpr uint8_t kGlyphWays = 4;
  static constexpr uint8_t kVerifiedFiles = 3;
  static constexpr uint16_t kGlyphCacheBytes = 128;
  static constexpr uint8_t kIndexBlockRecords = 16;
  static constexpr uint8_t kRecordBytes = 24;
  struct CacheSlot {
    Glyph glyph;
    uint8_t bitmap[kGlyphCacheBytes];
    bool valid = false;
    bool found = false;
  };
  struct IndexBlock {
    uint32_t start = UINT32_MAX;
    uint8_t records[kIndexBlockRecords * kRecordBytes];
  };
  struct PsramCache {
    CacheSlot glyphs[kPsramGlyphSlots];
    IndexBlock index[kPsramIndexBlocks];
    uint8_t common[kMaxCommonGlyphs * kCommonRecordBytes];
  };
  static_assert(sizeof(PsramCache) <= SdFontCacheBuffer::kBudgetBytes,
                "SD font cache exceeds PSRAM budget");

  bool readRecord(uint32_t index, Glyph& glyph);
  bool fail(Error error);
  bool validateRow(const Glyph& glyph, const uint8_t* data) const;
  void clearCache();
  void closeCurrent();
  void clearVerified();
  void beginCache();
  void releaseCache();
  CacheSlot* glyphSet(uint32_t codepoint);
  void beginCommonIndex();
  void tickCommonIndex();
  void disableCommonIndex(const char* reason);
  bool commonRecord(uint32_t codepoint, uint32_t& recordIndex) const;

  SdFontFile file_;
  SdFontFile commonFile_;
  SdFontCacheBuffer cacheMemory_;
  PsramCache* psramCache_ = nullptr;
  CacheStats cacheStats_;
  Metrics metrics_;
  State state_ = State::Closed;
  Error error_ = Error::None;
  bool changed_ = false;
  uint32_t bitmapOffset_ = 0;
  uint32_t verifyOffset_ = 0;
  uint32_t checksum_ = 0xFFFFFFFFU;
  uint32_t expectedChecksum_ = 0;
  uint32_t verifyStartMs_ = 0;
  uint32_t session_ = 0;
  uint32_t commonCount_ = 0;
  uint32_t commonExpectedCount_ = 0;
  uint32_t commonLoaded_ = 0;
  uint32_t commonChecksum_ = 0;
  uint32_t commonExpectedChecksum_ = 0;
  CheckPhase checkPhase_ = CheckPhase::Font;
  uint8_t header_[64];
  uint8_t verifiedHeaders_[kVerifiedFiles][64] = {};
  uint8_t nextVerified_ = 0;
  uint32_t indexBlockStart_ = UINT32_MAX;
  uint8_t indexBlock_[kIndexBlockRecords * kRecordBytes];
  CacheSlot cache_[kCacheSlots];
  uint8_t nextSlot_ = 0;
};
