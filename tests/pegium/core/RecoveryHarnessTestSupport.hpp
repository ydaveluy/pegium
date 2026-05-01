#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

namespace pegium::test {

inline converter::CstJsonConversionOptions recovery_cst_json_options() {
  return {
      .includeText = true,
      .includeGrammarSource = true,
      .includeHidden = false,
      .includeRecovered = true,
  };
}

template <typename Result> struct RecoveryHarnessResult {
  Result result;
};

template <typename RuleType>
auto StrictRecoveryDocument(const RuleType &entryRule, std::string_view text,
                            const parser::Skipper &skipper,
                            const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessResult<parser::detail::StrictParseResult> harness;
  const auto snapshot = text::TextSnapshot::copy(text);
  const parser::detail::StrictFailureEngine strictFailureEngine;
  harness.result = strictFailureEngine.runStrictParse(entryRule, skipper,
                                                      snapshot, cancelToken);
  return harness;
}

template <typename RuleType>
auto FailureAnalysisDocument(const RuleType &entryRule, std::string_view text,
                             const parser::Skipper &skipper,
                             const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessResult<parser::detail::StrictFailureEngineResult>
      failureHarness;
  const auto snapshot = text::TextSnapshot::copy(text);
  failureHarness.result = parser::detail::run_strict_parse_with_failure_snapshot(
      entryRule, skipper, snapshot, cancelToken);
  return failureHarness;
}

/// Snapshot-diff observe/rewind harness.
///
/// Every observation that touches the recovery context must capture a
/// snapshot of the relevant state, run the observation under
/// checkpoint/rewind, and verify that the snapshot is identical after
/// rewind. A non-empty diff is a test error, not a warning.

struct RecoveryContextSnapshot {
  std::uint64_t hash = 0;
  std::string summary;

  [[nodiscard]] friend bool
  operator==(const RecoveryContextSnapshot &a,
             const RecoveryContextSnapshot &b) noexcept {
    return a.hash == b.hash;
  }
  [[nodiscard]] friend bool
  operator!=(const RecoveryContextSnapshot &a,
             const RecoveryContextSnapshot &b) noexcept {
    return !(a == b);
  }
};

namespace detail {

inline std::uint64_t snapshot_mix(std::uint64_t h, std::uint64_t value) noexcept {
  // FNV-1a-flavoured mixing; deterministic and order-sensitive.
  h ^= value;
  h *= 0x100000001B3ULL;
  return h;
}

inline std::uint64_t snapshot_mix_ptr(std::uint64_t h, const void *ptr) noexcept {
  return snapshot_mix(
      h, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ptr)));
}

inline std::uint64_t snapshot_mix_bool(std::uint64_t h, bool value) noexcept {
  return snapshot_mix(h, value ? 1ULL : 0ULL);
}

} // namespace detail

