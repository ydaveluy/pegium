#pragma once

/// Helpers that parse or probe expressions while preserving caller state on failure.

#include <algorithm>
#include <concepts>
#include <memory>
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
  static bool probe(const Expr &expression, Context &ctx)
    requires requires {
      { expression.fast_probe_impl(ctx) } -> std::same_as<bool>;
    }
  {
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
inline bool probe_started_without_edits(RecoveryContext &ctx,
                                        const E &expression);

struct NoEditParseObservation {
  bool matched = false;
  bool startedWithoutEdits = false;
};

template <Expression E>
inline NoEditParseObservation observe_no_edit_parse(RecoveryContext &ctx,
                                                    const E &expression) {
  const auto startOffset = ctx.cursorOffset();
  const auto furthestExploredOffsetBefore = ctx.furthestExploredOffset();
  auto noEditGuard = ctx.withEditState(false, false, false);
  (void)noEditGuard;
  TrackedParseContext &strictCtx = ctx;
  bool matched = false;
  if constexpr (!requires_checkpoint_on_failure_v<E>) {
    matched = parse(expression, strictCtx);
  } else {
    const auto checkpoint = ctx.mark();
    matched = parse(expression, strictCtx);
    if (!matched) {
      bool startedWithoutEdits =
          ctx.cursorOffset() > startOffset ||
          ctx.furthestExploredOffset() >
              std::max(startOffset, furthestExploredOffsetBefore);
      ctx.rewind(checkpoint);
      if (!startedWithoutEdits) {
        startedWithoutEdits = probe_started_without_edits(ctx, expression);
      }
      return {
          .matched = false,
          .startedWithoutEdits = startedWithoutEdits,
      };
    }
  }
  if (!matched) {
    bool startedWithoutEdits =
        ctx.cursorOffset() > startOffset ||
        ctx.furthestExploredOffset() >
            std::max(startOffset, furthestExploredOffsetBefore);
    if (!startedWithoutEdits) {
      startedWithoutEdits = probe_started_without_edits(ctx, expression);
    }
    return {
        .matched = false,
        .startedWithoutEdits = startedWithoutEdits,
    };
  }
  return {
      .matched = matched,
      .startedWithoutEdits =
          matched || ctx.cursorOffset() > startOffset ||
          ctx.furthestExploredOffset() >
              std::max(startOffset, furthestExploredOffsetBefore),
  };
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
  const auto expressionIdentity =
      static_cast<const void *>(std::addressof(expression));
  const auto startOffset = ctx.cursorOffset();
  const auto memoKey = RecoveryContext::RecoveryMemoKey{
      .queryKind = RecoveryContext::RecoveryMemoQueryKind::FastProbe,
      .ownerIdentity = expressionIdentity,
      .cursorOffset = startOffset,
      .furthestExploredOffset =
          RecoveryContext::RecoveryMemoTable::kNoFurthestExploredOffset,
      .policySignature = ctx.recoveryProbeMemoSignature(),
  };
  RecoveryContext::RecoveryMemoTable::ValueFor<
      RecoveryContext::RecoveryMemoQueryKind::FastProbe>
      cachedResult{};
  if (ctx.memoTable().template tryGet<
          RecoveryContext::RecoveryMemoQueryKind::FastProbe>(memoKey,
                                                             cachedResult)) {
    const char *const cachedFurthestExploredCursor =
        ctx.begin + cachedResult.observedFurthestExploredOffset;
    if (cachedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(cachedFurthestExploredCursor);
    }
    return cachedResult.result;
  }
  const auto result = [&]() {
    if constexpr (detail::FastProbeCapableExpression<E, RecoveryContext>) {
      return detail::FastProbeAccess::probe(expression, ctx);
    } else if constexpr (detail::FastProbeCapableExpression<E,
                                                            TrackedParseContext>) {
      TrackedParseContext &strictCtx = ctx;
      return detail::FastProbeAccess::probe(expression, strictCtx);
    } else {
      TrackedParseContext &strictCtx = ctx;
      return probe(expression, strictCtx);
    }
  }();
  ctx.memoTable().template store<
      RecoveryContext::RecoveryMemoQueryKind::FastProbe>(
      memoKey,
      {.observedFurthestExploredOffset = ctx.furthestExploredOffset(),
       .result = result});
  return result;
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

template <Expression E>
inline bool probe_recoverable_at_entry(const E &expression,
                                       RecoveryContext &ctx) {
  if constexpr (requires {
                  { expression.probeRecoverableAtEntry(ctx) } ->
                      std::same_as<bool>;
                }) {
    const auto expressionIdentity =
        static_cast<const void *>(std::addressof(expression));
    const auto startOffset = ctx.cursorOffset();
    const auto policySignature = ctx.recoveryPolicySignature();
    const auto memoKey = RecoveryContext::RecoveryMemoKey{
        .queryKind = RecoveryContext::RecoveryMemoQueryKind::EntryRecoverable,
        .ownerIdentity = expressionIdentity,
        .cursorOffset = startOffset,
        .furthestExploredOffset =
            RecoveryContext::RecoveryMemoTable::kNoFurthestExploredOffset,
        .policySignature = policySignature,
        .activeRecoverySignature = ctx.activeRecoverySignature(),
    };
    RecoveryContext::RecoveryMemoTable::ValueFor<
        RecoveryContext::RecoveryMemoQueryKind::EntryRecoverable>
        cachedResult{};
    if (ctx.memoTable().template tryGet<
            RecoveryContext::RecoveryMemoQueryKind::EntryRecoverable>(
            memoKey, cachedResult)) {
      const char *const cachedFurthestExploredCursor =
          ctx.begin + cachedResult.observedFurthestExploredOffset;
      if (cachedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(cachedFurthestExploredCursor);
      }
      return cachedResult.result;
    }
    const bool result = expression.probeRecoverableAtEntry(ctx);
    ctx.memoTable().template store<
        RecoveryContext::RecoveryMemoQueryKind::EntryRecoverable>(
        memoKey,
        {.observedFurthestExploredOffset = ctx.furthestExploredOffset(),
         .result = result});
    return result;
  } else {
    return false;
  }
}

template <Expression E>
inline bool probe_started_without_edits(RecoveryContext &ctx,
                                        const E &expression) {
  const auto expressionIdentity =
      static_cast<const void *>(std::addressof(expression));
  const auto startOffset = ctx.cursorOffset();
  const auto memoKey = RecoveryContext::RecoveryMemoKey{
      .queryKind =
          RecoveryContext::RecoveryMemoQueryKind::StartedWithoutEdits,
      .ownerIdentity = expressionIdentity,
      .cursorOffset = startOffset,
      .furthestExploredOffset =
          RecoveryContext::RecoveryMemoTable::kNoFurthestExploredOffset,
      .policySignature = ctx.recoveryProbeMemoSignature(),
  };
  RecoveryContext::RecoveryMemoTable::ValueFor<
      RecoveryContext::RecoveryMemoQueryKind::StartedWithoutEdits>
      cachedResult{};
  if (ctx.memoTable().template tryGet<
          RecoveryContext::RecoveryMemoQueryKind::StartedWithoutEdits>(
          memoKey, cachedResult)) {
    const char *const cachedFurthestExploredCursor =
        ctx.begin + cachedResult.observedFurthestExploredOffset;
    if (cachedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(cachedFurthestExploredCursor);
    }
    return cachedResult.result;
  }
  const auto checkpoint = ctx.mark();
  const char *const savedFurthestExploredCursor =
      ctx.furthestExploredCursor();
  ctx.restoreFurthestExploredCursor(ctx.cursor());
  if (attempt_fast_probe(ctx, expression)) {
    const auto observedFurthestExploredOffset = ctx.furthestExploredOffset();
    ctx.rewind(checkpoint);
    ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    ctx.memoTable().template store<
        RecoveryContext::RecoveryMemoQueryKind::StartedWithoutEdits>(
        memoKey,
        {.observedFurthestExploredOffset = observedFurthestExploredOffset,
         .result = true});
    return true;
  }
  (void)attempt_parse_no_edits(ctx, expression);
  const bool started =
      ctx.cursorOffset() > startOffset ||
      ctx.furthestExploredOffset() > startOffset;
  const auto observedFurthestExploredOffset = ctx.furthestExploredOffset();
  ctx.rewind(checkpoint);
  ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
  ctx.memoTable().template store<
      RecoveryContext::RecoveryMemoQueryKind::StartedWithoutEdits>(
      memoKey,
      {.observedFurthestExploredOffset = observedFurthestExploredOffset,
       .result = started});
  return started;
}

} // namespace pegium::parser
