#include "SdBitmapFont.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#ifndef OB_SD_FONT_LOG_ENABLED
#define OB_SD_FONT_LOG_ENABLED 1
#endif
#if OB_SD_FONT_LOG_ENABLED
#define FONT_LOG(...) do { if (Serial) Serial.printf("[FONT] " __VA_ARGS__); } while (0)
#else
#define FONT_LOG(...) ((void)0)
#endif

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
}

void SdBitmapFont::end() {
  if (state_ != State::Closed) FONT_LOG("close size=%u\n", metrics_.pixelSize);
  file_.close();
  ++session_;
  metrics_ = Metrics{};
  state_ = State::Closed;
  error_ = Error::None;
  clearCache();
  changed_ = true;
}

bool SdBitmapFont::fail(Error error) {
  FONT_LOG("error=%u size=%u offset=%lu; close file, OK retries\n",
           static_cast<unsigned>(error), metrics_.pixelSize,
           static_cast<unsigned long>(verifyOffset_));
  file_.close();
  clearCache();
  state_ = State::Error;
  error_ = error;
  changed_ = true;
  return false;
}

bool SdBitmapFont::open(uint16_t pixelSize) {
  end();
  metrics_.pixelSize = pixelSize;
  verifyOffset_ = 0;
  char path[32];
  snprintf(path, sizeof(path), "/Font/font-%u.bin", pixelSize);
  FONT_LOG("open %s\n", path);
  if (!file_.open(path)) return fail(Error::OpenFailed);
  uint8_t header[64];
  if (!file_.readAt(0, header, sizeof(header))) return fail(Error::Header);
  if (memcmp(header, "NBFT", 4) != 0 || le16(header + 4) != 1U ||
      le16(header + 6) != 64U || le16(header + 8) != kRecordBytes ||
      le16(header + 18) != 1U) return fail(Error::Header);
  const uint16_t size = le16(header + 10);
  if (size < 6U || size > 128U) return fail(Error::Header);
  if (size != pixelSize) return fail(Error::SizeMismatch);
  for (uint8_t i = 40; i < sizeof(header); ++i) {
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
  checksum_ = 0xFFFFFFFFU;
  verifyOffset_ = 64U;
  state_ = State::Checking;
  changed_ = true;
  return true;
}

void SdBitmapFont::tick() {
  if (state_ != State::Checking) return;
  uint8_t block[1024];
  // At most 8 KiB each tick; button polling/Left cancellation continue between ticks.
  for (uint8_t i = 0; i < 8U && verifyOffset_ < file_.size(); ++i) {
    const uint32_t left = file_.size() - verifyOffset_;
    const size_t length = left < sizeof(block) ? left : sizeof(block);
    if (!file_.readAt(verifyOffset_, block, length)) { fail(Error::Read); return; }
    checksum_ = crc32(checksum_, block, length);
    verifyOffset_ += length;
  }
  if (verifyOffset_ != file_.size()) return;
  if ((checksum_ ^ 0xFFFFFFFFU) != expectedChecksum_) { fail(Error::Crc); return; }
  state_ = State::Ready;
  changed_ = true;
  FONT_LOG("ready size=%u glyphs=%lu bytes=%lu CRC32=OK\n", metrics_.pixelSize,
           static_cast<unsigned long>(metrics_.glyphCount),
           static_cast<unsigned long>(file_.size()));
}

bool SdBitmapFont::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

bool SdBitmapFont::readRecord(uint32_t index, Glyph& glyph) {
  const uint32_t blockStart = index / kIndexBlockRecords * kIndexBlockRecords;
  if (indexBlockStart_ != blockStart) {
    const uint32_t remaining = metrics_.glyphCount - blockStart;
    const uint32_t count = remaining < kIndexBlockRecords ? remaining : kIndexBlockRecords;
    if (!file_.readAt(64U + blockStart * kRecordBytes, indexBlock_, count * kRecordBytes))
      return fail(Error::Read);
    indexBlockStart_ = blockStart;
  }
  const uint8_t* p = indexBlock_ + (index - blockStart) * kRecordBytes;
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
  for (const auto& slot : cache_) {
    if (slot.valid && slot.glyph.codepoint == codepoint) {
      out = slot.glyph;
      return slot.found;
    }
  }
  uint32_t low = 0, high = metrics_.glyphCount;
  Glyph candidate;
  bool found = false;
  while (low < high) {
    const uint32_t mid = low + (high - low) / 2U;
    if (!readRecord(mid, candidate)) return false;
    if (candidate.codepoint < codepoint) low = mid + 1U;
    else if (candidate.codepoint > codepoint) high = mid;
    else { found = true; break; }
  }
  auto& slot = cache_[nextSlot_];
  nextSlot_ = static_cast<uint8_t>((nextSlot_ + 1U) % kCacheSlots);
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
  for (const auto& slot : cache_) {
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
