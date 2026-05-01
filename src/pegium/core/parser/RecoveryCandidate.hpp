#pragma once

/// Candidate structures and the unified 4-axis recovery ranking key.
///
/// Phase C collapsed the former 24-axis / 3-profile comparator into a single
/// lexicographic key with four axes:
///   1. `matched`            (higher wins)
///   2. `firstEditOffset`    (higher wins — later edits beat earlier edits)
///   3. `editCost`           (lower wins)
///   4. `progressAfterEdits` (higher wins)

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pegium/core/parser/RecoveryContract.hpp>
#include <pegium/core/parser/RecoveryCost.hpp>
#include <pegium/core/syntax-tree/CstNode.hpp>

namespace pegium::parser::detail {

enum class TerminalRecoveryChoiceKind : std::uint8_t {
  None,
  Insert,
  Replace,
  DeleteScan,
};

struct TerminalRecoveryCandidate {
  TerminalRecoveryChoiceKind kind = TerminalRecoveryChoiceKind::None;
  RecoveryCost cost{};
  std::uint32_t distance = std::numeric_limits<std::uint32_t>::max();
  std::size_t consumed = 0;
  std::uint32_t substitutionCount = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t operationCount = std::numeric_limits<std::uint32_t>::max();
};

struct EditableRecoveryCandidate {
  bool matched = false;
  bool hasDeleteEdit = false;
  TextOffset cursorOffset = 0;
  // Cursor after one skipper pass from the matched position. This lets choice
  // recovery ignore progress that only comes from trailing trivia.
  TextOffset postSkipCursorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  TextOffset editSpan = 0;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  bool reachedEof = false;
  /// Closed classification of the candidate's replay prefix relative
  /// to the surrounding scope's committed prefix. Set at construction
  /// based on the edit shape (`classify_editable_replay_prefix`);
  /// consumed by dominance predicates that require both candidates
  /// to belong to the same replay-prefix equivalence class. Replaces
  /// the legacy ad-hoc `editCount/hasDeleteEdit` checks scattered
  /// across dominance predicates with a single closed enum read.
  ReplayPrefixClass replayPrefix = ReplayPrefixClass::Empty;
};

/// Classifies a candidate's replay prefix from its observed edit
/// shape. Closed mapping:
///
///   - 0 edits                  -> `Empty`
///   - edits with at least one
///     delete                   -> `ExtendedCommittedPrefix`
///   - insert-only edits        -> `NewLocalPrefix`
///
/// `SameCommittedPrefix` is reserved for sites that compare the
/// candidate's replay against an already-committed prefix snapshot
/// (e.g. the `RecoverySearch` driver between attempts); the
/// `OrderedChoice` / `Group` / `Repetition` per-attempt classifier
/// does not have a committed snapshot at hand and keeps to the
/// three values above.
[[nodiscard]] constexpr ReplayPrefixClass
classify_editable_replay_prefix(std::uint32_t editCount,
                                bool hasDeleteEdit) noexcept {
  if (editCount == 0u) {
    return ReplayPrefixClass::Empty;
  }
  if (hasDeleteEdit) {
    return ReplayPrefixClass::ExtendedCommittedPrefix;
  }
  return ReplayPrefixClass::NewLocalPrefix;
}

/// Classifies a structural-progress candidate's replay prefix. Same
/// closed mapping as `classify_editable_replay_prefix`, but the
/// "extension" signal is `hasDestructiveEdit` (Deleted OR Replaced)
/// — the term `Repetition` uses for the predicate that powers
/// `ExtendedCommittedPrefix` dominance. Both kinds of destructive
/// edit commit to a strictly different replay prefix and therefore
/// belong to the same equivalence class.
[[nodiscard]] constexpr ReplayPrefixClass
classify_structural_replay_prefix(bool hadEdits,
                                  bool hasDestructiveEdit) noexcept {
  if (!hadEdits) {
    return ReplayPrefixClass::Empty;
  }
  if (hasDestructiveEdit) {
    return ReplayPrefixClass::ExtendedCommittedPrefix;
  }
  return ReplayPrefixClass::NewLocalPrefix;
}

struct StructuralProgressRecoveryCandidate {
  bool matched = false;
  TextOffset cursorOffset = 0;
  std::uint32_t editCost = std::numeric_limits<std::uint32_t>::max();
  bool hadEdits = false;
  /// True iff the candidate's edits include at least one Deleted or
  /// Replaced edit. "Destructive" is the term `Repetition` uses for
  /// the predicate that powers `ExtendedCommittedPrefix` dominance:
  /// either kind of edit commits to a strictly different replay
  /// prefix, so both feed the same equivalence class.
  bool hasDestructiveEdit = false;
  bool continuesAfterFirstEdit = true;
  bool rewritesParseStartBoundary = false;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  /// Closed classification of the candidate's replay prefix. Set at
  /// construction (`classify_structural_replay_prefix`); consumed by
  /// `Repetition`'s dominance predicates that require both candidates
  /// to belong to the same replay-prefix equivalence class. Replaces
  /// the legacy ad-hoc `hadEdits / hasDestructiveEdits` checks
  /// scattered across those predicates with a single closed enum
  /// read, mirroring the `EditableRecoveryCandidate` path.
  ReplayPrefixClass replayPrefix = ReplayPrefixClass::Empty;
};

struct RecoveryProbeProgress {
  TextOffset committedOffset = 0;
  TextOffset furthestExploredOffset = 0;
  std::size_t exploredVisibleLeafCount = 0;

