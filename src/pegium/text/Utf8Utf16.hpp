#pragma once

#include <cstdint>

namespace pegium::text {

inline void decodeOneUtf8ToUtf16Units(const std::uint8_t *bytes,
                                      std::uint32_t available,
                                      std::uint32_t &advance,
                                      std::uint32_t &utf16Units) {
  const auto first = bytes[0];
  if (first < 0x80u) {
    advance = 1;
    utf16Units = 1;
    return;
  }

  const auto expectedLength =
      first < 0xE0u ? 2u : first < 0xF0u ? 3u : first < 0xF8u ? 4u : 1u;
  if (expectedLength == 1u || available < expectedLength) {
    advance = 1;
    utf16Units = 1;
    return;
  }

  for (std::uint32_t index = 1; index < expectedLength; ++index) {
    if ((bytes[index] & 0xC0u) != 0x80u) {
      advance = 1;
      utf16Units = 1;
      return;
    }
  }

  std::uint32_t codePoint = 0;
  if (expectedLength == 2u) {
    codePoint = ((first & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
  } else if (expectedLength == 3u) {
    codePoint = ((first & 0x0Fu) << 12) | ((bytes[1] & 0x3Fu) << 6) |
                (bytes[2] & 0x3Fu);
  } else {
    codePoint = ((first & 0x07u) << 18) | ((bytes[1] & 0x3Fu) << 12) |
                ((bytes[2] & 0x3Fu) << 6) | (bytes[3] & 0x3Fu);
  }

  advance = expectedLength;
  utf16Units = codePoint >= 0x10000u ? 2u : 1u;
}

} // namespace pegium::text