[[nodiscard]] inline RecoveryContextSnapshot
capture_recovery_context_snapshot(const parser::RecoveryContext &ctx) {
  RecoveryContextSnapshot snapshot;
  std::uint64_t h = 0xCBF29CE484222325ULL; // FNV offset basis.

  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.cursorOffset()));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.maxCursorOffset()));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.furthestFailureOffset()));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.failureHistorySize()));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.furthestFailureHistorySize()));

  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.currentEditCost()));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.currentEditCount()));
  h = detail::snapshot_mix(
      h,
      static_cast<std::uint64_t>(ctx.recoveryState.editBudget.consecutiveDeletes));
  h = detail::snapshot_mix_bool(h, ctx.recoveryState.editBudget.hadEdits);
  h = detail::snapshot_mix_bool(h,
                                 ctx.recoveryState.editBudget.allowBudgetOverflowEdits);

  const auto &wr = ctx.recoveryState.windowReplay;
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(wr.activeWindowEditCostBase));
  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(wr.activeWindowEditCountBase));
  h = detail::snapshot_mix(h,
                            static_cast<std::uint64_t>(wr.currentForwardVisibleLeafCount));
  h = detail::snapshot_mix(
      h, static_cast<std::uint64_t>(wr.strictVisibleLeafCountAfterRecovery));
  h = detail::snapshot_mix(h,
                            static_cast<std::uint64_t>(wr.completedRecoveryWindows));
  h = detail::snapshot_mix_bool(h, wr.inRecoveryPhase);
  h = detail::snapshot_mix_bool(h, wr.reachedRecoveryTarget);
  h = detail::snapshot_mix_bool(h, wr.stableAfterRecovery);
  h = detail::snapshot_mix_bool(h, wr.awaitingStrictStability);
  h = detail::snapshot_mix_bool(h, wr.recoveryBookkeepingEnabled);
  h = detail::snapshot_mix_bool(h, wr.activeEditWindowCompleted);

  h = detail::snapshot_mix_bool(h, ctx.recoveryState.frontierBlocked);

  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.recoveryEdits.size()));
  h = detail::snapshot_mix(h,
                            static_cast<std::uint64_t>(ctx.committedRecoveryEditIndex));
  h = detail::snapshot_mix(
      h, static_cast<std::uint64_t>(ctx.committedRecoveryResumeFloor));

  h = detail::snapshot_mix_ptr(h, ctx._deleteBridge.pendingHiddenTriviaStart);
  h = detail::snapshot_mix_ptr(h, ctx._deleteBridge.pendingHiddenTriviaEnd);

  h = detail::snapshot_mix_bool(h, ctx.editWindow.has_value());
  if (ctx.editWindow.has_value()) {
    h = detail::snapshot_mix(h,
                              static_cast<std::uint64_t>(ctx.editWindow->beginOffset));
    h = detail::snapshot_mix(
        h, static_cast<std::uint64_t>(ctx.editWindow->editFloorOffset));
    h = detail::snapshot_mix(
        h, static_cast<std::uint64_t>(ctx.editWindow->maxCursorOffset));
    h = detail::snapshot_mix(
        h, static_cast<std::uint64_t>(ctx.editWindow->forwardTokenCount));
    h = detail::snapshot_mix(
        h, static_cast<std::uint64_t>(ctx.editWindow->replayForwardTokenCount));
  }

  h = detail::snapshot_mix(h, static_cast<std::uint64_t>(ctx.editFloorOffset));

  h = detail::snapshot_mix_ptr(h,
                                reinterpret_cast<const void *>(ctx._followProbeFn));
  h = detail::snapshot_mix_ptr(h, ctx._followProbeData);
  h = detail::snapshot_mix_ptr(
      h, reinterpret_cast<const void *>(ctx._recoverableFollowProbeFn));
  h = detail::snapshot_mix_ptr(h, ctx._recoverableFollowProbeData);

  snapshot.hash = h;
  snapshot.summary = "cursor=" + std::to_string(ctx.cursorOffset()) +
                      " maxCursor=" + std::to_string(ctx.maxCursorOffset()) +
                      " edits=" + std::to_string(ctx.recoveryEdits.size()) +
                      " editCost=" + std::to_string(ctx.currentEditCost()) +
                      " editCount=" + std::to_string(ctx.currentEditCount()) +
                      " inRecovery=" +
                      (ctx.isInRecoveryPhase() ? "1" : "0");
  return snapshot;
}

template <typename Op>
void expect_observe_rewind_neutral(parser::RecoveryContext &ctx, Op operation,
                                   std::string_view caseName) {
  const auto before = capture_recovery_context_snapshot(ctx);
  const auto checkpoint = ctx.mark();
  (void)operation(ctx);
  ctx.rewind(checkpoint);
  const auto after = capture_recovery_context_snapshot(ctx);

  SCOPED_TRACE(testing::Message() << "case=" << caseName);
  EXPECT_EQ(before, after)
      << "observe/rewind not neutral\n  before: " << before.summary
      << "\n  after : " << after.summary;
}

} // namespace pegium::test
