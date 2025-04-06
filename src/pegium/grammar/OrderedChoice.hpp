#pragma once

#include <pegium/grammar/IGrammarElement.hpp>

// #define INLINE_RULES
namespace pegium::grammar {

struct AbstractOrderedChoice : IGrammarElement {
  const IGrammarElement *const *begin() const noexcept;
  const IGrammarElement *const *end() const noexcept;
  GrammarElementKind getKind() const noexcept final;
  void print(std::ostream &os) const final;

protected:
  const IGrammarElement *const *_begin;
  IGrammarElement *const *_end;
};

template <typename... Elements>
  requires(IsGrammarElement<Elements> && ...)
struct OrderedChoice final : AbstractOrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  constexpr ~OrderedChoice() override = default;

  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {

    rebuildPtrCache();
  }

  OrderedChoice(const OrderedChoice &other) : _elements(other._elements) {

    rebuildPtrCache();
  }

  OrderedChoice(OrderedChoice &&other) noexcept
      : _elements(std::move(other._elements)) {

    rebuildPtrCache();
  }

  OrderedChoice &operator=(const OrderedChoice &other) {
    if (this != &other) {
      _elements = other._elements;
      rebuildPtrCache();
    }
    return *this;
  }
  OrderedChoice &operator=(OrderedChoice &&other) noexcept {
    if (this != &other) {
      _elements = std::move(other._elements);
      rebuildPtrCache();
    }
    return *this;
  }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {

    MatchResult i = MatchResult::failure(sv.begin());
    std::apply(
        [&](const auto &...element) {
          ((i |= element.parse_rule(sv, parent, c)) || ...);
        },
        _elements);

    return i;
  }

  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    MatchResult i = MatchResult::failure(sv.begin());

    std::apply(
        [&](const auto &...element) {
          ((i |= element.parse_terminal(sv)) || ...);
        },
        _elements);
    return i;
  }
private:
  std::tuple<Elements...> _elements;

  std::array<IGrammarElement *, sizeof...(Elements)> _elementsPtr;
  constexpr void rebuildPtrCache() {
    _elementsPtr = makePtrCache(
        _elements, std::make_index_sequence<sizeof...(Elements)>{});
    _begin = _elementsPtr.begin();
    _end = _elementsPtr.end();
  }
  template <std::size_t... I>
  static constexpr std::array<IGrammarElement *, sizeof...(Elements)>
  makePtrCache(std::tuple<Elements...> &tuple, std::index_sequence<I...>) {
    return {(&std::get<I>(tuple))...};
  }

  template <typename... Lhs, typename... Rhs>
    requires(IsGrammarElement<Lhs> && ...) && (IsGrammarElement<Rhs> && ...)
  friend constexpr auto operator|(OrderedChoice<Lhs...> &&lhs,
                                  OrderedChoice<Rhs...> &&rhs);
  template <typename... Lhs, typename Rhs>
    requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
  friend constexpr auto operator|(OrderedChoice<Lhs...> &&lhs, Rhs &&rhs);
  template <typename Lhs, typename... Rhs>
    requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
  friend constexpr auto operator|(Lhs &&lhs, OrderedChoice<Rhs...> &&rhs);
};

template <typename... Lhs, typename... Rhs>
  requires(IsGrammarElement<Lhs> && ...) && (IsGrammarElement<Rhs> && ...)
constexpr auto operator|(OrderedChoice<Lhs...> &&lhs,
                         OrderedChoice<Rhs...> &&rhs) {
  return OrderedChoice<GrammarElementType<Lhs>..., GrammarElementType<Rhs>...>{
      std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
}

template <typename... Lhs, typename Rhs>
  requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
constexpr auto operator|(OrderedChoice<Lhs...> &&lhs, Rhs &&rhs) {
  return OrderedChoice<GrammarElementType<Lhs>..., GrammarElementType<Rhs>>{
      std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
  requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
constexpr auto operator|(Lhs &&lhs, OrderedChoice<Rhs...> &&rhs) {
  return OrderedChoice<GrammarElementType<Lhs>, GrammarElementType<Rhs>...>{
      std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs._elements))};
}

template <typename Lhs, typename Rhs>
  requires IsGrammarElement<Lhs> && IsGrammarElement<Rhs>
constexpr auto operator|(Lhs &&lhs, Rhs &&rhs) {
  return OrderedChoice<GrammarElementType<Lhs>, GrammarElementType<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

template <typename T> struct IsOrderedChoice : std::false_type {};

template <typename... E>
struct IsOrderedChoice<OrderedChoice<E...>> : std::true_type {};

} // namespace pegium::grammar