  [[nodiscard]] constexpr bool
  committedProgressed(TextOffset startOffset) const noexcept {
    return committedOffset > startOffset;
  }

  [[nodiscard]] constexpr bool
  exploredBeyond(TextOffset offset) const noexcept {
    return furthestExploredOffset > offset;
  }

  [[nodiscard]] constexpr bool deferred() const noexcept {
    return furthestExploredOffset > committedOffset;
  }

  [[nodiscard]] constexpr bool
  exploredSingleVisibleLeafOrLess() const noexcept {
    return exploredVisibleLeafCount <= 1u;
  }
};

enum class ChoiceRecoveryEntrySignalRequirement : std::uint8_t {
  Reject,
  Accept,
  ProbeEntryStart,
};

[[nodiscard]] constexpr bool continues_after_first_edit(
    const EditableRecoveryCandidate &candidate) noexcept {
  return candidate.firstEditOffset != std::numeric_limits<TextOffset>::max() &&
         candidate.postSkipCursorOffset >
             candidate.firstEditOffset + candidate.editSpan;
}

[[nodiscard]] constexpr ChoiceRecoveryEntrySignalRequirement
classify_choice_recovery_entry_signal_requirement(
    const EditableRecoveryCandidate &candidate, TextOffset parseStartOffset,
    bool hasStrictStartSignal, bool branchHasStrictStartSignal) noexcept {
  if (!candidate.matched) {
    return ChoiceRecoveryEntrySignalRequirement::Reject;
  }
  if (hasStrictStartSignal) {
    return branchHasStrictStartSignal
               ? ChoiceRecoveryEntrySignalRequirement::Accept
               : ChoiceRecoveryEntrySignalRequirement::ProbeEntryStart;
  }

  const bool noEditCandidate = candidate.editCount == 0u;
  const bool continuesAfterFirstEdit = continues_after_first_edit(candidate);
  const bool sameStartContinuingInsert =
      candidate.firstEditOffset == parseStartOffset &&
      !candidate.hasDeleteEdit && continuesAfterFirstEdit;
  const bool autoLegalWithoutEntryStart =
      noEditCandidate || candidate.firstEditOffset > parseStartOffset ||
      sameStartContinuingInsert;
  if (autoLegalWithoutEntryStart) {
    return ChoiceRecoveryEntrySignalRequirement::Accept;
  }
  if (candidate.hasDeleteEdit && !continuesAfterFirstEdit) {
    return candidate.reachedEof
               ? ChoiceRecoveryEntrySignalRequirement::Accept
               : ChoiceRecoveryEntrySignalRequirement::Reject;
  }
  // A replace-style candidate at parseStartOffset that goes on to consume the
  // entire remaining input is admissible without a per-token entry probe.
  // Without this, a "far typo" of an alternative-leading keyword (e.g. `thing
  // Blog {}` where the user meant `entity Blog {}`) is rejected because no
  // surface-level character matches `entity`, even though replacing the typo
  // produces a single Entity that consumes everything cleanly. The rejected
  // branch then loses to a sequence of cheap inserts that fragment the same
  // input across two AbstractElements.
  if (candidate.reachedEof && candidate.firstEditOffset == parseStartOffset) {
    return ChoiceRecoveryEntrySignalRequirement::Accept;
  }
  return ChoiceRecoveryEntrySignalRequirement::ProbeEntryStart;
}

struct RecoveryKey {
  /// Highest-priority axis: a fullMatch attempt always beats a non-fullMatch
  /// one regardless of edit cost or offset. Local candidate constructors
  /// (`terminal_recovery_key`, `editable_recovery_key`,
  /// `structural_progress_recovery_key`) leave this `false` — only
  /// attempt-level callers (`recovery_attempt_key`) know whether a global
  /// fullMatch is in scope.
  bool fullMatch = false;
  bool matched = false;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  std::uint32_t editCost = 0u;
  TextOffset progressAfterEdits = 0;
};

[[nodiscard]] constexpr TextOffset
recovery_key_first_edit_score(const RecoveryKey &key) noexcept {
  // Compare on firstEditOffset penalized only when the edit is significantly
  // more expensive than a single cheap edit. Kept as a named helper so pruning
  // sites can prove a candidate cannot beat another one without duplicating
  // the first ranking axis.
  constexpr std::uint32_t kCostThreshold = 4u;
  const auto penalty = key.editCost > kCostThreshold
                           ? (key.editCost - kCostThreshold) / 4u
                           : 0u;
  return key.firstEditOffset > penalty ? key.firstEditOffset - penalty : 0u;
}

[[nodiscard]] constexpr bool
is_better_recovery_key(const RecoveryKey &lhs,
                       const RecoveryKey &rhs) noexcept {
  if (lhs.fullMatch != rhs.fullMatch) {
    return lhs.fullMatch;
  }
  if (lhs.matched != rhs.matched) {
    return lhs.matched;
  }
  // Compare on firstEditOffset penalized only when the edit is significantly
  // more expensive than a single cheap edit. A multi-byte delete that only
  // wins on a 2-3 byte offset advantage should not beat a short insert. But
  // a single-char delete (cost 4) should keep its baseline ranking.
  //
  // Threshold = cost of a single delete (4). Anything cheaper pays no
  // penalty. Above that, each extra cost unit divides by 4 ~ 1 byte of
  // offset-advantage lost.
  const auto lhsScore = recovery_key_first_edit_score(lhs);
  const auto rhsScore = recovery_key_first_edit_score(rhs);
  if (lhsScore != rhsScore) {
    return lhsScore > rhsScore;
  }
  // After firstEditOffset is tied, before falling back to pure edit cost,
  // promote a candidate that progresses much further for only a slight
  // edit-cost increase. Without this nudge, a "far typo" like `thing Blog
  // {}` recovers as `datatype thing` + `package Blog {}` (two cheap
  // inserts, two confusing diagnostics) instead of a single Entity (one
  // edit, one diagnostic). The thresholds are intentionally narrow:
  //
  //   - cost gap must be small (`<= kBonusCostBudget`), to keep the
  //     existing "lower cost wins at same offset" rule for genuine
  //     cost-vs-progress trade-offs;
  //   - progress gap must outpace the cost gap by a clear margin
  //     (`>= kBonusProgressMargin`), so a candidate is only promoted
  //     when the extra spend buys substantially more input.
  //
  // These constants leave the cost-dominated decisions covered by the
  // unit tests (e.g. `RecoveryKeyPrefersLowerEditCostAtSameOffset`)
  // unchanged while letting "single-edit far typos" win.
  constexpr std::uint32_t kBonusCostBudget = 2u;
  constexpr TextOffset kBonusProgressMargin = 5;
  if (lhs.matched && rhs.matched && lhs.editCost != rhs.editCost) {
    const auto cheaper = lhs.editCost < rhs.editCost ? lhs : rhs;
    const auto pricier = lhs.editCost < rhs.editCost ? rhs : lhs;
    const auto costGap = pricier.editCost - cheaper.editCost;
    if (costGap <= kBonusCostBudget &&
        pricier.progressAfterEdits >
            cheaper.progressAfterEdits + kBonusProgressMargin) {
      const auto progressGap =
          pricier.progressAfterEdits - cheaper.progressAfterEdits;
      if (progressGap > costGap + kBonusProgressMargin) {
        return &pricier == &lhs;
      }
    }
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  return lhs.progressAfterEdits > rhs.progressAfterEdits;
}

[[nodiscard]] constexpr RecoveryKey
terminal_recovery_key(const TerminalRecoveryCandidate &candidate) noexcept {
  // Terminal candidates use primaryRankCost (logical edit distance + strategy
  // penalty) as the cost axis. budgetCost measures the raw span deleted/
  // inserted, which inflates for fuzzy keyword repair and would lose to a
  // cheap-looking synthetic-gap split even when the fuzzy repair is the better
  // single-token edit.
  return {
      .matched = candidate.kind != TerminalRecoveryChoiceKind::None,
      .editCost = candidate.cost.primaryRankCost,
      .progressAfterEdits = static_cast<TextOffset>(candidate.consumed),
  };
}

[[nodiscard]] constexpr RecoveryKey
editable_recovery_key(const EditableRecoveryCandidate &candidate) noexcept {
  // Treat "no edit" as an edit at the candidate's current progress. Otherwise
  // a candidate with firstEditOffset = UINT_MAX would always beat an
  // insert-and-continue candidate on axis 2, even when the latter made much
  // more real progress.
  const TextOffset effectiveEditOffset =
      candidate.editCount == 0u
          ? candidate.postSkipCursorOffset
          : candidate.firstEditOffset;
  return {
      .matched = candidate.matched,
      .firstEditOffset = effectiveEditOffset,
      .editCost = candidate.editCost,
      .progressAfterEdits = candidate.postSkipCursorOffset,
  };
}

[[nodiscard]] constexpr RecoveryKey
structural_progress_recovery_key(
    const StructuralProgressRecoveryCandidate &candidate) noexcept {
  const TextOffset effectiveEditOffset =
      candidate.hadEdits ? candidate.firstEditOffset : candidate.cursorOffset;
  return {
      .matched = candidate.matched,
      .firstEditOffset = effectiveEditOffset,
      .editCost = candidate.editCost,
      .progressAfterEdits = candidate.cursorOffset,
  };
}

// All dispatches project onto `RecoveryKey` via the type-specific
// `*_recovery_key` helpers and call `is_better_recovery_key`
// directly. There is no per-candidate-type ranking wrapper.

} // namespace pegium::parser::detail
