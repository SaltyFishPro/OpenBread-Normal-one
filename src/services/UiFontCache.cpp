#include "UiFontCache.h"

#include <Arduino.h>
#include <cstring>
#include "UiFontCodepoints.h"

#ifndef OB_SD_FONT_LOG_ENABLED
#define OB_SD_FONT_LOG_ENABLED 1
#endif
#if OB_SD_FONT_LOG_ENABLED
#define UI_FONT_LOG(...) do { if (Serial) Serial.printf("[FONT] ui " __VA_ARGS__); } while (0)
#else
#define UI_FONT_LOG(...) ((void)0)
#endif

namespace {
constexpr size_t kCount = sizeof(UiFontCodepoints::kValues) / sizeof(uint32_t);
static_assert(sizeof(UiFontCache::Glyph) * kCount <= UiFontCache::kBudgetBytes,
              "Fixed UI glyph cache exceeds its bounded PSRAM budget");
constexpr char kSubsetPath[] = "/Font/font-14.ui.bin";
constexpr size_t kSubsetChunkBytes = 8U * 1024U;
constexpr size_t kSubsetMaxBytes = 128U * 1024U;
constexpr size_t kParseBatchRecords = 64U;

uint16_t le16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8U); }
uint32_t le32(const uint8_t* p) { return le16(p) | (uint32_t(le16(p + 2)) << 16U); }
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

bool UiFontCache::begin() {
  if (state_ == State::Ready) return true;
  end();
  state_ = State::Loading;
  startedMs_ = millis();
  if (beginSubset()) {
    UI_FONT_LOG("loading %s glyphs=%u\n", kSubsetPath, static_cast<unsigned>(kCount));
    return true;
  }
  // Reuse the bounded index/common cache during extraction. With one index
  // block, every binary-search level re-reads SD and startup can take a minute.
  // source_.end() releases this temporary cache before the UI becomes Ready.
  UI_FONT_LOG("loading /Font/font-14.bin glyphs=%u (no usable ui subset)\n",
              static_cast<unsigned>(kCount));
  if (!source_.open(14)) { fail("open font"); return false; }
  return true;
}

void UiFontCache::fail(const char* reason) {
  UI_FONT_LOG("unavailable reason=%s source_error=%u loaded=%u/%u elapsed_ms=%lu; retry on next home entry\n",
              reason, static_cast<unsigned>(source_.error()), static_cast<unsigned>(next_),
              static_cast<unsigned>(kCount), static_cast<unsigned long>(millis() - startedMs_));
  freeSubset();
  source_.end();
  memory_.end();
  glyphs_ = nullptr;
  state_ = State::Error;
  changed_ = true;
}

void UiFontCache::tick() {
  if (state_ != State::Loading) return;
  if (millis() - startedMs_ >= 120000U) { fail("load timeout"); return; }
  if (subsetActive_) { tickSubset(); return; }
  if (source_.state() == SdBitmapFont::State::Checking) { source_.tick(); return; }
  if (source_.state() != SdBitmapFont::State::Ready || source_.metrics().lineHeight() > 32) {
    fail("font state or line height"); return;
  }
  if (!glyphs_) {
    if (!memory_.allocate(kCount * sizeof(Glyph))) { fail("UI PSRAM allocation"); return; }
    glyphs_ = static_cast<Glyph*>(memory_.data());
    UI_FONT_LOG("extract glyphs=%u source_cache=%u common=%lu\n", static_cast<unsigned>(kCount),
                static_cast<unsigned>(source_.cacheBytes()),
                static_cast<unsigned long>(source_.commonGlyphCount()));
  }
  const uint32_t start = millis();
  const size_t previous = next_;
  uint8_t processed = 0;
  do {
    Glyph& cached = glyphs_[next_];
    cached = {};
    SdBitmapFont::Glyph glyph;
    if (source_.glyph(UiFontCodepoints::kValues[next_], glyph)) {
      if (glyph.length > sizeof(cached.bitmap) || glyph.width > 32 || glyph.height > 32) {
        fail("glyph exceeds UI bounds"); return;
      }
      cached.width = glyph.width; cached.height = glyph.height;
      cached.xOffset = glyph.xOffset; cached.yOffset = glyph.yOffset;
      cached.advance = glyph.advance; cached.length = glyph.length;
      const size_t stride = (glyph.width + 7U) / 8U;
      for (uint16_t row = 0; stride && row < glyph.height; ++row) {
        if (!source_.readRow(glyph, row, cached.bitmap + row * stride,
                             sizeof(cached.bitmap) - row * stride)) { fail("glyph read"); return; }
      }
    } else {
      if (source_.state() != SdBitmapFont::State::Ready) { fail("glyph lookup"); return; }
      cached.advance = 14;
      cached.length = UINT16_MAX;  // Source font has no glyph; use a visible box.
    }
    ++next_;
    ++processed;
  } while (next_ < kCount && processed < 8 && millis() - start < 4U);
  if (previous / 128 != next_ / 128)
    UI_FONT_LOG("progress=%u/%u elapsed_ms=%lu\n", static_cast<unsigned>(next_),
                static_cast<unsigned>(kCount), static_cast<unsigned long>(millis() - startedMs_));
  if (next_ != kCount) return;
  source_.end();
  state_ = State::Ready;
  changed_ = true;
  UI_FONT_LOG("ready glyphs=%u bytes=%u elapsed_ms=%lu; SD closed, render reads=0\n",
              static_cast<unsigned>(kCount), static_cast<unsigned>(cacheBytes()),
              static_cast<unsigned long>(millis() - startedMs_));
}

