#include "SdFontFile.h"
#include "SdCardSession.h"

#include <SD_MMC.h>

bool SdFontFile::open(const char* path, bool invalidateOnReadError) {
  close();
  invalidateOnReadError_ = invalidateOnReadError;
  file_ = SD_MMC.open(path, FILE_READ);
  if (!file_ || file_.isDirectory() || file_.size() > UINT32_MAX) {
    close();
    return false;
  }
  size_ = static_cast<uint32_t>(file_.size());
  return true;
}

void SdFontFile::close() {
  file_.close();
  size_ = 0;
}

bool SdFontFile::readAt(uint32_t offset, uint8_t* data, size_t length) {
  if (!file_ || offset > size_ || length > size_ - offset) return false;
  if (length == 0U) return true;
  if (file_.seek(offset, SeekSet) && file_.read(data, length) == length) return true;
  if (invalidateOnReadError_) SdCardSession::invalidate();
  return false;
}
