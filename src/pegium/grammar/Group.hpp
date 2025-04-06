#pragma once
#include <pegium/grammar/IContext.hpp>
#include <pegium/grammar/IGrammarElement.hpp>
#include <string_view>

namespace pegium::grammar {

struct AbstractGroup : IGrammarElement {

  const IGrammarElement *const *begin() const noexcept;
  const IGrammarElement *const *end() const noexcept;
  GrammarElementKind getKind() const noexcept final;
  void print(std::ostream &os) const final;

protected:
  IGrammarElement *const *_begin;
  IGrammarElement *const *_end;
};

template <typename... Elements>
  requires(IsGrammarElement<Elements> && ...)
struct Group final : AbstractGroup {
  static_assert(sizeof...(Elements) > 0,
                "A Group shall contains at least 1 element.");
  constexpr ~Group() override = default;

  constexpr explicit Group(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {
    rebuildPtrCache();
  }

  constexpr Group(const Group &other) : _elements(other._elements) {
    rebuildPtrCache();
  }

  constexpr Group(Group &&other) noexcept
      : _elements(std::move(other._elements)) {
    rebuildPtrCache();
  }

  constexpr Group &operator=(const Group &other) {
    if (this != &other) {
      _elements = other._elements;
      rebuildPtrCache();
    }
    return *this;
  }
  constexpr Group &operator=(Group &&other) noexcept {
    if (this != &other) {
      _elements = std::move(other._elements);
      rebuildPtrCache();
    }
    return *this;
  }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    MatchResult i = MatchResult::success(sv.begin());
    auto size = parent.content.size();
    std::apply(
        [&](const auto &...element) {
          ((i = element.parse_rule({i.offset, sv.end()}, parent, c)) && ...);
        },
        _elements);
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
  friend constexpr auto operator+(Group<Lhs...> &&lhs, Group<Rhs...> &&rhs);

  template <typename... Lhs, typename Rhs>
    requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
  friend constexpr auto operator+(Group<Lhs...> &&lhs, Rhs &&rhs);

  template <typename Lhs, typename... Rhs>
    requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
  friend constexpr auto operator+(Lhs &&lhs, Group<Rhs...> &&rhs);

  template <typename Lhs, typename Rhs>
    requires IsGrammarElement<Lhs> && IsGrammarElement<Rhs>
  friend constexpr auto operator+(Lhs &&lhs, Rhs &&rhs);
};

template <typename... Lhs, typename... Rhs>
  requires(IsGrammarElement<Lhs> && ...) && (IsGrammarElement<Rhs> && ...)
constexpr auto operator+(Group<Lhs...> &&lhs, Group<Rhs...> &&rhs) {
  return Group<GrammarElementType<Lhs>..., GrammarElementType<Rhs>...>{
      std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
}

template <typename... Lhs, typename Rhs>
  requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
constexpr auto operator+(Group<Lhs...> &&lhs, Rhs &&rhs) {
  return Group<GrammarElementType<Lhs>..., GrammarElementType<Rhs>>{
      std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
  requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
constexpr auto operator+(Lhs &&lhs, Group<Rhs...> &&rhs) {
  return Group<GrammarElementType<Lhs>, GrammarElementType<Rhs>...>{
      std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs._elements))};
}

template <typename Lhs, typename Rhs>
  requires IsGrammarElement<Lhs> && IsGrammarElement<Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return Group<GrammarElementType<Lhs>, GrammarElementType<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}
} // namespace pegium::grammar