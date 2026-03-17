#pragma once

#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/SkipperContext.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <typename Tuple> struct HiddenRules {
  Tuple rules;
};

template <typename Tuple> struct IgnoredRules {
  Tuple rules;
};

template <typename RulesTuple>
HiddenRules(RulesTuple &&) -> HiddenRules<std::remove_cvref_t<RulesTuple>>;

template <typename RulesTuple>
IgnoredRules(RulesTuple &&) -> IgnoredRules<std::remove_cvref_t<RulesTuple>>;

template <NonNullableTerminalCapableExpression... Rules>
[[nodiscard]] auto hidden(Rules &&...rules) {
  return HiddenRules{
      std::tuple<ExpressionHolder<Rules>...>(std::forward<Rules>(rules)...)};
}

template <NonNullableTerminalCapableExpression... Rules>
[[nodiscard]] auto ignored(Rules &&...rules) {
  return IgnoredRules{
      std::tuple<ExpressionHolder<Rules>...>(std::forward<Rules>(rules)...)};
}

namespace detail {

template <typename T> struct IsHiddenRules : std::false_type {};
template <typename Tuple> struct IsHiddenRules<HiddenRules<Tuple>> : std::true_type {};

template <typename T> struct IsIgnoredRules : std::false_type {};
template <typename Tuple>
struct IsIgnoredRules<IgnoredRules<Tuple>> : std::true_type {};

template <typename T>
inline constexpr bool IsHiddenRules_v =
    IsHiddenRules<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr bool IsIgnoredRules_v =
    IsIgnoredRules<std::remove_cvref_t<T>>::value;

template <typename Part>
[[nodiscard]] auto hidden_tuple_from(Part &&) {
  return std::tuple<>{};
}

template <typename Tuple>
[[nodiscard]] auto hidden_tuple_from(HiddenRules<Tuple> spec) {
  return std::move(spec.rules);
}

template <typename Part>
[[nodiscard]] auto ignored_tuple_from(Part &&) {
  return std::tuple<>{};
}

template <typename Tuple>
[[nodiscard]] auto ignored_tuple_from(IgnoredRules<Tuple> spec) {
  return std::move(spec.rules);
}

} // namespace detail

[[nodiscard]] inline auto skip() {
  return Skipper::owning(SkipperContext{std::tuple<>{}, std::tuple<>{}});
}

template <typename... Parts>
  requires((... && (detail::IsHiddenRules_v<Parts> ||
                     detail::IsIgnoredRules_v<Parts>)))
[[nodiscard]] auto skip(Parts &&...parts) {
  auto hiddenRules =
      std::tuple_cat(detail::hidden_tuple_from(std::forward<Parts>(parts))...);
  auto ignoredRules =
      std::tuple_cat(detail::ignored_tuple_from(std::forward<Parts>(parts))...);
  using SkipperContextType =
      SkipperContext<decltype(hiddenRules), decltype(ignoredRules)>;
  return Skipper::owning(
      SkipperContextType{std::move(hiddenRules), std::move(ignoredRules)});
}

template <typename HiddenTuple = std::tuple<>,
          typename IgnoredTuple = std::tuple<>>
struct SkipperBuilder {
  explicit SkipperBuilder(HiddenTuple &&hiddenRules = std::tuple<>{},
                          IgnoredTuple &&ignoredRules = std::tuple<>{})
      : _hiddenRules{std::move(hiddenRules)},
        _ignoredRules{std::move(ignoredRules)} {}

  template <NonNullableTerminalCapableExpression... Rules>
    requires((std::tuple_size_v<IgnoredTuple> == 0))
  [[nodiscard]] auto ignore(Rules &&...rules) {
    return SkipperBuilder<HiddenTuple, decltype(ignored(std::forward<Rules>(rules)...).rules)>{
        std::move(_hiddenRules),
        ignored(std::forward<Rules>(rules)...).rules};
  }

  template <NonNullableTerminalCapableExpression... Rules>
    requires((std::tuple_size_v<HiddenTuple> == 0))
  [[nodiscard]] auto hide(Rules &&...rules) {
    return SkipperBuilder<decltype(hidden(std::forward<Rules>(rules)...).rules),
                          IgnoredTuple>{
        hidden(std::forward<Rules>(rules)...).rules,
        std::move(_ignoredRules)};
  }

  [[nodiscard]] auto build() {
    return skip(HiddenRules{std::move(_hiddenRules)},
                IgnoredRules{std::move(_ignoredRules)});
  }

private:
  HiddenTuple _hiddenRules;
  IgnoredTuple _ignoredRules;
};

SkipperBuilder() -> SkipperBuilder<>;

} // namespace pegium::parser
