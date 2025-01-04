#pragma once

#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

struct AnyCharacter final : IGrammarElement {
  using type = std::string;
  constexpr ~AnyCharacter() noexcept override = default;

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    auto i = codepoint_length(sv);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i};

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return codepoint_length(sv);
  }
  void print(std::ostream &os) const override { os << '.'; }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::AnyCharacter;
  }

private:
  static constexpr std::size_t codepoint_length(std::string_view sv) noexcept {
    if (!sv.empty()) {
      auto b = static_cast<std::byte>(sv.front());
      if ((b & std::byte{0x80}) == std::byte{0}) {
        return 1;
      }
      if ((b & std::byte{0xE0}) == std::byte{0xC0} && sv.size() >= 2) {
        return 2;
      }
      if ((b & std::byte{0xF0}) == std::byte{0xE0} && sv.size() >= 3) {
        return 3;
      }
      if ((b & std::byte{0xF8}) == std::byte{0xF0} && sv.size() >= 4) {
        return 4;
      }
    }
    return PARSE_ERROR;
  }
};
} // namespace pegium::grammar