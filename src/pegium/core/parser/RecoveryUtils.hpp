#pragma once

/// Shared delete-scan recovery utilities.

#include <concepts>
#include <limits>
#include <utility>

#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/utils/TextUtils.hpp>

namespace pegium::parser::detail {

template <typename Context>
inline void enable_budget_overflow_edits_for_attempt(Context &ctx) noexcept {
  if constexpr (requires { ctx.allowBudgetOverflowEdits(); }) {
    ctx.allowBudgetOverflowEdits();
  }
}

template <typename Context>
[[nodiscard]] constexpr bool
allows_extended_delete_scan(const Context &ctx) noexcept {
  if constexpr (requires { ctx.allowExtendedDeleteScan; }) {
    return ctx.allowExtendedDeleteScan;
  } else {
    return true;
  }
}

template <typename Context> struct ExtendedDeleteScanBudgetScope {
  Context &ctx;
  std::uint32_t savedMaxConsecutiveDeletes;
  std::uint32_t savedMaxEditsPerAttempt;
  std::uint32_t savedMaxEditCost;
  bool enabled = false;

  explicit ExtendedDeleteScanBudgetScope(Context &ctx) noexcept
      : ctx(ctx),
        savedMaxConsecutiveDeletes(ctx.maxConsecutiveCodepointDeletes),
        savedMaxEditsPerAttempt(ctx.maxEditsPerAttempt),
        savedMaxEditCost(ctx.maxEditCost) {}

  ExtendedDeleteScanBudgetScope(const ExtendedDeleteScanBudgetScope &) = delete;
  ExtendedDeleteScanBudgetScope &
  operator=(const ExtendedDeleteScanBudgetScope &) = delete;

  ~ExtendedDeleteScanBudgetScope() noexcept { restore(); }

  [[nodiscard]] bool tryEnable() noexcept {
    if (enabled || !allows_extended_delete_scan(ctx)) {
      return false;
    }
    ctx.maxConsecutiveCodepointDeletes = std::numeric_limits<std::uint32_t>::max();
    ctx.maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
    ctx.maxEditCost = std::numeric_limits<std::uint32_t>::max();
    enabled = true;
    return true;
  }

  void restore() noexcept {
    if (!enabled) {
      return;
    }
    ctx.maxConsecutiveCodepointDeletes = savedMaxConsecutiveDeletes;
    ctx.maxEditsPerAttempt = savedMaxEditsPerAttempt;
    ctx.maxEditCost = savedMaxEditCost;
    enabled = false;
  }

  void commitOverflowEdits() noexcept {
    if (!enabled) {
      return;
    }
    restore();
    enable_budget_overflow_edits_for_attempt(ctx);
  }
};

template <typename Context> struct DeleteRetryReplayScope {
  Context &ctx;
  bool savedAllowDeleteRetry;
  bool savedSkipAfterDelete;
  bool disableDeleteRetry;
  ExtendedDeleteScanBudgetScope<Context> overflowBudgetScope;

  explicit DeleteRetryReplayScope(Context &ctx,
                                  bool disableDeleteRetry = true) noexcept
      : ctx(ctx), savedAllowDeleteRetry(ctx.allowDeleteRetry),
        savedSkipAfterDelete(ctx.skipAfterDelete),
        disableDeleteRetry(disableDeleteRetry), overflowBudgetScope(ctx) {
    if (disableDeleteRetry) {
      ctx.allowDeleteRetry = false;
    }
    ctx.skipAfterDelete = false;
  }

  DeleteRetryReplayScope(const DeleteRetryReplayScope &) = delete;
  DeleteRetryReplayScope &
  operator=(const DeleteRetryReplayScope &) = delete;

  ~DeleteRetryReplayScope() noexcept {
    ctx.skipAfterDelete = savedSkipAfterDelete;
    if (disableDeleteRetry) {
      ctx.allowDeleteRetry = savedAllowDeleteRetry;
    }
  }

  [[nodiscard]] bool tryEnableExtendedDeleteScan() noexcept {
    return overflowBudgetScope.tryEnable();
  }

  void commitOverflowEdits() noexcept { overflowBudgetScope.commitOverflowEdits(); }
};

struct DeleteScanOptions {
  std::uint32_t maxDeletes = std::numeric_limits<std::uint32_t>::max();
  bool allowOverflow = true;
};

enum class DeleteScanVisitResult : std::uint8_t {
  Continue,
  Stop,
  Accept,
};

struct DeleteScanVisitOptions {
  DeleteScanOptions scan{};
  bool extendThroughHiddenTrivia = false;
  bool stopAtHiddenTriviaBoundary = false;
  bool visitAfterHiddenTriviaExtension = false;
};

struct DeleteScanVisitState {
  bool overflowBudget = false;
  bool hiddenTriviaBoundary = false;
  bool hiddenTriviaExtended = false;
  std::uint32_t deleteCount = 0u;
};

using DeleteRetryVisitResult = DeleteScanVisitResult;

struct DeleteRetryOptions {
  DeleteScanOptions scan{};
  bool disableDeleteRetry = true;
  bool extendThroughHiddenTrivia = true;
  bool stopAtHiddenTriviaBoundary = false;
  bool visitAfterHiddenTriviaExtension = true;
  bool stopAtStructuredVisibleSource = false;
  bool stopOverflowAtStructuredVisibleSource = false;
};

using DeleteRetryVisitState = DeleteScanVisitState;

template <typename ShouldRetryFn>
[[nodiscard]] inline bool invoke_delete_retry_predicate(
    ShouldRetryFn &shouldRetryFn, const DeleteRetryVisitState &state) {
  if constexpr (requires { shouldRetryFn(state); }) {
    return shouldRetryFn(state);
  } else {
    return shouldRetryFn();
  }
}

template <typename MatchFn>
[[nodiscard]] inline const char *
invoke_delete_scan_match(MatchFn &matchFn, const char *scanCursor,
                         std::uint32_t deleteCount) {
  if constexpr (requires { matchFn(scanCursor, deleteCount); }) {
    return matchFn(scanCursor, deleteCount);
  } else {
    return matchFn(scanCursor);
  }
}

template <typename OnMatchFn>
inline void invoke_delete_scan_on_match(OnMatchFn &onMatchFn,
                                        const char *matchedEnd,
                                        std::uint32_t deleteCount) {
  if constexpr (requires { onMatchFn(matchedEnd, deleteCount); }) {
    onMatchFn(matchedEnd, deleteCount);
  } else {
    onMatchFn(matchedEnd);
  }
}

template <typename Context>
[[nodiscard]] constexpr bool
position_starts_structured_visible_source(const Context &ctx,
                                          const char *position) noexcept {
  if (position == nullptr || position >= ctx.end) {
    return false;
  }
  if constexpr (requires { ctx.skip_without_builder(position); }) {
    if (ctx.skip_without_builder(position) != position) {
      return false;
    }
  }
  return is_identifier_like_codepoint(decode_utf8_codepoint(position));
}

template <typename VisitFn>
[[nodiscard]] inline DeleteScanVisitResult
invoke_delete_scan_visit(VisitFn &visitFn, const DeleteScanVisitState &state) {
  if constexpr (requires {
                  { visitFn(state) } -> std::same_as<DeleteScanVisitResult>;
                }) {
    return visitFn(state);
  } else if constexpr (requires {
                         { visitFn() } -> std::same_as<DeleteScanVisitResult>;
                       }) {
    return visitFn();
  } else if constexpr (requires { visitFn(state); }) {
    return visitFn(state) ? DeleteScanVisitResult::Accept
                          : DeleteScanVisitResult::Continue;
  } else {
    return visitFn() ? DeleteScanVisitResult::Accept
                     : DeleteScanVisitResult::Continue;
  }
}

template <typename Context, typename CanDeleteStepFn, typename VisitFn>
[[nodiscard]] inline DeleteScanVisitResult
visit_guarded_delete_scan_positions(
    Context &ctx, CanDeleteStepFn &&canDeleteStepFn, VisitFn &&visitFn,
    DeleteScanVisitOptions options = {}) {
  if (!ctx.canDelete()) {
    return DeleteScanVisitResult::Stop;
  }

  const auto recoveryCheckpoint = ctx.mark();
  ExtendedDeleteScanBudgetScope overflowBudgetScope{ctx};
  auto &&canDeleteStep = canDeleteStepFn;
  auto &&visit = visitFn;
  std::uint32_t deleteCount = 0u;
  const auto try_visit_at_current_position =
      [&](const DeleteScanVisitState &state) {
        const auto result = invoke_delete_scan_visit(visit, state);
        if (result == DeleteScanVisitResult::Accept && state.overflowBudget) {
          overflowBudgetScope.commitOverflowEdits();
        }
        return result;
      };
  const auto try_visit_pass = [&](bool overflowBudget) {
    while (deleteCount < options.scan.maxDeletes && canDeleteStep() &&
           ctx.deleteOneCodepoint()) {
      ++deleteCount;
      bool hiddenTriviaBoundary = false;
      if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
        hiddenTriviaBoundary =
            ctx.skip_without_builder(ctx.cursor()) > ctx.cursor();
      }
      const auto visitState = DeleteScanVisitState{
          .overflowBudget = overflowBudget,
          .hiddenTriviaBoundary = hiddenTriviaBoundary,
          .hiddenTriviaExtended = false,
          .deleteCount = deleteCount,
      };
      if (const auto result = try_visit_at_current_position(visitState);
          result != DeleteScanVisitResult::Continue) {
        return result;
      }
      if (!hiddenTriviaBoundary) {
        continue;
      }
      if (options.extendThroughHiddenTrivia) {
        if constexpr (requires { ctx.extendLastDeleteThroughHiddenTrivia(); }) {
          if (ctx.extendLastDeleteThroughHiddenTrivia()) {
            if (!options.visitAfterHiddenTriviaExtension) {
              continue;
            }
            const auto extendedVisitState = DeleteScanVisitState{
                .overflowBudget = overflowBudget,
                .hiddenTriviaBoundary = false,
                .hiddenTriviaExtended = true,
                .deleteCount = deleteCount,
            };
            if (const auto result =
                    try_visit_at_current_position(extendedVisitState);
                result != DeleteScanVisitResult::Continue) {
              return result;
            }
            continue;
          }
        }
      }
      if (options.stopAtHiddenTriviaBoundary) {
        return DeleteScanVisitResult::Stop;
      }
    }
    return DeleteScanVisitResult::Continue;
  };

