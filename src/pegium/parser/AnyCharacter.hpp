#pragma once

#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

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
  static constexpr MatchResult parse_codepoint(std::string_view sv) noexcept {
    const auto size = sv.size();
    if (size) {
      auto b = static_cast<std::byte>(sv.front());
      if ((b & std::byte{0x80}) == std::byte{0}) {
        return MatchResult::success(sv.begin() + 1);
      }
      if ((b & std::byte{0xE0}) == std::byte{0xC0} && size >= 2) {
        return MatchResult::success(sv.begin() + 2);
      }
      if ((b & std::byte{0xF0}) == std::byte{0xE0} && size >= 3) {
        return MatchResult::success(sv.begin() + 3);
      }
      if ((b & std::byte{0xF8}) == std::byte{0xF0} && size >= 4) {
        return MatchResult::success(sv.begin() + 4);
      }
    }
    return MatchResult::failure(sv.begin());
  }
};
} // namespace pegium::parser