#pragma once

#include <concepts>
#include <pegium/grammar/AbstractElement.hpp>
#include <type_traits>

namespace pegium::parser {

struct MatchResult {
  const char *offset; // pointer to the end of the match
  bool valid;         // validity status of the match
  [[nodiscard]] constexpr bool IsValid() const noexcept { return valid; }

  [[nodiscard]] constexpr static MatchResult failure(const char *offset) {
    return {offset, false};
  }

  [[nodiscard]] constexpr static MatchResult success(const char *offset) {
    return {offset, true};
  }
};

struct ParseContext;

template <typename T>
concept ParseExpression =
    std::derived_from<std::remove_cvref_t<T>, grammar::AbstractElement> &&
    requires(const std::remove_cvref_t<T> &t, const char *begin,
             const char *end, ParseContext &ctx) {
      { t.terminal(begin, end) } noexcept -> std::same_as<MatchResult>;
      { t.rule(ctx) } -> std::same_as<bool>;
    };

template <ParseExpression T>
using ParseExpressionHolder = std::conditional_t<std::is_lvalue_reference_v<T>,
                                                 T, std::remove_cvref_t<T>>;

} // namespace pegium::parser
