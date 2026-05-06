#pragma once

/// Helpers for evaluating recovery candidates in editable parse modes.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>

namespace pegium::parser::detail {

/// True iff `kind` commits the candidate to a strictly different replay
/// prefix. Both `Deleted` and `Replaced` consume source text and shift
/// what a downstream replay must reproduce; an `Inserted` edit alone
/// does not. Recovery's family-redundancy and replay-prefix
/// classifications collapse to this single predicate.
[[nodiscard]] constexpr bool
is_destructive_edit_kind(ParseDiagnosticKind kind) noexcept {
  return kind == ParseDiagnosticKind::Deleted ||
         kind == ParseDiagnosticKind::Replaced;
}

/// Summary of the edit-script slice [`baseEditCount`, end()): the first
/// edit's begin offset, the maximum end offset, and whether any edit in
/// the slice is destructive (`Deleted` or `Replaced`). Both
/// `EditableRecoveryCandidate` and `StructuralProgressRecoveryCandidate`
/// constructors collapse the same loop into this projection.
struct EditSliceSummary {
  TextOffset firstEditBeginOffset = 0;
  TextOffset maxEndOffset = 0;
  bool hasDestructiveEdit = false;
  /// True iff the slice is non-empty and every edit in it is `Deleted`
  /// (no inserts, no replaces). Default `false` on an empty slice so
  /// callers that only consult this when the slice is non-empty get a
  /// safe default.
  bool allDeleted = false;
};

[[nodiscard]] inline EditSliceSummary
summarize_edits_since(std::span<const SyntaxScriptEntry> edits,
                      std::size_t baseEditCount) noexcept {
  EditSliceSummary summary{};
  if (baseEditCount >= edits.size()) {
    return summary;
  }
  summary.firstEditBeginOffset = edits[baseEditCount].beginOffset;
  summary.allDeleted = true;
  for (std::size_t i = baseEditCount; i < edits.size(); ++i) {
    summary.hasDestructiveEdit =
        summary.hasDestructiveEdit || is_destructive_edit_kind(edits[i].kind);
    summary.maxEndOffset = std::max(summary.maxEndOffset, edits[i].endOffset);
    if (edits[i].kind != ParseDiagnosticKind::Deleted) {
      summary.allDeleted = false;
    }
  }
  return summary;
}

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
      // Both `Deleted` and `Replaced` commit to a strictly different
      // replay prefix than an insert-only candidate, so both belong to
      // the destructive equivalence class consumed by the family-
      // redundancy filters. Treating Replace as non-destructive here
      // misclassifies fuzzy keyword recoveries (e.g. `entit -> entity`)
      // as `NewLocalPrefix`, which silently drops the
      // `extension_outranks_anchor_base` ratio guard and lets a sibling
      // insert-only path with the same cost win on enumeration order.
      const auto editSummary = summarize_edits_since(ctx.recoveryEditsView(),
                                                     baseRecoveryEditCount);
      candidate.firstEditOffset = editSummary.firstEditBeginOffset;
      candidate.hasDestructiveEdit = editSummary.hasDestructiveEdit;
      candidate.editSpan =
          editSummary.maxEndOffset > editSummary.firstEditBeginOffset
              ? editSummary.maxEndOffset - editSummary.firstEditBeginOffset
              : 0;
    }
    candidate.reachedEof =
        candidate.postSkipCursorOffset >=
        static_cast<TextOffset>(ctx.end - ctx.begin);
    candidate.replayPrefix = classify_editable_replay_prefix(
        candidate.editCount, candidate.hasDestructiveEdit);
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
