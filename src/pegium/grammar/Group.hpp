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

  template <typename T>
  constexpr bool parse_rule_element(const T &element, std::string_view sv,
                                    CstNode &parent, IContext &c,
                                    std::size_t size, std::size_t &i) const {

    auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
    if (fail(len)) {
      parent.content.resize(size);
      i = len;
      return false;
    }
    i += len;
    return true;
  }

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    std::size_t i = 0;

    std::apply(
        [&](const auto &...element) {
          auto size = parent.content.size();
          (parse_rule_element(element, sv, parent, c, size, i) && ...);
        },
        elements);

    return i;
  }

  template <typename T>
  constexpr bool parse_terminal_element(const T &element, std::string_view sv,
                                        std::size_t &i) const noexcept {

    auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
    if (fail(len)) {
      i = len;
      return false;
    }
    i += len;
    return true;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t i = 0;

    std::apply(
        [&](const auto &...element) {
          (parse_terminal_element(element, sv, i) && ...);
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

//private:
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