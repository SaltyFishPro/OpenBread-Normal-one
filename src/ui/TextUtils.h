#pragma once

#include <stdint.h>

#include <U8g2_for_ST73XX.h>

// Shared text helpers. ReaderPage and MusicPage had grown their own copies of
// the same UTF-8 byte classification, and four pages repeated the same
// horizontal centering math.
namespace TextUtils {

// True for continuation bytes (0b10xxxxxx), i.e. not the first byte of a
// character.
inline bool utf8IsContinuation(char c) {
  return (static_cast<uint8_t>(c) & 0xC0U) == 0x80U;
}

// Number of bytes the character starting at `c` occupies. Invalid lead bytes
// report 1 so callers always advance instead of looping forever.
inline uint8_t utf8CharLen(char c) {
  const uint8_t b = static_cast<uint8_t>(c);
  if ((b & 0x80U) == 0U) {
    return 1;
  }
  if ((b & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((b & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((b & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

// Number of UTF-8 characters in a NUL-terminated string.
inline uint8_t utf8CharCount(const char* value) {
  uint8_t count = 0;
  for (uint8_t i = 0; value[i] != '\0'; ++i) {
    if (!utf8IsContinuation(value[i])) {
      ++count;
    }
  }
  return count;
}

// Start of the last `visibleChars` characters; the whole string when there are
// fewer characters than requested.
inline const char* utf8SuffixByChars(const char* value, uint8_t visibleChars) {
  const uint8_t totalChars = utf8CharCount(value);
  if (visibleChars >= totalChars) {
    return value;
  }

  uint8_t charsToSkip = static_cast<uint8_t>(totalChars - visibleChars);
  for (uint8_t i = 0; value[i] != '\0'; ++i) {
    if (!utf8IsContinuation(value[i])) {
      --charsToSkip;
      if (charsToSkip == 0) {
        ++i;
        while (value[i] != '\0' && utf8IsContinuation(value[i])) {
          ++i;
        }
        return value + i;
      }
    }
  }
  return value;
}

// Left X for drawing `value` horizontally centered on `centerX`.
inline int16_t centeredTextX(U8G2_FOR_ST73XX& text, const char* value, int16_t centerX) {
  return static_cast<int16_t>(centerX - text.getUTF8Width(value) / 2);
}

// Left X for drawing `value` horizontally centered inside a box that starts at
// `boxX` and is `boxWidth` pixels wide.
inline int16_t centeredTextXInBox(U8G2_FOR_ST73XX& text, const char* value, int16_t boxX,
                                  int16_t boxWidth) {
  return static_cast<int16_t>(boxX + (boxWidth - text.getUTF8Width(value)) / 2);
}

}  // namespace TextUtils
