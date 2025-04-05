#pragma once

#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

struct AnyCharacter final : IGrammarElement {
  using type = std::string;
  constexpr ~AnyCharacter() noexcept override = default;

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    auto i = codepoint_length(sv);
    if (!i) {
      return i;
    }
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i.offset};

    return c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    return codepoint_length(sv);
  }
  void print(std::ostream &os) const override { os << '.'; }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::AnyCharacter;
  }

private:
  static constexpr MatchResult codepoint_length(std::string_view sv) noexcept {
    if (!sv.empty()) {
      auto b = static_cast<std::byte>(sv.front());
      if ((b & std::byte{0x80}) == std::byte{0}) {
        return MatchResult::success(sv.begin() + 1);
      }
      if ((b & std::byte{0xE0}) == std::byte{0xC0} && sv.size() >= 2) {
        return MatchResult::success(sv.begin() + 2);
      }
      if ((b & std::byte{0xF0}) == std::byte{0xE0} && sv.size() >= 3) {
        return MatchResult::success(sv.begin() + 3);
      }
      if ((b & std::byte{0xF8}) == std::byte{0xF0} && sv.size() >= 4) {
        return MatchResult::success(sv.begin() + 4);
      }
    }
    return MatchResult::failure(sv.begin());
  }
};
} // namespace pegium::grammar