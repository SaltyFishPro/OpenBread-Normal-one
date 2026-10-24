#include "SdBitmapFont.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <new>

#include "../log/Log.h"

namespace {
uint16_t le16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8U); }
uint32_t le32(const uint8_t* p) { return le16(p) | (uint32_t(le16(p + 2)) << 16U); }
bool scalar(uint32_t cp) { return cp <= 0x10FFFFU && !(cp >= 0xD800U && cp <= 0xDFFFU); }
uint32_t crc32(uint32_t crc, const uint8_t* data, size_t length) {
  static constexpr uint32_t table[16] = {
      0, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
      0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8,
      0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    crc = (crc >> 4U) ^ table[crc & 15U];
    crc = (crc >> 4U) ^ table[crc & 15U];
  }
  return crc;
}
}

void SdBitmapFont::clearCache() {
  indexBlockStart_ = UINT32_MAX;
  nextSlot_ = 0;
  for (auto& slot : cache_) slot.valid = false;
  if (psramCache_) {
    for (auto& slot : psramCache_->glyphs) slot.valid = false;
    for (auto& block : psramCache_->index) block.start = UINT32_MAX;
  }
  cacheStats_ = CacheStats{};
}

void SdBitmapFont::beginCache() {
  if (psramCache_) return;
  if (cacheMemory_.allocate(sizeof(PsramCache))) {
    psramCache_ = new (cacheMemory_.data()) PsramCache;
    LogI("FONT", "cache=psram bytes=%u budget=%u glyphs=%u index_blocks=%u common_max=%u",
         static_cast<unsigned>(cacheMemory_.size()),
         static_cast<unsigned>(SdFontCacheBuffer::kBudgetBytes),
         kPsramGlyphSlots, kPsramIndexBlocks, kMaxCommonGlyphs);
  } else {
    LogI("FONT", "cache=internal glyphs=%u; PSRAM unavailable or allocation failed", kCacheSlots);
  }
}

void SdBitmapFont::releaseCache() {
  if (!psramCache_) return;
  psramCache_->~PsramCache();
  psramCache_ = nullptr;
  cacheMemory_.end();
  LogI("FONT", "cache=psram released");
}

SdBitmapFont::CacheSlot* SdBitmapFont::glyphSet(uint32_t codepoint) {
  CacheSlot* slots = psramCache_ ? psramCache_->glyphs : cache_;
  const uint16_t count = psramCache_ ? kPsramGlyphSlots : kCacheSlots;
  return slots + (codepoint % (count / kGlyphWays)) * kGlyphWays;
}

void SdBitmapFont::closeCurrent() {
  if (state_ != State::Closed) {
    LogI("FONT", "close size=%u index_hit=%lu miss=%lu glyph_hit=%lu miss=%lu common_hit=%lu miss=%lu",
         metrics_.pixelSize, static_cast<unsigned long>(cacheStats_.indexHits),
         static_cast<unsigned long>(cacheStats_.indexMisses),
         static_cast<unsigned long>(cacheStats_.glyphHits),
         static_cast<unsigned long>(cacheStats_.glyphMisses),
             static_cast<unsigned long>(cacheStats_.commonHits),
             static_cast<unsigned long>(cacheStats_.commonMisses));
  }
  file_.close();
  commonFile_.close();
  checkPhase_ = CheckPhase::Font;
  commonCount_ = 0;
  ++session_;
  metrics_ = Metrics{};
  state_ = State::Closed;
  error_ = Error::None;
  clearCache();
  changed_ = true;
}

void SdBitmapFont::clearVerified() {
  memset(verifiedHeaders_, 0, sizeof(verifiedHeaders_));
  nextVerified_ = 0;
}

void SdBitmapFont::end() {
  closeCurrent();
  releaseCache();
  clearVerified();
}

