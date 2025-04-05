#pragma once
#include <pegium/grammar/IContext.hpp>
#include <pegium/grammar/IGrammarElement.hpp>
#include <string_view>
namespace pegium::grammar {

template <typename... Elements>
  requires(IsGrammarElement<Elements> && ...)
struct Group final : IGrammarElement {
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");
  constexpr ~Group() override = default;
  constexpr explicit Group(std::tuple<Elements...> &&elems)
      : elements{std::move(elems)} {}

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    MatchResult i = MatchResult::success(sv.begin());
    auto size = parent.content.size();
    std::apply(
        [&](const auto &...element) {
          ((i &= element.parse_rule({i.offset, sv.end()}, parent, c)) && ...);
        },
        elements);
    if (!i) {
      parent.content.resize(size);
    }
    return i;
  }

  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    MatchResult i = MatchResult::success(sv.begin());

    std::apply(
        [&](const auto &...element) {
          ((i = element.parse_terminal({i.offset, sv.end()})) && ...);
        },
        elements);

    return i;
  }
  void print(std::ostream &os) const override {
    os << '(';
    std::apply(
        [&os](const auto &...args) {
          bool first = true;
          ((os << (first ? "" : " + ") << args, first = false), ...);
        },
        elements);
    os << ')';
  }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::Group;
  }

  // private:
  std::tuple<Elements...> elements;
};

template <typename... Lhs, typename... Rhs>
  requires(IsGrammarElement<Lhs> && ...) && (IsGrammarElement<Rhs> && ...)
constexpr auto operator+(Group<Lhs...> &&lhs, Group<Rhs...> &&rhs) {
  return Group<GrammarElementType<Lhs>..., GrammarElementType<Rhs>...>{
      std::tuple_cat(std::move(lhs.elements), std::move(rhs.elements))};
}

template <typename... Lhs, typename Rhs>
  requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
constexpr auto operator+(Group<Lhs...> &&lhs, Rhs &&rhs) {
  return Group<GrammarElementType<Lhs>..., GrammarElementType<Rhs>>{
      std::tuple_cat(std::move(lhs.elements), std::forward_as_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
  requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
constexpr auto operator+(Lhs &&lhs, Group<Rhs...> &&rhs) {
  return Group<GrammarElementType<Lhs>, GrammarElementType<Rhs>...>{
      std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs.elements))};
}

template <typename Lhs, typename Rhs>
  requires IsGrammarElement<Lhs> && IsGrammarElement<Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return Group<GrammarElementType<Lhs>, GrammarElementType<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}
} // namespace pegium::grammar