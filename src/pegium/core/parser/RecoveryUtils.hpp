#pragma once

/// Shared delete-scan recovery utilities.

#include <limits>
#include <utility>

#include <pegium/core/parser/ParseContext.hpp>

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
  ExtendedDeleteScanBudgetScope<Context> overflowBudgetScope;

  explicit DeleteRetryReplayScope(Context &ctx) noexcept
      : ctx(ctx), savedAllowDeleteRetry(ctx.allowDeleteRetry),
        savedSkipAfterDelete(ctx.skipAfterDelete), overflowBudgetScope(ctx) {
    ctx.allowDeleteRetry = false;
    ctx.skipAfterDelete = false;
  }

  DeleteRetryReplayScope(const DeleteRetryReplayScope &) = delete;
  DeleteRetryReplayScope &
  operator=(const DeleteRetryReplayScope &) = delete;

  ~DeleteRetryReplayScope() noexcept {
    ctx.skipAfterDelete = savedSkipAfterDelete;
    ctx.allowDeleteRetry = savedAllowDeleteRetry;
  }

  [[nodiscard]] bool tryEnableExtendedDeleteScan() noexcept {
    return overflowBudgetScope.tryEnable();
  }

  void restoreExtendedDeleteScan() noexcept { overflowBudgetScope.restore(); }

  void commitOverflowEdits() noexcept { overflowBudgetScope.commitOverflowEdits(); }
};

template <typename Context, typename MatchFn, typename OnMatchFn>
[[nodiscard]] inline bool recover_by_delete_scan(Context &ctx,
                                                 MatchFn &&matchFn,
                                                 OnMatchFn &&onMatchFn) {
  if (!ctx.canDelete()) {
    return false;
  }

  const auto recoveryCheckpoint = ctx.mark();
  while (ctx.deleteOneCodepoint()) {
    const char *const scanCursor = ctx.cursor();
    if (const char *const matchedEnd =
            std::forward<MatchFn>(matchFn)(scanCursor);
        matchedEnd != nullptr) {
      std::forward<OnMatchFn>(onMatchFn)(matchedEnd);
      return true;
    }
  }
  ExtendedDeleteScanBudgetScope overflowBudgetScope{ctx};
  if (overflowBudgetScope.tryEnable()) {
    while (ctx.deleteOneCodepoint()) {
      const char *const scanCursor = ctx.cursor();
      if (const char *const matchedEnd =
              std::forward<MatchFn>(matchFn)(scanCursor);
          matchedEnd != nullptr) {
        overflowBudgetScope.commitOverflowEdits();
        std::forward<OnMatchFn>(onMatchFn)(matchedEnd);
        return true;
      }
    }
  }

  ctx.rewind(recoveryCheckpoint);
  return false;
}

template <typename Context, typename RetryFn>
[[nodiscard]] inline bool recover_by_delete_retry(Context &ctx,
                                                  RetryFn &&retryFn) {
  if (!ctx.canDelete()) {
    return false;
  }

  const auto recoveryCheckpoint = ctx.mark();
  const bool previousSkipAfterDelete = ctx.skipAfterDelete;
  ctx.skipAfterDelete = false;
  while (ctx.deleteOneCodepoint()) {
    if (std::forward<RetryFn>(retryFn)()) {
      ctx.skipAfterDelete = previousSkipAfterDelete;
      return true;
    }
    if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
      if (ctx.skip_without_builder(ctx.cursor()) > ctx.cursor()) {
        if constexpr (requires { ctx.extendLastDeleteThroughHiddenTrivia(); }) {
          if (ctx.extendLastDeleteThroughHiddenTrivia()) {
            if (std::forward<RetryFn>(retryFn)()) {
              ctx.skipAfterDelete = previousSkipAfterDelete;
              return true;
            }
            continue;
          }
        }
        break;
      }
    }
  }
  ctx.skipAfterDelete = previousSkipAfterDelete;

  ctx.rewind(recoveryCheckpoint);
  return false;
}

} // namespace pegium::parser::detail
