#pragma once

#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

namespace helpers {
static constexpr auto make_codepoint_len_table() {
  std::array<std::size_t, 256> table = {};
  for (unsigned int i = 0; i < 256; ++i) {

    if ((i & 0x80u) == 0) {
      table[i] = 1;
    } else if ((i & 0xE0u) == 0xC0u) {
      table[i] = 2;
    } else if ((i & 0xF0u) == 0xE0u) {
      table[i] = 3;
    } else if ((i & 0xF8u) == 0xF0u) {
      table[i] = 4;
    } else {
      table[i] = std::numeric_limits<std::size_t>::max(); // invalid prefix /
                                                          // continuation byte
    }
  }
  return table;
}
} // namespace helpers

struct AnyCharacter final : grammar::AnyCharacter {
  using type = std::string;
  // constexpr ~AnyCharacter() noexcept override = default;

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    auto i = parse_codepoint(sv);
    if (!i) {
      return i;
    }
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i.offset};

    return c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_codepoint(sv);
  }

private:
  static constexpr std::array<std::size_t, 256> len_table{
      helpers::make_codepoint_len_table()};

  [[nodiscard]] static inline constexpr MatchResult
  parse_codepoint(const std::string_view sv) noexcept {
    const char *ptr = sv.data();
    const auto len = len_table[static_cast<unsigned char>(*ptr)];
    if (sv.size() >= len) {
      return MatchResult::success(ptr + len);
    }
    return MatchResult::failure(ptr);
  }
};
} // namespace pegium::parser