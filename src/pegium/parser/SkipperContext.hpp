#pragma once

#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <string>
#include <string_view>
#include <tuple>

namespace pegium::parser {

template <typename HiddenTuple, typename IgnoredTuple> struct SkipperContext;

template <typename... Hidden, typename... Ignored>
  requires((NonNullableTerminalCapableExpression<Hidden> && ...) &&
           (NonNullableTerminalCapableExpression<Ignored> && ...))
struct SkipperContext<std::tuple<Hidden...>, std::tuple<Ignored...>> final {

  template <typename... H, typename... I>
  SkipperContext(std::tuple<H...> &&hiddenRules, std::tuple<I...> &&ignoredRules)
      : _hiddenRules{std::move(hiddenRules)},
        _ignoredRules{std::move(ignoredRules)},
        _withoutBuilderRules{std::tuple_cat(_ignoredRules, _hiddenRules)} {}

  [[nodiscard]] const char *skip(const char *inputBegin,
                                 CstBuilder &builder) const noexcept {
    const char *scanCursor = inputBegin;
    const char *const builderInputBegin = builder.input_begin();

    while (true) {
      if constexpr (sizeof...(Ignored) > 0) {
        while (const char *const matchEnd = parse_any_ignored(scanCursor)) {
          scanCursor = matchEnd;
        }
      }

      if constexpr (sizeof...(Hidden) == 0) {
        return scanCursor;
      }

      const char *const hiddenMatchEnd =
          parse_any_hidden(scanCursor, builder, builderInputBegin);
      if (hiddenMatchEnd == nullptr) {
        return scanCursor;
      }
      scanCursor = hiddenMatchEnd;
    }
  }

  [[nodiscard]] const char *skip(const std::string &text,
                                 CstBuilder &builder) const noexcept {
    return skip(text.c_str(), builder);
  }

  [[nodiscard]] const char *skip(const char *inputBegin) const noexcept {
    const char *scanCursor = inputBegin;

    while (const char *const matchEnd =
               parse_any_without_builder(_withoutBuilderRules, scanCursor)) {
      scanCursor = matchEnd;
    }
    return scanCursor;
  }

  [[nodiscard]] const char *skip(const std::string &text) const noexcept {
    return skip(text.c_str());
  }

  [[nodiscard]] operator Skipper() const noexcept {
    return Skipper::from(*this);
  }

private:
  template <std::size_t I = 0>
  [[nodiscard]] const char *parse_any_ignored(const char *cursor) const noexcept {
    if constexpr (I == sizeof...(Ignored)) {
      return nullptr;
    } else {
      const auto &element = std::get<I>(_ignoredRules);
      if (const char *const matchEnd = element.terminal(cursor)) {
        return matchEnd;
      }
      return parse_any_ignored<I + 1>(cursor);
    }
  }

  template <std::size_t I = 0>
  [[nodiscard]] const char *parse_any_hidden(const char *cursor,
                                             CstBuilder &builder,
                                             const char *builderInputBegin) const
      noexcept {
    if constexpr (I == sizeof...(Hidden)) {
      return nullptr;
    } else {
      const auto &element = std::get<I>(_hiddenRules);
      if (const char *const matchEnd = element.terminal(cursor)) {
        builder.leaf(
            static_cast<TextOffset>(cursor - builderInputBegin),
            static_cast<TextOffset>(matchEnd - builderInputBegin),
            std::addressof(element), true);
        return matchEnd;
      }
      return parse_any_hidden<I + 1>(cursor, builder, builderInputBegin);
    }
  }

  template <typename Tuple, std::size_t I = 0>
  [[nodiscard]] static const char *parse_any_without_builder(
      const Tuple &rules, const char *cursor) noexcept {
    if constexpr (I == std::tuple_size_v<Tuple>) {
      return nullptr;
    } else {
      const auto &element = std::get<I>(rules);
      if (const char *const matchEnd = element.terminal(cursor)) {
        return matchEnd;
      }
      return parse_any_without_builder<Tuple, I + 1>(rules, cursor);
    }
  }

  std::tuple<Hidden...> _hiddenRules;
  std::tuple<Ignored...> _ignoredRules;
  std::tuple<Ignored..., Hidden...> _withoutBuilderRules;
};

template <NonNullableTerminalCapableExpression... H,
          NonNullableTerminalCapableExpression... I>
SkipperContext(std::tuple<H...> &&, std::tuple<I...> &&)
    -> SkipperContext<std::tuple<H...>, std::tuple<I...>>;

} // namespace pegium::parser
