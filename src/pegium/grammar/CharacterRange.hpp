#pragma once

#include <algorithm>
#include <array>
#include <pegium/grammar/IGrammarElement.hpp>
#include <ranges>

namespace pegium::grammar {

template <std::array<bool, 256> lookup>
struct CharacterRange final : IGrammarElement {
  using type = std::string;
  constexpr ~CharacterRange() override = default;

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    auto i = CharacterRange::parse_terminal(sv);
    if (!i) {
      return i;
    }
    if (sv.end() > i.offset && isword(*(i.offset - 1)) && isword(*i.offset)) {
      return MatchResult::failure(i.offset);
    }
    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i.offset};
    node.grammarSource = this;

    return c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {

    return (!sv.empty() && lookup[static_cast<unsigned char>(sv[0])])
               ? MatchResult::success(sv.begin() + 1)
               : MatchResult::failure(sv.begin());
  }
  /// Create an insensitive Characters Ranges
  /// @return the insensitive Characters Ranges
  constexpr auto i() const noexcept {
    return CharacterRange<toInsensitive()>{};
  }
  /// Negate the Characters Ranges
  /// @return the negated Characters Ranges
  constexpr auto operator!() const noexcept {
    return CharacterRange<toNegated()>{};
  }
  void print(std::ostream &os) const override { os << '[' << ']'; }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::CharacterRange;
  }

private:
  static constexpr auto toInsensitive() {
    auto newLookup = lookup;
    for (char c = 'a'; c <= 'z'; ++c) {
      auto lower = static_cast<unsigned char>(c);
      auto upper = static_cast<unsigned char>(c - 'a' + 'A');

      newLookup[lower] |= lookup[upper];
      newLookup[upper] |= lookup[lower];
    }
    return newLookup;
  }
  static constexpr auto toNegated() {
    decltype(lookup) newLookup;
    std::ranges::transform(lookup, newLookup.begin(), std::logical_not{});
    return newLookup;
  }
};

/// Concat 2 Characters Ranges
/// @tparam otherLookup the other Characters Ranges
/// @param
/// @return the concatenation of both Characters Ranges
template <std::array<bool, 256> lhs, std::array<bool, 256> rhs>
constexpr auto operator|(CharacterRange<lhs>, CharacterRange<rhs>) noexcept {
  auto toOr = [] {
    std::array<bool, 256> newLookup;
    std::ranges::transform(lhs, rhs, newLookup.begin(), std::logical_or{});
    return newLookup;
  };
  return CharacterRange<toOr()>{};
}

template <range_array_builder builder> consteval auto operator""_cr() {
  return CharacterRange<builder.value>{};
}

} // namespace pegium::grammar