bool UiFontCache::beginSubset() {
  if (!subsetFile_.open(kSubsetPath)) return false;
  uint8_t header[64];
  if (!subsetFile_.readAt(0, header, sizeof(header))) {
    subsetFile_.close();
    return false;
  }
  const uint32_t count = le32(header + 20);
  const uint32_t indexOffset = le32(header + 24);
  const uint32_t bitmapOffset = le32(header + 28);
  const uint32_t fileSize = le32(header + 32);
  const uint32_t expectedCount = le32(header + 40);
  const int16_t ascent = static_cast<int16_t>(le16(header + 12));
  const int16_t descent = static_cast<int16_t>(le16(header + 14));
  const int16_t lineGap = static_cast<int16_t>(le16(header + 16));
  if (memcmp(header, "NBFT", 4) != 0 || le16(header + 4) != 1U ||
      le16(header + 6) != 64U || le16(header + 8) != 24U ||
      le16(header + 10) != 14U || le16(header + 18) != 1U ||
      ascent < 0 || descent < 0 || lineGap < 0 ||
      ascent + descent + lineGap > 32 || count < 1U || count > kCount ||
      expectedCount != kCount || indexOffset != 64U ||
      bitmapOffset != 64U + count * 24U || fileSize != subsetFile_.size() ||
      fileSize < bitmapOffset || fileSize - 64U > kSubsetMaxBytes) {
    UI_FONT_LOG("subset header rejected; fallback full font\n");
    subsetFile_.close();
    return false;
  }
  for (size_t i = 44; i < 64; ++i) {
    if (header[i] != 0U) {
      subsetFile_.close();
      return false;
    }
  }
  subsetSize_ = fileSize - 64U;
  if (!subsetMemory_.allocate(subsetSize_)) {
    UI_FONT_LOG("subset staging PSRAM unavailable; fallback full font\n");
    subsetFile_.close();
    return false;
  }
  subsetData_ = static_cast<uint8_t*>(subsetMemory_.data());
  subsetCount_ = static_cast<uint16_t>(count);
  subsetExpectedCrc_ = le32(header + 36);
  subsetLoaded_ = 0;
  subsetParsed_ = 0;
  subsetCursor_ = 0;
  subsetPrevCp_ = 0;
  subsetCrc_ = 0xFFFFFFFFU;
  subsetCrcOk_ = false;
  subsetActive_ = true;
  UI_FONT_LOG("subset open glyphs=%u payload=%lu bytes\n", static_cast<unsigned>(count),
              static_cast<unsigned long>(subsetSize_));
  return true;
}

