#pragma once

/// Shared delete-scan recovery utilities.

#include <utility>

#include <pegium/core/parser/ParseContext.hpp>

namespace pegium::parser::detail {

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
  while (ctx.deleteOneCodepoint()) {
    if (std::forward<RetryFn>(retryFn)()) {
      return true;
    }
  }

  ctx.rewind(recoveryCheckpoint);
  return false;
}

} // namespace pegium::parser::detail
