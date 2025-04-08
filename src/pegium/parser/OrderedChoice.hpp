#pragma once

#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <ParserExpression... Elements>
struct OrderedChoice final : grammar::OrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  // constexpr ~OrderedChoice() override = default;

  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {}

  template <std::size_t I = 0>
  constexpr MatchResult parse_rule_impl(std::string_view sv, CstNode &parent,
                                        IContext &c) const {
    if constexpr (I == sizeof...(Elements)) {
      return MatchResult::failure(sv.begin());
    } else {
      auto r = std::get<I>(_elements).parse_rule(sv, parent, c);
      return r ? r : parse_rule_impl<I + 1>(sv, parent, c);
    }
  }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    return parse_rule_impl(sv, parent, c);
  }

  template <std::size_t I = 0>
  constexpr MatchResult
  parse_terminal_impl(std::string_view sv) const noexcept {
    if constexpr (I == sizeof...(Elements)) {
      return MatchResult::failure(sv.begin());
    } else {
      auto r = std::get<I>(_elements).parse_terminal(sv);
      return r ? r : parse_terminal_impl<I + 1>(sv);
    }
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal_impl(sv);
  }

  void print(std::ostream &os) const override {
    os << '(';
    bool first = true;
    std::apply(
        [&](auto const &...elems) {
          ((os << (first ? "" : " | "), os << elems, first = false), ...);
        },
        _elements);

    os << ')';
  }

private:
  std::tuple<Elements...> _elements;

  template <ParserExpression... Rhs>
  friend constexpr auto operator|(OrderedChoice &&lhs,
                                  OrderedChoice<Rhs...> &&rhs) {
    return OrderedChoice<Elements..., ParserExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParserExpression Rhs>
  friend constexpr auto operator|(OrderedChoice &&lhs, Rhs &&rhs) {
    return OrderedChoice<Elements..., ParserExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(rhs))};
  }
  template <ParserExpression Lhs>
  friend constexpr auto operator|(Lhs &&lhs, OrderedChoice &&rhs) {
    return OrderedChoice<ParserExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs._elements))};
  }
};

template <ParserExpression Lhs, ParserExpression Rhs>
constexpr auto operator|(Lhs &&lhs, Rhs &&rhs) {
  return OrderedChoice<ParserExpressionHolder<Lhs>,
                       ParserExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

template <typename T> struct IsOrderedChoice : std::false_type {};

template <typename... E>
struct IsOrderedChoice<OrderedChoice<E...>> : std::true_type {};

} // namespace pegium::parser