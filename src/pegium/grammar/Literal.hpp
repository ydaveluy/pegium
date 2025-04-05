#pragma once
#include <algorithm>
#include <array>
#include <pegium/grammar/IGrammarElement.hpp>
#include <ranges>

namespace pegium::grammar {

template <auto literal, bool case_sensitive = true>
struct Literal final : IGrammarElement {
  using type = std::string;
  constexpr ~Literal() override = default;

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {

    auto i = Literal::parse_terminal(sv);
 
    if (!i || (isword(literal.back()) && isword(*i.offset))) {
      return i;
    }

    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.begin(), i.offset};

    return  c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {

    if (literal.size() > sv.size()) {
      return MatchResult::failure(sv.begin());
    }
    std::size_t i = 0;
    for (; i < literal.size(); i++) {
      if ((isCaseSensitive() ? sv[i] : tolower(sv[i])) != literal[i]) {
        return MatchResult::failure(sv.begin() + i);
      }
    }
    return MatchResult::success(sv.begin() + i);
  }

  /// Create an insensitive Literal
  /// @return the insensitive Literal
  constexpr auto i() const noexcept { return Literal<toLower(), false>{}; }
  void print(std::ostream &os) const override {
    os << "KW(";
    for (auto c : literal)
      os << c;
    os << ")";
  }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::Literal;
  }

private:
  static constexpr auto toLower() {
    decltype(literal) newLiteral;
    std::ranges::transform(literal, newLiteral.begin(),
                           [](char c) { return tolower(c); });
    return newLiteral;
  }

  static constexpr bool isCaseSensitive() {
    if (!case_sensitive) {
      return std::ranges::none_of(literal, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      });
    }
    return case_sensitive;
  }
};

template <typename T> struct IsLiteralImpl : std::false_type {};
template <auto literal, bool case_sensitive>
struct IsLiteralImpl<Literal<literal, case_sensitive>> : std::true_type {};

template <typename T>
concept IsLiteral = IsLiteralImpl<T>::value;

/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct char_array_builder {
  std::array<char, N - 1> value;
  explicit(false) consteval char_array_builder(char const (&pp)[N]) {
    for (std::size_t i = 0; i < value.size(); ++i) {
      value[i] = pp[i];
    }
  }
};

template <char_array_builder builder> consteval auto operator""_kw() {
  static_assert(!builder.value.empty(), "A keyword cannot be empty.");
  return Literal<builder.value>{};
}

} // namespace pegium::grammar