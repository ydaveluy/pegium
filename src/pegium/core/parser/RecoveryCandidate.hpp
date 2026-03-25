#pragma once

/// Candidate structures and ranking helpers used by parser recovery.

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pegium/core/syntax-tree/CstNode.hpp>

namespace pegium::parser::detail {

enum class TerminalRecoveryChoiceKind : std::uint8_t {
  None,
  WordBoundarySplit,
  Fuzzy,
  InsertSynthetic,
  DeleteScan,
};

struct TerminalRecoveryCandidate {
  TerminalRecoveryChoiceKind kind = TerminalRecoveryChoiceKind::None;
  std::uint32_t normalizedEditCost =
      std::numeric_limits<std::uint32_t>::max();
  std::uint32_t distance = std::numeric_limits<std::uint32_t>::max();
  std::size_t consumed = 0;
  std::uint32_t substitutionCount =
      std::numeric_limits<std::uint32_t>::max();
  std::uint32_t operationCount = std::numeric_limits<std::uint32_t>::max();
};

[[nodiscard]] constexpr std::uint8_t
terminal_recovery_choice_priority(TerminalRecoveryChoiceKind kind) noexcept {
  using enum TerminalRecoveryChoiceKind;
  switch (kind) {
  case WordBoundarySplit:
    return 0u;
  case Fuzzy:
    return 1u;
  case InsertSynthetic:
    return 2u;
  case DeleteScan:
    return 3u;
  case None:
    return 4u;
  }
  return 4u;
}

[[nodiscard]] constexpr bool is_better_terminal_recovery_candidate(
    const TerminalRecoveryCandidate &lhs,
    const TerminalRecoveryCandidate &rhs) noexcept {
  if (rhs.kind == TerminalRecoveryChoiceKind::None) {
    return lhs.kind != TerminalRecoveryChoiceKind::None;
  }
  const auto prefer_nearby_delete_scan_over_insert =
      [](const TerminalRecoveryCandidate &deleteScan,
         const TerminalRecoveryCandidate &insert) constexpr noexcept {
        return deleteScan.kind == TerminalRecoveryChoiceKind::DeleteScan &&
               insert.kind == TerminalRecoveryChoiceKind::InsertSynthetic &&
               deleteScan.consumed > insert.consumed &&
               deleteScan.operationCount <= 2u;
      };
  if (prefer_nearby_delete_scan_over_insert(lhs, rhs)) {
    return true;
  }
  if (prefer_nearby_delete_scan_over_insert(rhs, lhs)) {
    return false;
  }
  if (lhs.normalizedEditCost != rhs.normalizedEditCost) {
    return lhs.normalizedEditCost < rhs.normalizedEditCost;
  }
  if (lhs.distance != rhs.distance) {
    return lhs.distance < rhs.distance;
  }
  if (lhs.consumed != rhs.consumed) {
    return lhs.consumed > rhs.consumed;
  }
  if (lhs.substitutionCount != rhs.substitutionCount) {
    return lhs.substitutionCount < rhs.substitutionCount;
  }
  if (lhs.operationCount != rhs.operationCount) {
    return lhs.operationCount < rhs.operationCount;
  }
  return terminal_recovery_choice_priority(lhs.kind) <
         terminal_recovery_choice_priority(rhs.kind);
}

struct EditableRecoveryCandidate {
  bool matched = false;
  TextOffset cursorOffset = 0;
  // Cursor after one skipper pass from the matched position. This lets
  // choice recovery ignore progress that only comes from trailing trivia.
  TextOffset postSkipCursorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  const grammar::AbstractElement *firstEditElement = nullptr;
};

[[nodiscard]] constexpr bool is_better_editable_recovery_candidate(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs) noexcept {
  if (lhs.matched != rhs.matched) {
    return lhs.matched && !rhs.matched;
  }
  if (!lhs.matched) {
    return false;
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  if (lhs.editCount != rhs.editCount) {
    return lhs.editCount < rhs.editCount;
  }
  if (lhs.cursorOffset != rhs.cursorOffset) {
    return lhs.cursorOffset > rhs.cursorOffset;
  }
  if (lhs.firstEditOffset != rhs.firstEditOffset) {
    return lhs.firstEditOffset > rhs.firstEditOffset;
  }
  return false;
}

[[nodiscard]] constexpr bool is_better_choice_recovery_candidate(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs) noexcept {
  if (lhs.matched != rhs.matched) {
    return lhs.matched && !rhs.matched;
  }
  if (!lhs.matched) {
    return false;
  }
  if (lhs.postSkipCursorOffset != rhs.postSkipCursorOffset) {
    return lhs.postSkipCursorOffset > rhs.postSkipCursorOffset;
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  if (lhs.editCount != rhs.editCount) {
    return lhs.editCount < rhs.editCount;
  }
  if (lhs.firstEditOffset != rhs.firstEditOffset) {
    return lhs.firstEditOffset > rhs.firstEditOffset;
  }
  if (lhs.cursorOffset != rhs.cursorOffset) {
    return lhs.cursorOffset > rhs.cursorOffset;
  }
  return false;
}

[[nodiscard]] constexpr bool
is_better_choice_recovery_candidate_for_element(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs,
    const grammar::AbstractElement *preferredFirstEditElement) noexcept {
  if (is_better_choice_recovery_candidate(lhs, rhs)) {
    return true;
  }
  if (is_better_choice_recovery_candidate(rhs, lhs)) {
    return false;
  }
  if (preferredFirstEditElement == nullptr) {
    return false;
  }
  const bool lhsEditsCurrent = lhs.firstEditElement == preferredFirstEditElement;
  const bool rhsEditsCurrent = rhs.firstEditElement == preferredFirstEditElement;
  if (lhsEditsCurrent != rhsEditsCurrent) {
    return lhsEditsCurrent;
  }
  return false;
}

struct RecoveryProbeProgress {
  TextOffset committedOffset = 0;
  TextOffset exploredOffset = 0;

  [[nodiscard]] constexpr bool
  committedProgressed(TextOffset startOffset) const noexcept {
    return committedOffset > startOffset;
  }

  [[nodiscard]] constexpr bool
  exploredBeyond(TextOffset offset) const noexcept {
    return exploredOffset > offset;
  }

  [[nodiscard]] constexpr bool deferred() const noexcept {
    return exploredOffset > committedOffset;
  }

  [[nodiscard]] constexpr bool
  reachedEndOfInput(TextOffset endOffset) const noexcept {
    return exploredOffset == endOffset;
  }
};

struct ProgressRecoveryCandidate {
  bool matched = false;
  TextOffset cursorOffset = 0;
  std::uint32_t editCost = std::numeric_limits<std::uint32_t>::max();
};

[[nodiscard]] constexpr bool is_better_progress_recovery_candidate(
    const ProgressRecoveryCandidate &lhs,
    const ProgressRecoveryCandidate &rhs) noexcept {
  if (lhs.matched != rhs.matched) {
    return lhs.matched && !rhs.matched;
  }
  if (!lhs.matched) {
    return false;
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  if (lhs.cursorOffset != rhs.cursorOffset) {
    return lhs.cursorOffset > rhs.cursorOffset;
  }
  return false;
}

} // namespace pegium::parser::detail
