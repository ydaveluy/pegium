#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

namespace pegium::utils {

/// Build an array of char (remove the trailing '\0')
/// @tparam N the number of char including '\0'
template <std::size_t N> struct char_array_builder {
  std::array<char, N - 1> value;
  explicit(false) consteval char_array_builder(char const (&sourceChars)[N]) {
    for (std::size_t charIndex = 0; charIndex < value.size(); ++charIndex) {
      value[charIndex] = sourceChars[charIndex];
    }
  }
};

template <std::size_t Pos, std::size_t Count, std::size_t N>
consteval auto array_substr(const std::array<char, N> &in) {
  static_assert(Pos <= N, "Pos out of range");
  static_assert(Pos + Count <= N, "Pos+Count out of range");

  std::array<char, Count> out{};
  for (std::size_t offset = 0; offset < Count; ++offset) {
    out[offset] = in[Pos + offset];
  }
  return out;
}

template <std::size_t Pos, std::size_t N>
consteval auto array_substr(const std::array<char, N> &in) {
  static_assert(Pos <= N, "Pos out of range");
  return array_substr<Pos, N - Pos>(in);
}

constexpr std::array<bool, 256> createCharacterRange(std::string_view s) {
  std::array<bool, 256> value{};
  std::size_t rangeCursor = 0;

  const std::size_t rangeTextLength = s.size();

  //  check that each char is not a codepoint
  for (const auto chr : s) {
    (void) chr;
    assert((static_cast<std::byte>(chr) & std::byte{0x80}) == std::byte{0});
  }

  // character range negation is handled in  operator""_cr
  assert(!(rangeTextLength > 0 && s[0] == '^') &&
         "Character range negation is not supported in character range "
         "definition.");

  while (rangeCursor < rangeTextLength) {
    auto rangeStart = s[rangeCursor];
    if (rangeCursor + 2 < rangeTextLength && s[rangeCursor + 1] == '-') {
      auto rangeEnd = s[rangeCursor + 2];
      for (auto codepoint = static_cast<unsigned char>(rangeStart);
           codepoint <= static_cast<unsigned char>(rangeEnd); ++codepoint) {
        value[codepoint] = true;
      }
      rangeCursor += 3;
    } else {
      value[static_cast<unsigned char>(rangeStart)] = true;
      ++rangeCursor;
    }
  }
  return value;
}

/// Compile-time builder of a 256-entry byte-membership table from a character
/// range literal (the trailing '\0' is excluded from the range).
/// @tparam N the number of chars including the ending '\0' (i.e. the literal's
/// array size)
template <std::size_t N> struct range_array_builder {
  std::array<bool, 256> value{};
  constexpr explicit(false) range_array_builder(const char (&s)[N])
      : value{createCharacterRange({s, N - 1})} {}
};

inline constexpr const auto is_word_lookup =
    range_array_builder{"a-zA-Z0-9_"}.value;
constexpr bool isWord(char c) {
  return is_word_lookup[static_cast<unsigned char>(c)];
}

inline constexpr const auto is_letter_lookup =
    range_array_builder{"a-zA-Z"}.value;
constexpr bool isLetter(char c) {
  return is_letter_lookup[static_cast<unsigned char>(c)];
}

consteval auto make_utf8_codepoint_length_table() {
  std::array<std::size_t, 256> table = {};
  for (unsigned int byteValue = 0; byteValue < 256; ++byteValue) {
    table[byteValue] = (byteValue == 0u)                ? 0
                       : ((byteValue & 0x80u) == 0u)    ? 1
                       : ((byteValue & 0xE0u) == 0xC0u) ? 2
                       : ((byteValue & 0xF0u) == 0xE0u) ? 3
                       : ((byteValue & 0xF8u) == 0xF0u)
                           ? 4
                           : std::numeric_limits<std::size_t>::max();
  }
  return table;
}

inline constexpr auto utf8_codepoint_length_table =
    make_utf8_codepoint_length_table();

constexpr std::size_t utf8_codepoint_length(char leadByte) noexcept {
  return utf8_codepoint_length_table[static_cast<unsigned char>(leadByte)];
}

/// Counts the number of UTF-8 codepoints in `text`. Lossy-tolerant: a
/// truncated/invalid lead byte (length 0, or a multibyte length that runs past
/// the end) is counted as a single codepoint and advances one byte, matching
/// the recovery matcher's `decode_text` so byte<->codepoint accounting stays
/// consistent across the fuzzy path. For pure-ASCII text this returns
/// `text.size()`.
constexpr std::size_t utf8_codepoint_count(std::string_view text) noexcept {
  std::size_t count = 0u;
  std::size_t index = 0u;
  while (index < text.size()) {
    const auto length = utf8_codepoint_length(text[index]);
    index += (length == 0u || length > text.size() - index) ? 1u : length;
    ++count;
  }
  return count;
}

/// Decodes the UTF-8 codepoint starting at `cursor` and returns its
/// Unicode value. ASCII (1 byte) is returned as the byte itself.
/// Multi-byte sequences are decoded; malformed input returns the lead
/// byte (lossy fallback so callers can keep advancing). The function
/// does NOT advance the cursor — pair with `utf8_codepoint_length` or
/// `advanceOneCodepointLossy` when iterating.
constexpr std::uint32_t
decode_utf8_codepoint(const char *cursor) noexcept {
  const auto lead = static_cast<unsigned char>(*cursor);
  if (lead < 0x80) {
    return lead;
  }
  switch (utf8_codepoint_length_table[lead]) {
  case 2:
    return (static_cast<std::uint32_t>(lead & 0x1Fu) << 6) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(cursor[1]) & 0x3Fu);
  case 3:
    return (static_cast<std::uint32_t>(lead & 0x0Fu) << 12) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(cursor[1]) & 0x3Fu)
            << 6) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(cursor[2]) & 0x3Fu);
  case 4:
    return (static_cast<std::uint32_t>(lead & 0x07u) << 18) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(cursor[1]) & 0x3Fu)
            << 12) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(cursor[2]) & 0x3Fu)
            << 6) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(cursor[3]) & 0x3Fu);
  default:
    return lead;
  }
}