  if (const auto result = try_visit_pass(false);
      result != DeleteScanVisitResult::Continue) {
    if (result != DeleteScanVisitResult::Accept) {
      ctx.rewind(recoveryCheckpoint);
    }
    return result;
  }

  if (options.scan.allowOverflow && overflowBudgetScope.tryEnable()) {
    if (const auto result = try_visit_pass(true);
        result != DeleteScanVisitResult::Continue) {
      if (result != DeleteScanVisitResult::Accept) {
        ctx.rewind(recoveryCheckpoint);
      }
      return result;
    }
  }

  ctx.rewind(recoveryCheckpoint);
  return DeleteScanVisitResult::Stop;
}

template <typename Context, typename CanDeleteStepFn, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] inline bool recover_by_guarded_delete_scan(
    Context &ctx, CanDeleteStepFn &&canDeleteStepFn, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn, DeleteScanOptions options = {}) {
  auto &&match = matchFn;
  auto &&onMatch = onMatchFn;
  return visit_guarded_delete_scan_positions(
             ctx, std::forward<CanDeleteStepFn>(canDeleteStepFn),
             [&](const DeleteScanVisitState &state) {
               if (const char *const matchedEnd =
                       invoke_delete_scan_match(match, ctx.cursor(),
                                                state.deleteCount);
                   matchedEnd != nullptr) {
                 invoke_delete_scan_on_match(onMatch, matchedEnd,
                                             state.deleteCount);
                 return DeleteScanVisitResult::Accept;
               }
               return DeleteScanVisitResult::Continue;
             },
             {.scan = options}) == DeleteScanVisitResult::Accept;
}

