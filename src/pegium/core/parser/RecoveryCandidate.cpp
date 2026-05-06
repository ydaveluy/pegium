#include <pegium/core/parser/RecoveryCandidate.hpp>

namespace pegium::parser::detail {

ReplayPrefixClass
classify_editable_replay_prefix(std::uint32_t editCount,
                                bool hasDestructiveEdit) noexcept {
  if (editCount == 0u) {
    return ReplayPrefixClass::Empty;
  }
  if (hasDestructiveEdit) {
    return ReplayPrefixClass::ExtendedCommittedPrefix;
  }
  return ReplayPrefixClass::NewLocalPrefix;
}

ReplayPrefixClass
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

bool continues_after_first_edit(
    const EditableRecoveryCandidate &candidate) noexcept {
  return candidate.firstEditOffset != std::numeric_limits<TextOffset>::max() &&
         candidate.postSkipCursorOffset >
             candidate.firstEditOffset + candidate.editSpan;
}

// Lexicographic comparison on (firstEditOffset, editCost) is not enough:
// a high-cost candidate at a slightly later firstEditOffset can win over a
// cheaper, more reliable candidate at a slightly earlier offset. The score
// applies a graduated penalty proportional to excess editCost so a candidate
// that paid for `firstEditOffset` is only preferred when the gain dominates
// the cost spread. Intentionally non-monotone in `firstEditOffset` alone.
TextOffset recovery_key_first_edit_score(const RecoveryKey &key) noexcept {
  constexpr std::uint32_t kDeleteCost =
      default_edit_cost(ParseDiagnosticKind::Deleted);
  const auto penalty = key.editCost > kDeleteCost
                           ? (key.editCost - kDeleteCost) / kDeleteCost
                           : 0u;
  return key.firstEditOffset > penalty ? key.firstEditOffset - penalty : 0u;
}

bool is_better_recovery_key(const RecoveryKey &lhs,
                            const RecoveryKey &rhs) noexcept {
  if (lhs.fullMatch != rhs.fullMatch) {
    return lhs.fullMatch;
  }
  if (lhs.matched != rhs.matched) {
    return lhs.matched;
  }
  const auto lhsScore = recovery_key_first_edit_score(lhs);
  const auto rhsScore = recovery_key_first_edit_score(rhs);
  if (lhsScore != rhsScore) {
    return lhsScore > rhsScore;
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  return lhs.progressAfterEdits > rhs.progressAfterEdits;
}

RecoveryKey
terminal_recovery_key(const TerminalRecoveryCandidate &candidate) noexcept {
  return {
      .matched = candidate.kind != TerminalRecoveryChoiceKind::None,
      .editCost = candidate.cost.primaryRankCost,
      .progressAfterEdits = static_cast<TextOffset>(candidate.consumed),
  };
}

RecoveryKey
editable_recovery_key(const EditableRecoveryCandidate &candidate) noexcept {
  const TextOffset effectiveEditOffset =
      candidate.editCount == 0u ? candidate.postSkipCursorOffset
                                : candidate.firstEditOffset;
  return {
      .matched = candidate.matched,
      .firstEditOffset = effectiveEditOffset,
      .editCost = candidate.editCost,
      .progressAfterEdits = candidate.postSkipCursorOffset,
  };
}

RecoveryKey structural_progress_recovery_key(
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

} // namespace pegium::parser::detail
