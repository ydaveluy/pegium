#pragma once
#include <pegium/grammar/Group.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/IContext.hpp>
#include <string_view>

namespace pegium::parser {

template <ParserExpression... Elements> struct Group final : grammar::Group {
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");
  // constexpr ~Group() override = default;

  constexpr explicit Group(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {}

      constexpr Group(Group &&) = default;
      constexpr Group(const Group &) = default;
      constexpr Group &operator=(Group &&) = default;
      constexpr Group &operator=(const Group &) = default;
  template <std::size_t I = 0>
  constexpr MatchResult parse_rule_impl(std::string_view sv, CstNode &parent,
                                        IContext &c, MatchResult r) const {
    if constexpr (I == sizeof...(Elements)) {
      return r;
    } else {
      r = std::get<I>(_elements).parse_rule({r.offset, sv.end()}, parent, c);
      return r ? parse_rule_impl<I + 1>(sv, parent, c, r) : r;
    }
  }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    auto size = parent.content.size();
    auto i = parse_rule_impl(sv, parent, c, MatchResult::failure(sv.begin()));
    if (!i) {
      while (parent.content.size() > size)
        parent.content.pop_back();
    }
    return i;
  }

  template <std::size_t I = 0>
  constexpr MatchResult parse_terminal_impl(std::string_view sv,
                                            MatchResult r) const noexcept {
    if constexpr (I == sizeof...(Elements)) {
      return r;
    } else {
      auto next_r = std::get<I>(_elements).parse_terminal({r.offset, sv.end()});
      return next_r ? parse_terminal_impl<I + 1>(sv, next_r) : next_r;
    }
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal_impl(sv, MatchResult::failure(sv.begin()));
  }
  void print(std::ostream &os) const override {
    os << '(';
    bool first = true;
    std::apply(
        [&](auto const &...elems) {
          ((os << (first ? "" : " "), os << elems, first = false), ...);
        },
        _elements);

    os << ')';
  }

private:
  std::tuple<Elements...> _elements;
  /*std::array<const IGrammarElement *, sizeof...(Elements)> _elementsPtr;

  constexpr void rebuildPtrCache() {
    _elementsPtr = makePtrCache(
        _elements, std::make_index_sequence<sizeof...(Elements)>{});
    _begin = _elementsPtr.begin();
    _end = _elementsPtr.end();
  }
  template <std::size_t... I>
  static constexpr std::array<const IGrammarElement *, sizeof...(Elements)>
  makePtrCache(std::tuple<Elements...> &tuple, std::index_sequence<I...>) {
    return {(&std::get<I>(tuple))...};
  }*/

  template <ParserExpression... Rhs>
  friend constexpr auto operator+(Group &&lhs, Group<Rhs...> &&rhs) {
    return Group<Elements..., ParserExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParserExpression Rhs>
  friend constexpr auto operator+(Group &&lhs, Rhs &&rhs) {
    return Group<Elements..., ParserExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(rhs))};
  }
  template <ParserExpression Lhs>
  friend constexpr auto operator+(Lhs &&lhs, Group &&rhs) {
    return Group<ParserExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs._elements))};
  }
};

template <ParserExpression Lhs, ParserExpression Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return Group<ParserExpressionHolder<Lhs>, ParserExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}
} // namespace pegium::parser