template <typename Context, typename ShouldRetryFn, typename RetryFn>
[[nodiscard]] inline DeleteRetryVisitResult
visit_guarded_delete_retry_positions(
    Context &ctx, ShouldRetryFn &&shouldRetryFn, RetryFn &&retryFn,
    DeleteRetryOptions options = {}) {
  if (!ctx.canDelete()) {
    return DeleteRetryVisitResult::Stop;
  }

  const auto recoveryCheckpoint = ctx.mark();
  DeleteRetryReplayScope retryScope{ctx, options.disableDeleteRetry};
  auto &&shouldRetry = shouldRetryFn;
  auto &&retry = retryFn;
  std::uint32_t deleteCount = 0u;
  const auto try_retry_at_current_position =
      [&](const DeleteRetryVisitState &state) {
        if (!invoke_delete_retry_predicate(shouldRetry, state)) {
          return DeleteRetryVisitResult::Continue;
        }
        const auto result = invoke_delete_scan_visit(retry, state);
        if (result == DeleteRetryVisitResult::Accept && state.overflowBudget) {
          retryScope.commitOverflowEdits();
        }
        return result;
  };
  const auto try_retry_pass = [&](bool overflowBudget) {
    while (deleteCount < options.scan.maxDeletes) {
      const bool structuredVisibleSource =
          position_starts_structured_visible_source(ctx,
                                                                ctx.cursor());
      if (((overflowBudget && options.stopOverflowAtStructuredVisibleSource) ||
           options.stopAtStructuredVisibleSource) &&
          structuredVisibleSource && deleteCount != 0u) {
        return DeleteRetryVisitResult::Continue;
      }
      if (!ctx.deleteOneCodepoint()) {
        break;
      }
      ++deleteCount;
      bool hiddenTriviaBoundary = false;
      if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
        hiddenTriviaBoundary =
            ctx.skip_without_builder(ctx.cursor()) > ctx.cursor();
      }
      const auto visitState = DeleteRetryVisitState{
          .overflowBudget = overflowBudget,
          .hiddenTriviaBoundary = hiddenTriviaBoundary,
          .hiddenTriviaExtended = false,
          .deleteCount = deleteCount,
      };
      if (const auto result = try_retry_at_current_position(visitState);
          result != DeleteRetryVisitResult::Continue) {
        return result;
      }
      if (options.stopAtStructuredVisibleSource &&
          position_starts_structured_visible_source(ctx,
                                                                ctx.cursor()) &&
          deleteCount != 0u) {
        return DeleteRetryVisitResult::Stop;
      }
      if (!hiddenTriviaBoundary) {
        continue;
      }
      if (options.extendThroughHiddenTrivia) {
        if constexpr (requires { ctx.extendLastDeleteThroughHiddenTrivia(); }) {
          if (ctx.extendLastDeleteThroughHiddenTrivia()) {
            if (!options.visitAfterHiddenTriviaExtension) {
              continue;
            }
            const auto extendedVisitState = DeleteRetryVisitState{
                .overflowBudget = overflowBudget,
                .hiddenTriviaBoundary = false,
                .hiddenTriviaExtended = true,
                .deleteCount = deleteCount,
            };
            if (const auto result =
                    try_retry_at_current_position(extendedVisitState);
                result != DeleteRetryVisitResult::Continue) {
              return result;
            }
            if (options.stopAtStructuredVisibleSource &&
                position_starts_structured_visible_source(
                    ctx, ctx.cursor()) &&
                deleteCount != 0u) {
              return DeleteRetryVisitResult::Stop;
            }
            continue;
          }
        }
      }
      if (options.stopAtHiddenTriviaBoundary) {
        return DeleteRetryVisitResult::Stop;
      }
    }
    return DeleteRetryVisitResult::Continue;
  };

  if (const auto result = try_retry_pass(false);
      result != DeleteRetryVisitResult::Continue) {
    if (result != DeleteRetryVisitResult::Accept) {
      ctx.rewind(recoveryCheckpoint);
    }
    return result;
  }

  if (options.scan.allowOverflow && retryScope.tryEnableExtendedDeleteScan()) {
    if (const auto result = try_retry_pass(true);
        result != DeleteRetryVisitResult::Continue) {
      if (result != DeleteRetryVisitResult::Accept) {
        ctx.rewind(recoveryCheckpoint);
      }
      return result;
    }
  }

  ctx.rewind(recoveryCheckpoint);
  return DeleteRetryVisitResult::Stop;
}

template <typename Context, typename ShouldRetryFn, typename RetryFn>
[[nodiscard]] inline bool recover_by_guarded_delete_retry(
    Context &ctx, ShouldRetryFn &&shouldRetryFn, RetryFn &&retryFn,
    DeleteRetryOptions options = {}) {
  const auto result = visit_guarded_delete_retry_positions(
      ctx, std::forward<ShouldRetryFn>(shouldRetryFn),
      std::forward<RetryFn>(retryFn), options);
  return result == DeleteRetryVisitResult::Accept;
}

} // namespace pegium::parser::detail
