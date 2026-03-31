#include "SdCardService.h"

bool SdCardService::begin() {
  return refresh() == Status::Ready;
}

SdCardService::Status SdCardService::refresh() {
  SdCardDriver::Info info;
  if (!driver_.refresh(info)) {
    status_ = Status::NotInserted;
    totalBytes_ = 0;
    freeBytes_ = 0;
    return status_;
  }

  if (!info.mounted || info.totalBytes == 0 || info.usedBytes > info.totalBytes) {
    status_ = Status::ReinsertNeeded;
    totalBytes_ = 0;
    freeBytes_ = 0;
    return status_;
  }

  status_ = Status::Ready;
  totalBytes_ = info.totalBytes;
  freeBytes_ = info.totalBytes - info.usedBytes;
  return status_;
}

SdCardService::Status SdCardService::status() const { return status_; }

uint64_t SdCardService::totalBytes() const { return totalBytes_; }

uint64_t SdCardService::freeBytes() const { return freeBytes_; }