/// True iff `codepoint` is part of an identifier-like token. Accepts ASCII
/// word characters (letters, digits, underscore) and any non-ASCII codepoint
/// (>= 0x80). This is a heuristic that covers letters of every script
/// (Latin-extended, Cyrillic, CJK, Arabic, ...) without embedding the Unicode
/// XID tables; false positives on Unicode punctuation are acceptable.
[[nodiscard]] constexpr bool
is_identifier_like_codepoint(std::uint32_t codepoint) noexcept {
  return codepoint >= 0x80 || isWord(static_cast<char>(codepoint));
}

/// True iff `[p, end)` begins with a complete, identifier-like UTF-8 codepoint.
/// A null/at-end pointer or a truncated multi-byte tail reads as non-identifier.
[[nodiscard]] constexpr bool
is_identifier_like_codepoint_at(const char *p, const char *end) noexcept {
  if (p == nullptr || p >= end) {
    return false;
  }
  if (const auto length = utf8_codepoint_length(*p);
      length == 0 || length > static_cast<std::size_t>(end - p)) {
    return false;
  }
  return is_identifier_like_codepoint(decode_utf8_codepoint(p));
}

/// Returns the byte index at which the codepoint preceding `pos` starts, by
/// stepping back over UTF-8 continuation bytes (`0b10xxxxxx`). Returns 0 at the
/// start of the text.
[[nodiscard]] constexpr std::size_t
previous_codepoint_start(std::string_view text, std::size_t pos) noexcept {
  if (pos == 0) {
    return 0;
  }
  std::size_t index = pos - 1;
  while (index > 0 &&
         (static_cast<unsigned char>(text[index]) & 0xC0U) == 0x80U) {
    --index;
  }
  return index;
}

constexpr const char *
consume_utf8_codepoint_if_complete(const char *cursor) noexcept {
  const auto leadByte = static_cast<unsigned char>(*cursor);
  if (leadByte == 0) [[unlikely]] {
    return nullptr;
  }
  if (leadByte < 0x80) [[likely]] {
    return cursor + 1;
  }

  switch (utf8_codepoint_length_table[leadByte]) {
  case 2:
    return cursor[1] == 0 ? nullptr : cursor + 2;
  case 3:
    return cursor[1] == 0 || cursor[2] == 0 ? nullptr : cursor + 3;
  case 4:
    return cursor[1] == 0 || cursor[2] == 0 || cursor[3] == 0 ? nullptr
                                                              : cursor + 4;
  default:
    return nullptr;
  }
}

constexpr const char *advanceOneCodepointLossy(const char *cursor) noexcept {
  const auto leadByte = static_cast<unsigned char>(*cursor);
  if (leadByte == 0) [[unlikely]] {
    return cursor;
  }
  if (leadByte < 0x80) [[likely]] {
    return cursor + 1;
  }

  switch (utf8_codepoint_length_table[leadByte]) {
  case 2:
    return cursor[1] == 0 ? cursor + 1 : cursor + 2;
  case 3:
    return cursor[1] == 0 || cursor[2] == 0 ? cursor + 1 : cursor + 3;
  case 4:
    return cursor[1] == 0 || cursor[2] == 0 || cursor[3] == 0 ? cursor + 1
                                                              : cursor + 4;
  default:
    return cursor + 1;
  }
}

consteval auto make_tolower() {
  std::array<unsigned char, 256> lookup{};
  for (std::size_t c = 0; c < lookup.size(); ++c) {
    if (c >= static_cast<std::size_t>('A') && c <= static_cast<std::size_t>('Z')) {
      lookup[c] = static_cast<unsigned char>(static_cast<int>(c) + ('a' - 'A'));
    } else {
      lookup[c] = static_cast<unsigned char>(c);
    }
  }
  return lookup;
}
inline constexpr auto tolower_array = make_tolower();

/// Fast helper function to convert a char to lower case
/// @param c the char to convert
/// @return the lower case char
constexpr char tolower(char c) {
  return static_cast<char>(tolower_array[static_cast<unsigned char>(c)]);
}

std::string escape_char(char c);

} // namespace pegium::utils