bool SdBitmapFont::fail(Error error) {
  LogW("FONT", "error=%u size=%u offset=%lu; close file, OK retries",
       static_cast<unsigned>(error), metrics_.pixelSize,
       static_cast<unsigned long>(verifyOffset_));
  file_.close();
  commonFile_.close();
  checkPhase_ = CheckPhase::Font;
  commonCount_ = 0;
  clearCache();
  releaseCache();
  clearVerified();
  state_ = State::Error;
  error_ = error;
  changed_ = true;
  return false;
}

bool SdBitmapFont::open(uint16_t pixelSize, bool useExternalCache) {
  closeCurrent();
  if (!useExternalCache) releaseCache();
  metrics_.pixelSize = pixelSize;
  verifyOffset_ = 0;
  char path[32];
  snprintf(path, sizeof(path), "/Font/font-%u.bin", pixelSize);
  LogI("FONT", "open %s", path);
  if (!file_.open(path)) return fail(Error::OpenFailed);
  uint8_t* header = header_;
  if (!file_.readAt(0, header, sizeof(header_))) return fail(Error::Header);
  if (memcmp(header, "NBFT", 4) != 0 || le16(header + 4) != 1U ||
      le16(header + 6) != 64U || le16(header + 8) != kRecordBytes ||
      le16(header + 18) != 1U) return fail(Error::Header);
  const uint16_t size = le16(header + 10);
  if (size < 6U || size > 128U) return fail(Error::Header);
  if (size != pixelSize) return fail(Error::SizeMismatch);
  for (uint8_t i = 40; i < sizeof(header_); ++i) {
    if (header[i] != 0U) return fail(Error::Header);
  }
  metrics_.ascent = static_cast<int16_t>(le16(header + 12));
  metrics_.descent = static_cast<int16_t>(le16(header + 14));
  metrics_.lineGap = static_cast<int16_t>(le16(header + 16));
  metrics_.glyphCount = le32(header + 20);
  bitmapOffset_ = le32(header + 28);
  if (metrics_.ascent < 0 || metrics_.descent < 0 || metrics_.lineGap < 0 ||
      metrics_.lineHeight() == 0U || metrics_.glyphCount == 0U ||
      metrics_.glyphCount > 0x110000U - 0x800U || le32(header + 24) != 64U ||
      bitmapOffset_ != 64U + metrics_.glyphCount * kRecordBytes ||
      le32(header + 32) != file_.size() || bitmapOffset_ > file_.size())
    return fail(Error::Header);
  expectedChecksum_ = le32(header + 36);
  if (useExternalCache) beginCache();
  for (const auto& verified : verifiedHeaders_) {
    if (memcmp(verified, header, sizeof(header_)) == 0) {
      LogD("FONT", "size=%u CRC32=reused (same SD session/header)", pixelSize);
      beginCommonIndex();
      return true;
    }
  }
  checksum_ = 0xFFFFFFFFU;
  verifyOffset_ = 64U;
  verifyStartMs_ = millis();
  state_ = State::Checking;
  changed_ = true;
  return true;
}

void SdBitmapFont::tick() {
  if (state_ != State::Checking) return;
  if (checkPhase_ == CheckPhase::CommonHeader) { beginCommonIndex(); return; }
  if (checkPhase_ == CheckPhase::CommonData) { tickCommonIndex(); return; }
  uint8_t block[1024];
  const uint32_t startMs = millis();
  // Yield after 4 ms or 32 KiB. A single blocking SD read can exceed the time budget.
  for (uint8_t i = 0; i < 32U && verifyOffset_ < file_.size(); ++i) {
    const uint32_t left = file_.size() - verifyOffset_;
    const size_t length = left < sizeof(block) ? left : sizeof(block);
    if (!file_.readAt(verifyOffset_, block, length)) { fail(Error::Read); return; }
    checksum_ = crc32(checksum_, block, length);
    verifyOffset_ += length;
    if (millis() - startMs >= 4U) break;
  }
  if (verifyOffset_ != file_.size()) return;
  if ((checksum_ ^ 0xFFFFFFFFU) != expectedChecksum_) { fail(Error::Crc); return; }
  memcpy(verifiedHeaders_[nextVerified_], header_, sizeof(header_));
  nextVerified_ = static_cast<uint8_t>((nextVerified_ + 1U) % kVerifiedFiles);
  LogI("FONT", "size=%u glyphs=%lu bytes=%lu CRC32=OK verify_ms=%lu", metrics_.pixelSize,
       static_cast<unsigned long>(metrics_.glyphCount),
       static_cast<unsigned long>(file_.size()),
       static_cast<unsigned long>(millis() - verifyStartMs_));
  // Read the optional header on the next tick, keeping this tick within its
  // byte/time budget even when the NBFT payload ends exactly on a block limit.
  checkPhase_ = CheckPhase::CommonHeader;
}

