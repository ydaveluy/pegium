#pragma once

/// Parse-mode concepts and dispatch helpers shared by parser expressions.

#include <concepts>
#include <type_traits>
#include <utility>

#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ParseContext.hpp>
namespace pegium::parser {

template <typename Context>
concept StrictParseModeContext =
    std::same_as<std::remove_cvref_t<Context>, ParseContext> ||
    std::same_as<std::remove_cvref_t<Context>, TrackedParseContext>;

template <typename Context>
concept RecoveryParseModeContext =
    std::same_as<std::remove_cvref_t<Context>, RecoveryContext>;

template <typename Context>
concept ExpectParseModeContext =
    std::same_as<std::remove_cvref_t<Context>, ExpectContext>;

template <typename Context>
concept ParseModeContext = StrictParseModeContext<Context> ||
                           RecoveryParseModeContext<Context> ||
                           ExpectParseModeContext<Context>;

template <typename Context>
concept EditableParseModeContext =
    RecoveryParseModeContext<Context> || ExpectParseModeContext<Context>;

namespace detail {

template <typename Expr, typename Context>
concept HasParseImpl = requires(const Expr &expression, Context &ctx) {
  { expression.parse_impl(ctx) } -> std::same_as<bool>;
};

struct ParseAccess {
  template <typename Expr, ParseModeContext Context>
    requires HasParseImpl<Expr, Context>
  static bool parse(const Expr &expression, Context &ctx) {
    return expression.parse_impl(ctx);
  }
};

template <typename Expr, typename Context>
concept ParseAccessAvailable =
    requires(const Expr &expression, Context &ctx) {
      { ParseAccess::parse(expression, ctx) } -> std::same_as<bool>;
    };

template <typename Expr, typename Context>
concept HasProbeImpl = requires(const Expr &expression, Context &ctx) {
  { expression.probe_impl(ctx) } -> std::same_as<bool>;
};

struct ProbeAccess {
  template <typename Expr, StrictParseModeContext Context>
    requires HasProbeImpl<Expr, Context>
  static bool probe(const Expr &expression, Context &ctx) {
    return expression.probe_impl(ctx);
  }

  template <typename Expr, StrictParseModeContext Context>
    requires(!HasProbeImpl<Expr, Context> &&
             ParseAccessAvailable<Expr, Context>)
  static bool probe(const Expr &expression, Context &ctx) {
    const auto checkpoint = ctx.mark();
    const auto maxCursor = ctx.maxCursor();
    const bool matched = ParseAccess::parse(expression, ctx);
    ctx.rewind(checkpoint);
    ctx.restoreMaxCursor(maxCursor);
    return matched;
  }
};

template <typename Expr, typename Context>
concept ProbeAccessAvailable =
    requires(const Expr &expression, Context &ctx) {
      { ProbeAccess::probe(expression, ctx) } -> std::same_as<bool>;
    };

} // namespace detail

template <typename Expr, ParseModeContext Context>
  requires detail::ParseAccessAvailable<Expr, Context>
inline bool parse(const Expr &expression, Context &ctx) {
  return detail::ParseAccess::parse(expression, ctx);
}

template <typename Expr, StrictParseModeContext Context>
  requires detail::ProbeAccessAvailable<Expr, Context>
inline bool probe(const Expr &expression, Context &ctx) {
  return detail::ProbeAccess::probe(expression, ctx);
}

template <ParseModeContext Context, typename Fn>
inline decltype(auto) with_no_edits(Context &ctx, Fn &&fn) {
  if constexpr (EditableParseModeContext<Context>) {
    auto noEditGuard = ctx.withEditState(false, false, false);
    (void)noEditGuard;
    return std::forward<Fn>(fn)();
  } else {
    return std::forward<Fn>(fn)();
  }
}

} // namespace pegium::parser
