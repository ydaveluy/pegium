#include <pegium/core/parser/RecoveryCandidate.hpp>

namespace pegium::parser::detail {

ReplayPrefixClass classify_replay_prefix(bool hadEdits,
                                         bool hasDestructiveEdit) noexcept {
  using enum ReplayPrefixClass;
  if (!hadEdits) {
    return Empty;
  }
  if (hasDestructiveEdit) {
    return ExtendedCommittedPrefix;
  }
  return NewLocalPrefix;
}

bool continues_after_first_edit(
    const EditableRecoveryCandidate &candidate) noexcept {
  return candidate.firstEditOffset != std::numeric_limits<TextOffset>::max() &&
         candidate.postSkipCursorOffset > candidate.lastEditEndOffset;
}

// Lexicographic comparison on (firstEditOffset, editCost) is not enough:
// a high-cost candidate at a slightly later firstEditOffset can win over a
// cheaper, more reliable candidate at a slightly earlier offset. The score
// applies a graduated penalty proportional to excess editCost so a candidate
// that paid for `firstEditOffset` is only preferred when the gain dominates
// the cost spread. Intentionally non-monotone in `firstEditOffset` alone.
TextOffset recovery_key_first_edit_score(const RecoveryKey &key) noexcept {
  const auto penalty = monus(key.editCost, kFirstEditPenaltyStep) /
                       kFirstEditPenaltyStep;
  return monus(key.firstEditOffset, penalty);
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
  // Operation-taxed edit cost: fuses the former separate `editCost` and
  // `editCount` rungs into one scalar (`editCost + W * editCount`). Lower wins.
  const auto lhsFaithfulness = faithfulness(lhs);
  const auto rhsFaithfulness = faithfulness(rhs);
  if (lhsFaithfulness != rhsFaithfulness) {
    return lhsFaithfulness < rhsFaithfulness;
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
  // keyProgressOffset is producer-chosen: post-skip cursor for OrderedChoice /
  // Group, raw iteration cursor for Repetition (Invariant 2).
  const TextOffset effectiveEditOffset = effective_first_edit_offset(
      candidate.editCount != 0u, candidate.firstEditOffset,
      candidate.keyProgressOffset);
  return {
      .matched = candidate.matched,
      .firstEditOffset = effectiveEditOffset,
      .editCost = candidate.editCost,
      .editCount = candidate.editCount,
      .progressAfterEdits = candidate.keyProgressOffset,
  };
}

} // namespace pegium::parser::detail
