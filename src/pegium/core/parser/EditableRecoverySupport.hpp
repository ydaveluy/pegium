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
      // Both `Deleted` and `Replaced` commit to a strictly different
      // replay prefix than an insert-only candidate, so both belong to
      // the destructive equivalence class consumed by the family-
      // redundancy filters. Treating Replace as non-destructive here
      // misclassifies fuzzy keyword recoveries (e.g. `entit -> entity`)
      // as `NewLocalPrefix`, which silently drops the
      // `extension_outranks_anchor_base` ratio guard and lets a sibling
      // insert-only path with the same cost win on enumeration order.
      candidate.hasDeleteEdit =
          firstEdit.kind == ParseDiagnosticKind::Deleted ||
          firstEdit.kind == ParseDiagnosticKind::Replaced;
      TextOffset maxEndOffset = firstEdit.endOffset;
      for (std::size_t i = baseRecoveryEditCount + 1u; i < edits.size(); ++i) {
        candidate.hasDeleteEdit =
            candidate.hasDeleteEdit ||
            edits[i].kind == ParseDiagnosticKind::Deleted ||
            edits[i].kind == ParseDiagnosticKind::Replaced;
        maxEndOffset = std::max(maxEndOffset, edits[i].endOffset);
      }
      candidate.editSpan = maxEndOffset > firstEdit.beginOffset
                               ? maxEndOffset - firstEdit.beginOffset
                               : 0;
    }
    candidate.reachedEof =
        candidate.postSkipCursorOffset >=
        static_cast<TextOffset>(ctx.end - ctx.begin);
    candidate.replayPrefix = classify_editable_replay_prefix(
        candidate.editCount, candidate.hasDeleteEdit);
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

} // namespace pegium::parser::detail