void SdBitmapFont::disableCommonIndex(const char* reason) {
  commonFile_.close();
  checkPhase_ = CheckPhase::Font;
  commonCount_ = 0;
  // The optional index never makes an otherwise valid NBFT unusable.
  state_ = State::Ready;
  changed_ = true;
  LogI("FONT", "common=off size=%u reason=%s; full-font lookup", metrics_.pixelSize, reason);
  (void)reason;
}

void SdBitmapFont::beginCommonIndex() {
  if (!psramCache_) { disableCommonIndex("no PSRAM cache"); return; }
  char path[40];
  snprintf(path, sizeof(path), "/Font/font-%u.common.bin", metrics_.pixelSize);
  if (!commonFile_.open(path)) { disableCommonIndex("not found/open failed"); return; }
  uint8_t header[kCommonHeaderBytes];
  if (!commonFile_.readAt(0, header, sizeof(header))) {
    disableCommonIndex("header read"); return;
  }
  commonExpectedCount_ = le32(header + 12);
  if (memcmp(header, "NBCI", 4) != 0 || le16(header + 4) != 1U ||
      le16(header + 6) != kCommonHeaderBytes || le16(header + 8) != kCommonRecordBytes ||
      le16(header + 10) != metrics_.pixelSize || commonExpectedCount_ == 0U ||
      commonExpectedCount_ > kMaxCommonGlyphs || commonExpectedCount_ > metrics_.glyphCount ||
      le32(header + 16) != commonFile_.size() ||
      commonFile_.size() != kCommonHeaderBytes + commonExpectedCount_ * kCommonRecordBytes) {
    disableCommonIndex("format/length/count (max 7500)"); return;
  }
  for (uint8_t i = 24; i < 32U; ++i) {
    if (header[i] != 0U) { disableCommonIndex("reserved bits"); return; }
  }
  if (memcmp(header + 32, header_, sizeof(header_)) != 0) {
    disableCommonIndex("font binding mismatch; copy matching pair"); return;
  }
  commonExpectedChecksum_ = le32(header + 20);
  commonLoaded_ = 0;
  commonChecksum_ = 0xFFFFFFFFU;
  checkPhase_ = CheckPhase::CommonData;
  state_ = State::Checking;
  changed_ = true;
  LogI("FONT", "common=loading size=%u count=%lu", metrics_.pixelSize,
       static_cast<unsigned long>(commonExpectedCount_));
}

