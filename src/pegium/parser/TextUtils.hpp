#pragma once

#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

namespace pegium::parser {

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

/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct range_array_builder {
  std::array<bool, 256> value{};
  constexpr explicit(false) range_array_builder(const char (&s)[N])
      : value{createCharacterRange({s, N - 1})} {}
};

static constexpr const auto is_word_lookup =
    range_array_builder{"a-zA-Z0-9_"}.value;
constexpr bool isWord(char c) {
  return is_word_lookup[static_cast<unsigned char>(c)];
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

static constexpr auto utf8_codepoint_length_table =
    make_utf8_codepoint_length_table();

constexpr std::size_t utf8_codepoint_length(char leadByte) noexcept {
  return utf8_codepoint_length_table[static_cast<unsigned char>(leadByte)];
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
static constexpr auto tolower_array = make_tolower();

/// Fast helper function to convert a char to lower case
/// @param c the char to convert
/// @return the lower case char
constexpr char tolower(char c) {
  return static_cast<char>(tolower_array[static_cast<unsigned char>(c)]);
}

constexpr std::string escape_char(char c) {
  switch (c) {
  case '\n':
    return R"(\n)";
  case '\r':
    return R"(\r)";
  case '\t':
    return R"(\t)";
  case '\v':
    return R"(\v)";
  case '\f':
    return R"(\f)";
  case '\b':
    return R"(\b)";
  case '\a':
    return R"(\a)";
  case '\\':
    return R"(\\)";
  case '\'':
    return R"(\')";
  case '\"':
    return R"(\")";
  default:
    if (std::isprint(static_cast<unsigned char>(c))) {
      return std::string{c};
    }
    char buf[5];
    std::snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(c));
    return buf;
  }
}

} // namespace pegium::parser
