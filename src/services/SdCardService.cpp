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
    rootReadable_ = false;
    return status_;
  }

  if (!info.mounted || info.totalBytes == 0 || info.usedBytes > info.totalBytes) {
    status_ = Status::ReinsertNeeded;
    totalBytes_ = 0;
    freeBytes_ = 0;
    rootReadable_ = false;
    return status_;
  }

  status_ = Status::Ready;
  totalBytes_ = info.totalBytes;
  freeBytes_ = info.totalBytes - info.usedBytes;
  rootReadable_ = info.rootReadable;
  return status_;
}

void SdCardService::end() {
  driver_.end();
  status_ = Status::ReinsertNeeded;
  totalBytes_ = 0;
  freeBytes_ = 0;
  rootReadable_ = false;
}

SdCardService::Status SdCardService::status() const { return status_; }

uint64_t SdCardService::totalBytes() const { return totalBytes_; }

uint64_t SdCardService::freeBytes() const { return freeBytes_; }

bool SdCardService::rootReadable() const { return rootReadable_; }
