#pragma once

/// Helpers for evaluating recovery candidates in editable parse modes.

#include <algorithm>
#include <cstdint>

#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>

namespace pegium::parser::detail {

[[nodiscard]] inline TextOffset
post_skip_cursor_offset(RecoveryContext &ctx) {
  if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
    const auto skipped = ctx.skip_without_builder(ctx.cursor());
    return static_cast<TextOffset>(skipped - ctx.begin);
  }
  const auto postMatchCheckpoint = ctx.mark();
  ctx.skip();
  const auto offset = ctx.cursorOffset();
  ctx.rewind(postMatchCheckpoint);
  return offset;
}

template <typename Checkpoint, typename Runner>
[[nodiscard]] inline EditableRecoveryCandidate
evaluate_editable_recovery_candidate(RecoveryContext &ctx,
                                     const Checkpoint &entryCheckpoint,
                                     std::uint32_t baseEditCost,
                                     std::size_t baseRecoveryEditCount,
                                     Runner &&runner) {
  EditableRecoveryCandidate candidate;
  const char *const savedFurthestExploredCursor =
      ctx.furthestExploredCursor();
  if (std::forward<Runner>(runner)()) {
    candidate.matched = true;
    candidate.cursorOffset = ctx.cursorOffset();
    candidate.postSkipCursorOffset = post_skip_cursor_offset(ctx);
    candidate.editCost = ctx.currentEditCost() - baseEditCost;
    candidate.editCount =
        static_cast<std::uint32_t>(ctx.recoveryEditCount() -
                                   baseRecoveryEditCount);
    if (ctx.recoveryEditCount() > baseRecoveryEditCount) {
      const auto edits = ctx.recoveryEditsView();
      const auto &firstEdit = edits[baseRecoveryEditCount];
      candidate.firstEditOffset = firstEdit.beginOffset;
      candidate.firstEditElement = firstEdit.element;
      candidate.hasDeleteEdit =
          firstEdit.kind == ParseDiagnosticKind::Deleted;
      TextOffset maxEndOffset = firstEdit.endOffset;
      for (std::size_t i = baseRecoveryEditCount + 1u; i < edits.size(); ++i) {
        candidate.hasDeleteEdit =
            candidate.hasDeleteEdit ||
            edits[i].kind == ParseDiagnosticKind::Deleted;
        maxEndOffset = std::max(maxEndOffset, edits[i].endOffset);
      }
      candidate.editSpan = maxEndOffset > firstEdit.beginOffset
                               ? maxEndOffset - firstEdit.beginOffset
                               : 0;
    }
  }
  ctx.rewind(entryCheckpoint);
  ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
  return candidate;
}

[[nodiscard]] inline RecoveryProbeProgress
capture_recovery_probe_progress(const RecoveryContext &ctx,
                                std::size_t visibleLeafCountBefore) {
  const auto furthestVisibleLeafCount = ctx.furthestFailureHistorySize();
  return {
      .committedOffset = ctx.cursorOffset(),
      .furthestExploredOffset = ctx.furthestExploredOffset(),
      .exploredVisibleLeafCount =
          furthestVisibleLeafCount > visibleLeafCountBefore
              ? furthestVisibleLeafCount - visibleLeafCountBefore
              : 0u,
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

} // namespace pegium::parser::detail
