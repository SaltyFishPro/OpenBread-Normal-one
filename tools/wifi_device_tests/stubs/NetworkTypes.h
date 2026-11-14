#pragma once
#include <Arduino.h>
#include <string>
class String {
 public:
  String(const char* value="") : value_(value) {}
  String(const std::string& value) : value_(value) {}
  size_t length() const { return value_.size(); }
  char operator[](size_t index) const { return value_[index]; }
  bool operator==(const char* other) const { return value_ == other; }
  void toCharArray(char* dest, size_t bytes) const { snprintf(dest,bytes,"%s",value_.c_str()); }
 private:
  std::string value_;
};
struct IPAddress {
  uint8_t bytes[4] = {};
  uint8_t operator[](size_t index) const { return bytes[index]; }
};
