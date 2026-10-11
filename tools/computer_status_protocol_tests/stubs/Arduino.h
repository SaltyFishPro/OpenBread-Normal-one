#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

// Host-only serial endpoint; firmware keeps its fixed-capacity buffers.
struct TestSerial {
  static constexpr size_t kRxCapacity = 256;
  std::deque<uint8_t> input;
  std::vector<uint8_t> output;
  size_t droppedBytes = 0;
  void receive(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
      if (input.size() < kRxCapacity) input.push_back(data[i]);
      else ++droppedBytes;
    }
  }
  explicit operator bool() const { return true; }
  int available() const { return static_cast<int>(input.size()); }
  int read() {
    if (input.empty()) return -1;
    const uint8_t value = input.front();
    input.pop_front();
    return value;
  }
  size_t write(const uint8_t* data, size_t length) {
    output.insert(output.end(), data, data + length);
    return length;
  }
};

extern TestSerial Serial;
extern uint32_t TestMillis;
inline uint32_t millis() { return TestMillis; }
