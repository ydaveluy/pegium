#pragma once
#include <algorithm>
#include <array>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <ranges>

namespace pegium::parser {

template <auto literal, bool case_sensitive = true>
struct Literal final : grammar::Literal {
  using type = std::string;
  std::string_view getValue() const noexcept override {
    return {literal.begin(), literal.size()};
  }
  bool isCaseSensitive() const noexcept override { return case_sensitive; }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {

    auto i = parse_terminal(sv);

    if (!i) {
      return i;
    }
    // TODO use something like IContext::isWordBoundary(char c) ?
    if constexpr (isword(literal.back())) {
      if (isword(*i.offset))
        return MatchResult::failure(i.offset);
    }

    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.begin(), i.offset};

    return c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {

    if (literal.size() > sv.size()) {
      return MatchResult::failure(sv.begin());
    }

    for (std::size_t i = 0; i < literal.size(); i++) {
      if constexpr (case_sensitive) {
        if (sv[i] != literal[i]) {
          return MatchResult::failure(sv.begin() + i);
        }
      } else {
        if (tolower(sv[i]) != literal[i]) {
          return MatchResult::failure(sv.begin() + i);
        }
      }
    }
    return MatchResult::success(sv.begin() + literal.size());
  }

  /// Create an insensitive Literal
  /// @return the insensitive Literal
  constexpr auto i() const noexcept {

    return Literal<toLower(), isCaseSensitive(literal)>{};
  }
  void print(std::ostream &os) const override {
    os << "'";
    for (auto c : literal)
      os << escape_char(c);
    os << "'";
    if (!case_sensitive)
      os << "i";
  }

private:
  static constexpr auto toLower() {
    decltype(literal) newLiteral;
    std::ranges::transform(literal, newLiteral.begin(),
                           [](char c) { return tolower(c); });
    return newLiteral;
  }

  static constexpr bool isCaseSensitive(auto lit) {
    return std::ranges::none_of(lit, [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
  }
};

template <typename T> struct IsLiteralImpl : std::false_type {};
template <auto literal, bool case_sensitive>
struct IsLiteralImpl<Literal<literal, case_sensitive>> : std::true_type {};

template <typename T>
concept IsLiteral = IsLiteralImpl<T>::value;

} // namespace pegium::parser