void SdBitmapFont::tickCommonIndex() {
  const uint32_t startMs = millis();
  constexpr uint16_t kBlockEntries = 1024U / kCommonRecordBytes;
  for (uint8_t block = 0; block < 32U && commonLoaded_ < commonExpectedCount_; ++block) {
    const uint32_t left = commonExpectedCount_ - commonLoaded_;
    const uint32_t count = left < kBlockEntries ? left : kBlockEntries;
    uint8_t* data = psramCache_->common + commonLoaded_ * kCommonRecordBytes;
    const size_t length = count * kCommonRecordBytes;
    if (!commonFile_.readAt(kCommonHeaderBytes + commonLoaded_ * kCommonRecordBytes,
                            data, length)) { disableCommonIndex("payload read"); return; }
    commonChecksum_ = crc32(commonChecksum_, data, length);
    for (uint32_t i = 0; i < count; ++i) {
      const uint8_t* entry = data + i * kCommonRecordBytes;
      const uint32_t cp = le32(entry), offset = le32(entry + 4);
      const bool hasPrevious = commonLoaded_ + i != 0U;
      if (!scalar(cp) || offset < 64U || offset > bitmapOffset_ - kRecordBytes ||
          (offset - 64U) % kRecordBytes != 0U ||
          (hasPrevious && (cp <= le32(entry - kCommonRecordBytes) ||
                          offset <= le32(entry - kCommonRecordBytes + 4)))) {
        disableCommonIndex("entry order/scalar/offset"); return;
      }
    }
    commonLoaded_ += count;
    if (millis() - startMs >= 4U) break;
  }
  if (commonLoaded_ != commonExpectedCount_) return;
  if ((commonChecksum_ ^ 0xFFFFFFFFU) != commonExpectedChecksum_) {
    disableCommonIndex("CRC32 mismatch"); return;
  }
  commonFile_.close();
  checkPhase_ = CheckPhase::Font;
  commonCount_ = commonLoaded_;
  state_ = State::Ready;
  changed_ = true;
  LogI("FONT", "common=ready size=%u count=%lu index_bytes=%lu cache_bytes=%u", metrics_.pixelSize,
       static_cast<unsigned long>(commonCount_),
       static_cast<unsigned long>(commonCount_ * kCommonRecordBytes),
       static_cast<unsigned>(cacheMemory_.size()));
}

bool SdBitmapFont::commonRecord(uint32_t codepoint, uint32_t& recordIndex) const {
  uint32_t low = 0, high = commonCount_;
  while (low < high) {
    const uint32_t mid = low + (high - low) / 2U;
    const uint8_t* entry = psramCache_->common + mid * kCommonRecordBytes;
    const uint32_t cp = le32(entry);
    if (cp < codepoint) low = mid + 1U;
    else if (cp > codepoint) high = mid;
    else { recordIndex = (le32(entry + 4) - 64U) / kRecordBytes; return true; }
  }
  return false;
}

bool SdBitmapFont::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

bool SdBitmapFont::readRecord(uint32_t index, Glyph& glyph) {
  const uint32_t blockStart = index / kIndexBlockRecords * kIndexBlockRecords;
  uint32_t* cachedStart = &indexBlockStart_;
  uint8_t* records = indexBlock_;
  if (psramCache_) {
    auto& block = psramCache_->index[(blockStart / kIndexBlockRecords) % kPsramIndexBlocks];
    cachedStart = &block.start;
    records = block.records;
  }
  if (*cachedStart != blockStart) {
    ++cacheStats_.indexMisses;
    const uint32_t remaining = metrics_.glyphCount - blockStart;
    const uint32_t count = remaining < kIndexBlockRecords ? remaining : kIndexBlockRecords;
    if (!file_.readAt(64U + blockStart * kRecordBytes, records, count * kRecordBytes))
      return fail(Error::Read);
    *cachedStart = blockStart;
  } else {
    ++cacheStats_.indexHits;
  }
  const uint8_t* p = records + (index - blockStart) * kRecordBytes;
  glyph.codepoint = le32(p);
  glyph.session = session_;
  glyph.offset = le32(p + 4);
  glyph.width = le16(p + 8);
  glyph.height = le16(p + 10);
  glyph.xOffset = static_cast<int16_t>(le16(p + 12));
  glyph.yOffset = static_cast<int16_t>(le16(p + 14));
  glyph.advance = static_cast<int16_t>(le16(p + 16));
  glyph.length = le32(p + 20);
  const uint32_t bitmapBytes = file_.size() - bitmapOffset_;
  if (!scalar(glyph.codepoint) || le16(p + 18) != 0U || glyph.advance < 0 ||
      glyph.width > 1024U || glyph.height > 1024U ||
      ((glyph.width == 0U) != (glyph.height == 0U)) ||
      glyph.length != ((glyph.width + 7U) / 8U) * glyph.height ||
      glyph.offset > bitmapBytes || glyph.length > bitmapBytes - glyph.offset)
    return fail(Error::Glyph);
  return true;
}

