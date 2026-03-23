#pragma once

#include <cstddef>
#include <cstdint>

namespace pegium::text {

/// Decodes one UTF-8 code point and reports its UTF-16 width.
inline void decodeOneUtf8ToUtf16Units(const std::byte *bytes,
                                      std::uint32_t available,
                                      std::uint32_t &advance,
                                      std::uint32_t &utf16Units) {
  const auto first = std::to_integer<std::uint8_t>(bytes[0]);
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
    const auto nextByte = std::to_integer<std::uint8_t>(bytes[index]);
    if ((nextByte & 0xC0u) != 0x80u) {
      advance = 1;
      utf16Units = 1;
      return;
    }
  }

  std::uint32_t codePoint = 0;
  if (expectedLength == 2u) {
    const auto second = std::to_integer<std::uint8_t>(bytes[1]);
    codePoint = ((first & 0x1Fu) << 6) | (second & 0x3Fu);
  } else if (expectedLength == 3u) {
    const auto second = std::to_integer<std::uint8_t>(bytes[1]);
    const auto third = std::to_integer<std::uint8_t>(bytes[2]);
    codePoint =
        ((first & 0x0Fu) << 12) | ((second & 0x3Fu) << 6) | (third & 0x3Fu);
  } else {
    const auto second = std::to_integer<std::uint8_t>(bytes[1]);
    const auto third = std::to_integer<std::uint8_t>(bytes[2]);
    const auto fourth = std::to_integer<std::uint8_t>(bytes[3]);
    codePoint = ((first & 0x07u) << 18) | ((second & 0x3Fu) << 12) |
                ((third & 0x3Fu) << 6) | (fourth & 0x3Fu);
  }

  advance = expectedLength;
  utf16Units = codePoint >= 0x10000u ? 2u : 1u;
}

} // namespace pegium::text
