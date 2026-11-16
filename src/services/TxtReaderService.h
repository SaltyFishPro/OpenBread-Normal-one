#pragma once
#include <FS.h>
#include "../bsp/SdFontCacheBuffer.h"

class SdCardService;
class TxtReaderService {
public:
  static constexpr uint8_t kMaxBooks = 32;
  static constexpr uint32_t kMaxPages = 32768;
  using Measure = int16_t (*)(void*, uint32_t);
  enum class State : uint8_t { Closed, Shelf, Empty, Missing, Error, Reading };
  struct Book { char path[192]{}; uint32_t size = 0; uint32_t page = 0; };
  struct Page { char lines[4][128]{}; };
  void scan(SdCardService& sd);
  bool open(uint8_t index);
  void closeBook();
  void end();
  void tick(Measure measure, void* context);
  void go(uint32_t page);
  void bookmark();
  bool consumeChanged();
  State state() const { return state_; }
  const char* error() const { return error_; }
  uint8_t count() const { return count_; }
  const Book& book(uint8_t index) const { return books_[index]; }
  static void title(const Book& book, char* out, size_t capacity);
  const Page& content() const { return content_; }
  uint32_t currentPage() const { return wanted_; }
  uint32_t totalPages() const { return pageCount_; }
  bool indexed() const { return indexed_; }
  bool pageReady() const { return pageReady_; }
  bool busy() const { return state_ == State::Reading && (!indexed_ || !pageReady_); }
  bool marked() const;
  uint8_t progress() const;
  bool dark() const { return dark_; }
  void toggleTheme() { dark_ = !dark_; changed_ = true; }
private:
  struct Scan {
    Page page;
    uint32_t offset = 0, start = 0;
    uint16_t width = 20;
    uint8_t row = 0, bytes = 0;
    bool indent = false, nextIndent = false;
  };
  struct History {
    uint64_t pathHash = 0;
    uint32_t size = 0, fingerprint = 0, page = 0, mark = UINT32_MAX;
  };
  struct Saved { uint32_t magic = 0x54585431; History entries[8]; uint8_t dark = 0; };
  void loadHistory();
  void saveHistory();
  void fail(const char* message, const char* stage);
  int byteAt(uint32_t offset);
  bool step(Scan& scan, Measure measure, void* context);
  void resetScan(Scan& scan, uint32_t offset);
  State state_ = State::Closed;
  const char* error_ = "";
  Book books_[kMaxBooks];
  uint8_t count_ = 0, active_ = 0;
  File file_;
  SdFontCacheBuffer indexMemory_;
  uint32_t* offsets_ = nullptr;
  uint32_t pageCount_ = 0, wanted_ = 0, size_ = 0, fingerprint_ = 0;
  Scan indexScan_, viewScan_;
  Page content_;
  Saved saved_;
  History current_;
  uint8_t buffer_[1024];
  uint32_t bufferStart_ = UINT32_MAX;
  size_t bufferSize_ = 0;
  bool indexed_ = false, pageReady_ = false, changed_ = false, dark_ = false;
};
