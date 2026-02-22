#pragma once

#include <algorithm>
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
  explicit(false) consteval char_array_builder(char const (&pp)[N]) {
    for (std::size_t i = 0; i < value.size(); ++i) {
      value[i] = pp[i];
    }
  }
};

constexpr std::array<bool, 256> createCharacterRange(std::string_view s) {
  std::array<bool, 256> value{};
  std::size_t i = 0;

  const std::size_t len = s.size();
  bool negate = false;
  if (len > 0 && s[0] == '^') {
    negate = true;
    ++i;
  }

  assert(!negate && "CharacterRange does not support '^'.");
  // check that each char is not a codepoint
  for (auto chr : s) {
    assert((static_cast<std::byte>(chr) & std::byte{0x80}) == std::byte{0});
  }
  while (i < len) {
    auto first = s[i];
    if (i + 2 < len && s[i + 1] == '-') {
      auto last = s[i + 2];
      for (auto c = static_cast<unsigned char>(first);
           c <= static_cast<unsigned char>(last); ++c) {
        value[c] = true;
      }
      i += 3;
    } else {
      value[static_cast<unsigned char>(first)] = true;
      ++i;
    }
  }
  if (negate) {
    std::transform(value.begin(), value.end(), value.begin(), std::logical_not{});
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
  for (unsigned int i = 0; i < 256; ++i) {
    table[i] = ((i & 0x80u) == 0u)       ? 1
               : ((i & 0xE0u) == 0xC0u) ? 2
               : ((i & 0xF0u) == 0xE0u) ? 3
               : ((i & 0xF8u) == 0xF0u)
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

constexpr bool is_utf8_codepoint_complete(const char *begin,
                                          const char *end) noexcept {
  if (begin == end) [[unlikely]] {
    return false;
  }
  const auto len = utf8_codepoint_length(*begin);
  if (len == std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  return static_cast<std::size_t>(end - begin) >= len;
}

constexpr const char *advanceOneCodepointLossy(const char *cursor,
                                               const char *end) noexcept {
  if (cursor == end) [[unlikely]] {
    return end;
  }
  const auto len = utf8_codepoint_length(*cursor);
  if (len == std::numeric_limits<std::size_t>::max() ||
      static_cast<std::size_t>(end - cursor) < len) {
    return cursor + 1;
  }
  return cursor + len;
}

consteval auto make_tolower() {
  std::array<unsigned char, 256> lookup{};
  for (int c = 0; c < 256; ++c) {
    if (c >= 'A' && c <= 'Z') {
      lookup[c] = static_cast<unsigned char>(c) + ('a' - 'A');
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
    std::snprintf(buf, sizeof(buf), "\\x%02X",
                  static_cast<unsigned char>(c));
    return buf;
  }
}

} // namespace pegium::parser
