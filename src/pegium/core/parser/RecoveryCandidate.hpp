#pragma once

/// Candidate structures and the unified recovery ranking key.
///
/// Comparison axes (in priority order):
///   1. `fullMatch`          (true wins)
///   2. `matched`            (true wins)
///   3. `firstEditOffset`    penalised by excess `editCost` so a
///                           later-but-expensive candidate does not
///                           silently outrank a cheaper one at a
///                           slightly earlier offset
///                           (`recovery_key_first_edit_score`)
///   4. `editCost`           (lower wins)
///   5. `progressAfterEdits` (higher wins)

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
  bool hasDestructiveEdit = false;
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
  /// to belong to the same replay-prefix equivalence class.
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
[[nodiscard]] ReplayPrefixClass
classify_editable_replay_prefix(std::uint32_t editCount,
                                bool hasDestructiveEdit) noexcept;

/// Classifies a structural-progress candidate's replay prefix. Same
/// closed mapping as `classify_editable_replay_prefix`, but the
/// "extension" signal is `hasDestructiveEdit` (Deleted OR Replaced)
/// — the term `Repetition` uses for the predicate that powers
/// `ExtendedCommittedPrefix` dominance. Both kinds of destructive
/// edit commit to a strictly different replay prefix and therefore
/// belong to the same equivalence class.
[[nodiscard]] ReplayPrefixClass
classify_structural_replay_prefix(bool hadEdits,
                                  bool hasDestructiveEdit) noexcept;

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
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  /// Closed classification of the candidate's replay prefix. Set at
  /// construction (`classify_structural_replay_prefix`); consumed by
  /// `Repetition`'s dominance predicates that require both candidates
  /// to belong to the same replay-prefix equivalence class.
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

  [[nodiscard]] constexpr bool
  exploredSingleVisibleLeafOrLess() const noexcept {
    return exploredVisibleLeafCount <= 1u;
  }
};

[[nodiscard]] bool
continues_after_first_edit(const EditableRecoveryCandidate &candidate) noexcept;

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

/// `firstEditOffset` penalised by edit cost: the score that axis 2
/// of the recovery key actually compares on.
///
/// A multi-byte delete that only wins on a 2-3 byte offset advantage
/// should not beat a short insert; but a single-char delete (cost 4)
/// should keep its baseline ranking. So:
///   - threshold = cost of a single Delete (the canonical "real" edit);
///     anything cheaper pays no penalty;
///   - above the threshold, each extra Delete-equivalent of cost
///     subtracts one offset unit from the score.
[[nodiscard]] TextOffset
recovery_key_first_edit_score(const RecoveryKey &key) noexcept;

[[nodiscard]] bool is_better_recovery_key(const RecoveryKey &lhs,
                                          const RecoveryKey &rhs) noexcept;

/// Terminal candidates use primaryRankCost (logical edit distance + strategy
/// penalty) as the cost axis. budgetCost measures the raw span deleted/
/// inserted, which inflates for fuzzy keyword repair and would lose to a
/// cheap-looking synthetic-gap split even when the fuzzy repair is the better
/// single-token edit.
[[nodiscard]] RecoveryKey
terminal_recovery_key(const TerminalRecoveryCandidate &candidate) noexcept;

/// Treat "no edit" as an edit at the candidate's current progress. Otherwise
/// a candidate with firstEditOffset = UINT_MAX would always beat an
/// insert-and-continue candidate on axis 2, even when the latter made much
/// more real progress.
[[nodiscard]] RecoveryKey
editable_recovery_key(const EditableRecoveryCandidate &candidate) noexcept;

[[nodiscard]] RecoveryKey structural_progress_recovery_key(
    const StructuralProgressRecoveryCandidate &candidate) noexcept;

// All dispatches project onto `RecoveryKey` via the type-specific
// `*_recovery_key` helpers and call `is_better_recovery_key`
// directly. There is no per-candidate-type ranking wrapper.

} // namespace pegium::parser::detail
