#pragma once

/// Helpers that parse or probe expressions while preserving caller state on failure.

#include <concepts>
#include <type_traits>

#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/ParseMode.hpp>

namespace pegium::parser {

template <typename E, typename = void>
struct IsFailureSafeExpression : std::false_type {};

template <typename E>
struct IsFailureSafeExpression<
    E, std::void_t<decltype(std::remove_cvref_t<E>::isFailureSafe)>>
    : std::bool_constant<std::remove_cvref_t<E>::isFailureSafe> {};

template <Expression E>
inline constexpr bool requires_checkpoint_on_failure_v =
    !IsFailureSafeExpression<E>::value;

namespace detail {

template <typename E>
concept LocalRecoveryProbeCapableExpression =
    Expression<E> &&
    requires(const std::remove_cvref_t<E> &expression, RecoveryContext &ctx) {
      { expression.probeRecoverable(ctx) } -> std::same_as<bool>;
    };

struct FastProbeAccess {
  template <typename Expr, typename Context>
    requires requires(const Expr &expression, Context &ctx) {
      { expression.fast_probe_impl(ctx) } -> std::same_as<bool>;
    }
  static bool probe(const Expr &expression, Context &ctx) {
    return expression.fast_probe_impl(ctx);
  }
};

template <typename E, typename Context>
concept FastProbeCapableExpression =
    Expression<E> &&
    requires(const std::remove_cvref_t<E> &expression, Context &ctx) {
      { FastProbeAccess::probe(expression, ctx) } -> std::same_as<bool>;
    };

template <Expression E>
inline bool attempt_parse_recovery_strict_view(RecoveryContext &ctx,
                                               const E &expression) {
  if constexpr (!requires_checkpoint_on_failure_v<E>) {
    TrackedParseContext &strictCtx = ctx;
    return parse(expression, strictCtx);
  } else {
    const auto checkpoint = ctx.mark();
    if (TrackedParseContext &strictCtx = ctx; parse(expression, strictCtx)) {
      return true;
    }
    ctx.rewind(checkpoint);
    return false;
  }
}

} // namespace detail

template <Expression E, StrictParseModeContext Context>
inline bool attempt_parse_strict(Context &ctx, const E &expression) {
  if constexpr (!requires_checkpoint_on_failure_v<E>) {
    return parse(expression, ctx);
  } else {
    const auto checkpoint = ctx.mark();
    if (parse(expression, ctx)) {
      return true;
    }
    ctx.rewind(checkpoint);
    return false;
  }
}

template <Expression E>
inline bool attempt_parse_no_edits(RecoveryContext &ctx, const E &expression) {
  auto noEditGuard = ctx.withEditState(false, false, false);
  (void)noEditGuard;
  return detail::attempt_parse_recovery_strict_view(ctx, expression);
}

template <Expression E>
inline bool attempt_parse_editable(RecoveryContext &ctx, const E &expression) {
  if (!ctx.isInRecoveryPhase()) {
    return detail::attempt_parse_recovery_strict_view(ctx, expression);
  }
  if constexpr (!requires_checkpoint_on_failure_v<E>) {
    return parse(expression, ctx);
  } else {
    const auto checkpoint = ctx.mark();
    if (parse(expression, ctx)) {
      return true;
    }
    ctx.rewind(checkpoint);
    return false;
  }
}

template <Expression E, StrictParseModeContext Context>
inline bool attempt_fast_probe(Context &ctx, const E &expression) {
  if constexpr (detail::FastProbeCapableExpression<E, Context>) {
    return detail::FastProbeAccess::probe(expression, ctx);
  } else {
    return probe(expression, ctx);
  }
}

template <Expression E>
inline bool attempt_fast_probe(RecoveryContext &ctx, const E &expression) {
  if constexpr (detail::FastProbeCapableExpression<E, RecoveryContext>) {
    return detail::FastProbeAccess::probe(expression, ctx);
  } else {
    TrackedParseContext &strictCtx = ctx;
    return probe(expression, strictCtx);
  }
}

template <Expression E>
inline bool attempt_fast_probe(ExpectContext &ctx, const E &expression) {
  return parse(expression, ctx);
}

template <Expression E>
inline bool probe_locally_recoverable(const E &expression, RecoveryContext &ctx) {
  if constexpr (detail::LocalRecoveryProbeCapableExpression<E>) {
    return expression.probeRecoverable(ctx);
  } else {
    (void)expression;
    (void)ctx;
    return false;
  }
}

} // namespace pegium::parser