bool SdBitmapFont::glyph(uint32_t codepoint, Glyph& out) {
  out = Glyph{};
  if (state_ != State::Ready || !scalar(codepoint)) return false;
  CacheSlot* slots = glyphSet(codepoint);
  for (uint8_t i = 0; i < kGlyphWays; ++i) {
    const auto& slot = slots[i];
    if (slot.valid && slot.glyph.codepoint == codepoint) {
      ++cacheStats_.glyphHits;
      out = slot.glyph;
      return slot.found;
    }
  }
  ++cacheStats_.glyphMisses;
  uint32_t low = 0, high = metrics_.glyphCount;
  Glyph candidate;
  bool found = false;
  uint32_t recordIndex = 0;
  if (commonCount_ != 0U) {
    if (commonRecord(codepoint, recordIndex)) {
      if (!readRecord(recordIndex, candidate)) return false;
      if (candidate.codepoint == codepoint) {
        found = true;
        ++cacheStats_.commonHits;
      } else {
        disableCommonIndex("target codepoint mismatch");
      }
    } else {
      ++cacheStats_.commonMisses;
    }
  }
  while (!found && low < high) {
    const uint32_t mid = low + (high - low) / 2U;
    if (!readRecord(mid, candidate)) return false;
    if (candidate.codepoint < codepoint) low = mid + 1U;
    else if (candidate.codepoint > codepoint) high = mid;
    else { found = true; break; }
  }
  uint8_t selected = nextSlot_;
  for (uint8_t i = 0; i < kGlyphWays; ++i) {
    if (!slots[i].valid) { selected = i; break; }
  }
  auto& slot = slots[selected];
  nextSlot_ = static_cast<uint8_t>((nextSlot_ + 1U) % kGlyphWays);
  slot.valid = false;
  slot.found = found;
  slot.glyph = found ? candidate : Glyph{};
  slot.glyph.codepoint = codepoint;
  if (found && candidate.length <= kGlyphCacheBytes) {
    if (!file_.readAt(bitmapOffset_ + candidate.offset, slot.bitmap, candidate.length))
      return fail(Error::Read);
    const uint16_t stride = (candidate.width + 7U) / 8U;
    for (uint16_t row = 0; row < candidate.height; ++row) {
      if (!validateRow(candidate, slot.bitmap + row * stride)) return fail(Error::Glyph);
    }
  }
  slot.valid = true;
  out = slot.glyph;
  return found;
}

bool SdBitmapFont::validateRow(const Glyph& glyph, const uint8_t* data) const {
  const uint16_t stride = (glyph.width + 7U) / 8U;
  const uint8_t extra = glyph.width % 8U;
  return extra == 0U || (data[stride - 1U] & ((1U << (8U - extra)) - 1U)) == 0U;
}

bool SdBitmapFont::readRow(const Glyph& glyph, uint16_t row, uint8_t* data, size_t capacity) {
  const uint16_t stride = (glyph.width + 7U) / 8U;
  if (state_ != State::Ready || glyph.session != session_ || !data ||
      glyph.width > 1024U || glyph.height > 1024U || row >= glyph.height ||
      stride > capacity || glyph.length != uint32_t(stride) * glyph.height ||
      glyph.offset > file_.size() - bitmapOffset_ ||
      glyph.length > file_.size() - bitmapOffset_ - glyph.offset) return false;
  const CacheSlot* slots = glyphSet(glyph.codepoint);
  for (uint8_t i = 0; i < kGlyphWays; ++i) {
    const auto& slot = slots[i];
    if (slot.valid && slot.found && slot.glyph.codepoint == glyph.codepoint &&
        glyph.length <= kGlyphCacheBytes && slot.glyph.offset == glyph.offset &&
        slot.glyph.width == glyph.width && slot.glyph.height == glyph.height &&
        slot.glyph.length == glyph.length) {
      memcpy(data, slot.bitmap + row * stride, stride);
      return true;
    }
  }
  if (!file_.readAt(bitmapOffset_ + glyph.offset + row * stride, data, stride))
    return fail(Error::Read);
  return validateRow(glyph, data) ? true : fail(Error::Glyph);
}
