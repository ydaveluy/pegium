#pragma once

#include <cstdint>

#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryCandidate.hpp>

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
    candidate.editCost = ctx.currentEditCost() - baseEditCost;
    candidate.editCount = ctx.currentEditCount() - baseEditCount;
    if (ctx.recoveryEditCount() > baseRecoveryEditCount) {
      const auto edits = ctx.snapshotRecoveryEdits();
      candidate.firstEditOffset = edits[baseRecoveryEditCount].offset;
    }
  }
  ctx.rewind(entryCheckpoint);
  return candidate;
}

[[nodiscard]] constexpr bool prefer_efficiency_weighted_delete_retry_candidate(
    const EditableRecoveryCandidate &deleteRetry,
    const EditableRecoveryCandidate &editable,
    TextOffset parseStartOffset) noexcept {
  if (!deleteRetry.matched) {
    return false;
  }
  if (!editable.matched) {
    return true;
  }
  const auto deleteGain = deleteRetry.cursorOffset - parseStartOffset;
  const auto editableGain = editable.cursorOffset - parseStartOffset;
  const std::uint64_t deleteEfficiency =
      static_cast<std::uint64_t>(deleteGain) *
      static_cast<std::uint64_t>(editable.editCost);
  const std::uint64_t editableEfficiency =
      static_cast<std::uint64_t>(editableGain) *
      static_cast<std::uint64_t>(deleteRetry.editCost);
  if (deleteEfficiency > editableEfficiency) {
    return true;
  }
  return is_better_editable_recovery_candidate(deleteRetry, editable);
}

} // namespace pegium::parser::detail
