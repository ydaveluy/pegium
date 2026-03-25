#pragma once

/// Helpers for evaluating recovery candidates in editable parse modes.

#include <cstdint>

#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>

namespace pegium::parser::detail {

template <typename Checkpoint, typename Runner>
[[nodiscard]] inline EditableRecoveryCandidate
evaluate_editable_recovery_candidate(RecoveryContext &ctx,
                                     const Checkpoint &entryCheckpoint,
                                     std::uint32_t baseEditCost,
                                     std::uint32_t baseEditCount,
                                     std::size_t baseRecoveryEditCount,
                                     Runner &&runner) {
  EditableRecoveryCandidate candidate;
  if (std::forward<Runner>(runner)()) {
    candidate.matched = true;
    candidate.cursorOffset = ctx.cursorOffset();
    const auto postMatchCheckpoint = ctx.mark();
    ctx.skip();
    candidate.postSkipCursorOffset = ctx.cursorOffset();
    ctx.rewind(postMatchCheckpoint);
    candidate.editCost = ctx.currentEditCost() - baseEditCost;
    candidate.editCount = ctx.currentEditCount() - baseEditCount;
    if (ctx.recoveryEditCount() > baseRecoveryEditCount) {
      const auto edits = ctx.snapshotRecoveryEdits();
      candidate.firstEditOffset = edits[baseRecoveryEditCount].offset;
      candidate.firstEditElement = edits[baseRecoveryEditCount].element;
    }
  }
  ctx.rewind(entryCheckpoint);
  return candidate;
}

[[nodiscard]] inline RecoveryProbeProgress
capture_recovery_probe_progress(const RecoveryContext &ctx) {
  return {
      .committedOffset = ctx.cursorOffset(),
      .exploredOffset = ctx.maxCursorOffset(),
  };
}

template <typename Checkpoint>
[[nodiscard]] inline ProgressRecoveryCandidate
capture_progress_recovery_candidate(const RecoveryContext &ctx,
                                    const Checkpoint &entryCheckpoint) {
  return {
      .matched = true,
      .cursorOffset = ctx.cursorOffset(),
      .editCost = ctx.editCostDelta(entryCheckpoint),
  };
}

[[nodiscard]] constexpr bool is_better_efficiency_weighted_editable_candidate(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs,
    TextOffset parseStartOffset) noexcept {
  if (lhs.matched != rhs.matched) {
    return lhs.matched && !rhs.matched;
  }
  if (!lhs.matched) {
    return false;
  }
  if (lhs.firstEditOffset == rhs.firstEditOffset) {
    if (lhs.editCost != rhs.editCost) {
      return lhs.editCost < rhs.editCost;
    }
    if (lhs.editCount != rhs.editCount) {
      return lhs.editCount < rhs.editCount;
    }
  }
  const auto lhsGain = lhs.cursorOffset - parseStartOffset;
  const auto rhsGain = rhs.cursorOffset - parseStartOffset;
  const std::uint64_t lhsEfficiency =
      static_cast<std::uint64_t>(lhsGain) *
      static_cast<std::uint64_t>(rhs.editCost);
  if (const std::uint64_t rhsEfficiency =
          static_cast<std::uint64_t>(rhsGain) *
          static_cast<std::uint64_t>(lhs.editCost);
      lhsEfficiency > rhsEfficiency) {
    return true;
  }
  return is_better_editable_recovery_candidate(lhs, rhs);
}

} // namespace pegium::parser::detail