void UiFontCache::tickSubset() {
  const uint32_t start = millis();
  if (subsetLoaded_ < subsetSize_) {
    do {
      const uint32_t remaining = subsetSize_ - subsetLoaded_;
      const uint32_t chunk = remaining < kSubsetChunkBytes ? remaining : kSubsetChunkBytes;
      if (!subsetFile_.readAt(64U + subsetLoaded_, subsetData_ + subsetLoaded_, chunk)) {
        fallbackToFullFont("subset read");
        return;
      }
      subsetCrc_ = crc32(subsetCrc_, subsetData_ + subsetLoaded_, chunk);
      subsetLoaded_ += chunk;
    } while (subsetLoaded_ < subsetSize_ && millis() - start < 4U);
    if (subsetLoaded_ < subsetSize_) return;
    subsetFile_.close();
    if ((subsetCrc_ ^ 0xFFFFFFFFU) != subsetExpectedCrc_) {
      fallbackToFullFont("subset CRC");
      return;
    }
    subsetCrcOk_ = true;
    UI_FONT_LOG("subset CRC ok glyphs=%u bytes=%lu\n", static_cast<unsigned>(subsetCount_),
                static_cast<unsigned long>(subsetSize_));
  }
  if (!glyphs_) {
    if (!memory_.allocate(kCount * sizeof(Glyph))) { fail("UI PSRAM allocation"); return; }
    glyphs_ = static_cast<Glyph*>(memory_.data());
    for (size_t i = 0; i < kCount; ++i) {
      glyphs_[i] = {};
      glyphs_[i].advance = 14;
      glyphs_[i].length = UINT16_MAX;  // Source font has no glyph; visible box.
    }
  }
  const size_t indexBytes = size_t(subsetCount_) * 24U;
  const size_t bitmapBytes = subsetSize_ - indexBytes;
  size_t processed = 0;
  do {
    if (subsetParsed_ >= subsetCount_) { finishSubset(); return; }
    const uint8_t* p = subsetData_ + subsetParsed_ * 24U;
    const uint32_t cp = le32(p);
    const uint32_t offset = le32(p + 4);
    const uint16_t width = le16(p + 8);
    const uint16_t height = le16(p + 10);
    const int16_t xOffset = static_cast<int16_t>(le16(p + 12));
    const int16_t yOffset = static_cast<int16_t>(le16(p + 14));
    const int16_t advance = static_cast<int16_t>(le16(p + 16));
    const uint32_t length = le32(p + 20);
    if (le16(p + 18) != 0U || cp <= subsetPrevCp_ ||
        ((width == 0U) != (height == 0U)) || width > 32U || height > 32U ||
        length > 64U || length != uint32_t((width + 7U) / 8U) * height ||
        offset > bitmapBytes || length > bitmapBytes - offset) {
      fallbackToFullFont("subset record bounds");
      return;
    }
    while (subsetCursor_ < kCount && UiFontCodepoints::kValues[subsetCursor_] < cp) ++subsetCursor_;
    if (subsetCursor_ >= kCount || UiFontCodepoints::kValues[subsetCursor_] != cp) {
      fallbackToFullFont("subset stale codepoints");
      return;
    }
    Glyph& cached = glyphs_[subsetCursor_];
    cached.width = width;
    cached.height = height;
    cached.xOffset = xOffset;
    cached.yOffset = yOffset;
    cached.advance = advance;
    cached.length = static_cast<uint16_t>(length);
    if (length != 0U) memcpy(cached.bitmap, subsetData_ + indexBytes + offset, length);
    subsetPrevCp_ = cp;
    ++subsetCursor_;
    ++subsetParsed_;
    ++processed;
  } while (subsetParsed_ < subsetCount_ && processed < kParseBatchRecords &&
           millis() - start < 4U);
  next_ = subsetParsed_;
}

void UiFontCache::finishSubset() {
  const uint32_t elapsedMs = millis() - startedMs_;
  const size_t bytes = cacheBytes();
  freeSubset();
  state_ = State::Ready;
  changed_ = true;
  UI_FONT_LOG("ready glyphs=%u bytes=%u elapsed_ms=%lu; source=ui subset, SD closed, render reads=0\n",
              static_cast<unsigned>(kCount), static_cast<unsigned>(bytes),
              static_cast<unsigned long>(elapsedMs));
}

void UiFontCache::fallbackToFullFont(const char* reason) {
  UI_FONT_LOG("subset unavailable reason=%s; fallback full font\n", reason);
  freeSubset();
  if (!source_.open(14)) fail("open font");
}

void UiFontCache::freeSubset() {
  subsetFile_.close();
  subsetMemory_.end();
  subsetData_ = nullptr;
  subsetSize_ = 0;
  subsetLoaded_ = 0;
  subsetParsed_ = 0;
  subsetCount_ = 0;
  subsetCursor_ = 0;
  subsetPrevCp_ = 0;
  subsetCrc_ = 0xFFFFFFFFU;
  subsetExpectedCrc_ = 0;
  subsetActive_ = false;
  subsetCrcOk_ = false;
}

const UiFontCache::Glyph* UiFontCache::glyph(uint32_t cp) const {
  if (state_ != State::Ready) return nullptr;
  size_t lo = 0, hi = kCount;
  while (lo < hi) {
    const size_t middle = lo + (hi - lo) / 2;
    if (UiFontCodepoints::kValues[middle] < cp) lo = middle + 1;
    else hi = middle;
  }
  return lo < kCount && UiFontCodepoints::kValues[lo] == cp ? glyphs_ + lo : nullptr;
}

void UiFontCache::cancelLoad() {
  if (state_ != State::Loading) return;
  UI_FONT_LOG("cancel loaded=%u/%u elapsed_ms=%lu\n", static_cast<unsigned>(next_),
              static_cast<unsigned>(kCount), static_cast<unsigned long>(millis() - startedMs_));
  end();
}
void UiFontCache::end() {
  freeSubset();
  source_.end(); memory_.end(); glyphs_ = nullptr; next_ = 0;
  state_ = State::Idle;
}
bool UiFontCache::consumeChanged() { const bool result = changed_; changed_ = false; return result; }
