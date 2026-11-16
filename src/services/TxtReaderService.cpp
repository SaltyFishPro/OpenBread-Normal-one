#include "TxtReaderService.h"
#include "SdCardService.h"
#include "../log/Log.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>

namespace {
constexpr char kDirectory[] = "/Books";
constexpr uint32_t kParagraph = 0x80000000U;
uint64_t pathHash(const char* value) {
  uint64_t hash = 14695981039346656037ULL;
  for (; *value; ++value) hash = (hash ^ static_cast<uint8_t>(*value)) * 1099511628211ULL;
  return hash;
}
bool isTxt(const char* name) {
  const size_t n = strlen(name);
  return n > 4 && name[n-4] == '.' && tolower(name[n-3]) == 't' &&
         tolower(name[n-2]) == 'x' && tolower(name[n-1]) == 't';
}
}

void TxtReaderService::title(const Book& book, char* out, size_t capacity) {
  const char* name = strrchr(book.path, '/');
  snprintf(out, capacity, "%s", name ? name + 1 : book.path);
  const size_t n = strlen(out);
  if (n >= 4 && isTxt(out)) out[n-4] = 0;
}
void TxtReaderService::loadHistory() {
  saved_ = Saved{};
  Preferences prefs;
  if (prefs.begin("txt-reader", true)) {
    Saved stored;
    if (prefs.getBytesLength("history") == sizeof(stored) &&
        prefs.getBytes("history", &stored, sizeof(stored)) == sizeof(stored) &&
        stored.magic == saved_.magic) saved_ = stored;
    prefs.end();
  }
  dark_ = saved_.dark != 0;
}
void TxtReaderService::saveHistory() {
  if (!current_.pathHash) return;
  if (pageReady_) current_.page = wanted_;
  uint8_t slot = 0;
  for (; slot < 7 && saved_.entries[slot].pathHash != current_.pathHash; ++slot) {}
  for (uint8_t i = slot; i > 0; --i) saved_.entries[i] = saved_.entries[i-1];
  saved_.entries[0] = current_;
  saved_.dark = dark_;
  Preferences prefs;
  bool ok = prefs.begin("txt-reader", false);
  if (ok) { ok = prefs.putBytes("history", &saved_, sizeof(saved_)) == sizeof(saved_); prefs.end(); }
  if (!ok) LogE("READER", "reading position save failed");
  books_[active_].page = current_.page;
}
void TxtReaderService::closeBook() {
  if (file_) saveHistory();
  file_.close();
  indexMemory_.end(); offsets_ = nullptr;
  current_ = History{};
  bufferStart_ = UINT32_MAX; bufferSize_ = 0;
  pageReady_ = indexed_ = false;
  if (state_ == State::Reading) state_ = State::Shelf;
  changed_ = true;
}
void TxtReaderService::end() { closeBook(); state_ = State::Closed; }
void TxtReaderService::fail(const char* message, const char* stage) {
  LogE("READER", "failure stage=%s", stage);
  closeBook(); error_ = message; state_ = State::Error; changed_ = true;
}
void TxtReaderService::scan(SdCardService& sd) {
  closeBook(); count_ = 0; loadHistory();
  if (sd.refresh() != SdCardService::Status::Ready) {
    state_ = State::Missing; changed_ = true; LogE("READER", "SD unavailable"); return;
  }
  if (!SD_MMC.exists(kDirectory) && !SD_MMC.mkdir(kDirectory)) {
    fail("无法创建 /Books，请检查 SD 卡", "mkdir"); return;
  }
  File dir = SD_MMC.open(kDirectory);
  if (!dir || !dir.isDirectory()) { fail("无法打开 /Books 文件夹", "directory"); return; }
  bool overflow = false;
  while (File item = dir.openNextFile()) {
    const char* name = item.name();
    if (!item.isDirectory() && isTxt(name)) {
      if (count_ == kMaxBooks) overflow = true;
      else {
        Book book;
        const int n = name[0] == '/' ? snprintf(book.path, sizeof(book.path), "%s", name) :
            snprintf(book.path, sizeof(book.path), "/Books/%s", name);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(book.path)) {
          item.close(); dir.close(); fail("文件名过长，请缩短文件名", "path length"); return;
        }
        book.size = item.size();
        books_[count_++] = book;
      }
    }
    item.close();
  }
  dir.close();
  if (overflow) { fail("书籍超过 32 本，请移出部分文件", "book limit"); return; }
  std::sort(books_, books_ + count_, [](const Book& a, const Book& b) { return strcmp(a.path, b.path) < 0; });
  for (uint8_t i = 0; i < count_; ++i)
    for (const auto& h : saved_.entries)
      if (h.pathHash == pathHash(books_[i].path) && h.size == books_[i].size) books_[i].page = h.page;
  state_ = count_ ? State::Shelf : State::Empty; changed_ = true;
  LogI("READER", "books ready directory=/Books count=%u", count_);
}
void TxtReaderService::resetScan(Scan& scan, uint32_t offset) {
  scan = Scan{}; scan.indent = (offset & kParagraph) != 0;
  scan.offset = scan.start = offset & ~kParagraph;
  // The preview uses a single ideographic-space paragraph indent.
  if (scan.indent) { memcpy(scan.page.lines[0], "　", 4); scan.bytes = 3; }
  else scan.width = 0;
}
bool TxtReaderService::open(uint8_t index) {
  if (index >= count_) return false;
  closeBook(); active_ = index;
  file_ = SD_MMC.open(books_[index].path, FILE_READ);
  if (!file_ || file_.isDirectory()) { fail("无法读取书籍，请检查 SD 卡", "open"); return false; }
  size_ = file_.size();
  if (size_ >= kParagraph) { fail("书籍过长，请拆分 TXT 文件", "file size"); return false; }
  if (!size_) { fail("这份文本没有可阅读的内容", "empty"); return false; }
  fingerprint_ = 2166136261U;
  for (uint32_t i = 0; i < std::min<uint32_t>(size_, 256U); ++i) {
    int value = byteAt(i); if (value < 0) return false;
    fingerprint_ = (fingerprint_ ^ static_cast<uint8_t>(value)) * 16777619U;
  }
  uint32_t first = 0;
  if (size_ >= 3 && byteAt(0) == 0xEF && byteAt(1) == 0xBB && byteAt(2) == 0xBF) first = 3;
  if (first == size_) { fail("这份文本没有可阅读的内容", "empty BOM"); return false; }
  if (!indexMemory_.allocate(kMaxPages * sizeof(uint32_t), kMaxPages * sizeof(uint32_t))) {
    fail("阅读缓存不足，请退出其他功能", "index allocation"); return false;
  }
  offsets_ = static_cast<uint32_t*>(indexMemory_.data());
  current_ = History{}; current_.pathHash = pathHash(books_[index].path);
  current_.size = size_; current_.fingerprint = fingerprint_;
  for (const auto& h : saved_.entries)
    if (h.pathHash == current_.pathHash && h.size == size_ && h.fingerprint == fingerprint_) current_ = h;
  wanted_ = current_.page; pageCount_ = 0; indexed_ = pageReady_ = false;
  resetScan(indexScan_, first | kParagraph); resetScan(viewScan_, first | kParagraph);
  state_ = State::Reading; changed_ = true;
  LogI("READER", "book opened bytes=%lu resume_page=%lu index_bytes=%u", static_cast<unsigned long>(size_),
       static_cast<unsigned long>(wanted_ + 1), static_cast<unsigned>(kMaxPages * sizeof(uint32_t)));
  return true;
}
int TxtReaderService::byteAt(uint32_t offset) {
  if (offset >= size_) return -1;
  if (bufferStart_ == UINT32_MAX || offset < bufferStart_ || offset - bufferStart_ >= bufferSize_) {
    if (!file_.seek(offset)) { fail("读取中断，请检查 SD 卡后重试", "seek"); return -1; }
    bufferStart_ = offset;
    bufferSize_ = file_.read(buffer_, std::min(sizeof(buffer_), static_cast<size_t>(size_ - offset)));
    if (!bufferSize_) { fail("读取中断，请检查 SD 卡后重试", "read"); return -1; }
  }
  return buffer_[offset - bufferStart_];
}
// One UTF-8 codepoint per step; callers bound work by both count and elapsed time.
bool TxtReaderService::step(Scan& scan, Measure measure, void* context) {
  if (scan.offset >= size_) return true;
  const uint32_t start = scan.offset;
  int first = byteAt(start); if (first < 0) return false;
  const uint8_t n = first < 0x80 ? 1 : first >= 0xC2 && first <= 0xDF ? 2 :
      first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
  if (!n || start + n > size_) { fail("请将 TXT 转换为 UTF-8 编码", "UTF-8 lead"); return false; }
  char glyph[5] = {static_cast<char>(first),0,0,0,0};
  uint32_t cp = first & (n == 1 ? 0x7F : (1U << (7U-n)) - 1U);
  for (uint8_t i = 1; i < n; ++i) {
    int b = byteAt(start+i); if (b < 0) return false;
    if ((b & 0xC0) != 0x80) { fail("请将 TXT 转换为 UTF-8 编码", "UTF-8 continuation"); return false; }
    cp = (cp << 6U) | (b & 0x3F); glyph[i] = static_cast<char>(b);
  }
  if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
      cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) || cp == 0) {
    fail("请将 TXT 转换为 UTF-8 编码", "UTF-8 scalar"); return false;
  }
  if (cp == '\r' || cp == '\n') {
    scan.offset += n;
    if (cp == '\r' && scan.offset < size_ && byteAt(scan.offset) == '\n') ++scan.offset;
    if (state_ != State::Reading) return false;
    scan.nextIndent = true;
    if (++scan.row == 4) return true;
    scan.width = 20; scan.bytes = 3; memcpy(scan.page.lines[scan.row], "　", 4);
    return scan.offset >= size_;
  }
  const int16_t advance = cp == '\t' ? 20 : measure(context, cp);
  if (advance < 0 || advance > 348) { fail("字库不可用，请返回后重试", "glyph width"); return false; }
  scan.nextIndent = false;
  if (scan.width + advance > 348 || scan.bytes + n >= sizeof(scan.page.lines[0])) {
    if (++scan.row == 4) return true; // Leave the codepoint for the next page.
    scan.bytes = scan.width = 0;
  }
  if (cp == '\t') { glyph[0] = ' '; glyph[1] = 0; }
  const uint8_t bytes = cp == '\t' ? 1 : n;
  memcpy(scan.page.lines[scan.row] + scan.bytes, glyph, bytes);
  scan.bytes += bytes; scan.page.lines[scan.row][scan.bytes] = 0;
  scan.width += advance; scan.offset += n;
  return scan.offset >= size_;
}
void TxtReaderService::tick(Measure measure, void* context) {
  if (state_ != State::Reading || !measure) return;
  const uint32_t started = millis();
  for (uint8_t work = 0; work < 16 && state_ == State::Reading; ++work) {
    const bool viewport = !pageReady_ && wanted_ < pageCount_;
    if (indexed_ && !viewport) return;
    Scan& scan = viewport ? viewScan_ : indexScan_;
    const bool done = step(scan, measure, context);
    if (state_ != State::Reading) return;
    if (done) {
      if (viewport) { content_ = scan.page; pageReady_ = changed_ = true; }
      else {
        if (pageCount_ == kMaxPages) { fail("书籍过长，请拆分 TXT 文件", "page limit"); return; }
        offsets_[pageCount_] = scan.start | (scan.indent ? kParagraph : 0U);
        if (wanted_ == pageCount_) { content_ = scan.page; pageReady_ = changed_ = true; }
        ++pageCount_;
        if (scan.offset >= size_) {
          indexed_ = changed_ = true;
          LogI("READER", "pagination complete pages=%lu", static_cast<unsigned long>(pageCount_));
          if (wanted_ >= pageCount_) go(pageCount_-1);
        } else resetScan(scan, scan.offset | (scan.nextIndent ? kParagraph : 0U));
      }
    }
    if (millis() - started >= 4U) return;
  }
}
void TxtReaderService::go(uint32_t page) {
  if (!pageCount_ || page >= pageCount_ || (pageReady_ && page == wanted_)) return;
  wanted_ = page; pageReady_ = false; changed_ = true;
  resetScan(viewScan_, offsets_[page]);
}
void TxtReaderService::bookmark() {
  if (!pageReady_) return;
  current_.mark = marked() ? UINT32_MAX : wanted_;
  saveHistory(); changed_ = true;
}
bool TxtReaderService::marked() const { return current_.mark == wanted_; }
uint8_t TxtReaderService::progress() const {
  if (!size_ || !pageReady_ || wanted_ >= pageCount_) return 0;
  if (indexed_ && wanted_+1 == pageCount_) return 100;
  const uint32_t end = wanted_+1 < pageCount_ ? offsets_[wanted_+1] & ~kParagraph : indexScan_.offset;
  return static_cast<uint8_t>(static_cast<uint64_t>(end)*100U/size_);
}
bool TxtReaderService::consumeChanged() { bool value = changed_; changed_ = false; return value; }
