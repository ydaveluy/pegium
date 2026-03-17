#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pegium/syntax-tree/CstNode.hpp>

namespace pegium::parser::detail {

enum class TerminalRecoveryChoiceKind : std::uint8_t {
  None,
  WordBoundarySplit,
  Fuzzy,
  InsertHidden,
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
  switch (kind) {
  case TerminalRecoveryChoiceKind::WordBoundarySplit:
    return 0u;
  case TerminalRecoveryChoiceKind::Fuzzy:
    return 1u;
  case TerminalRecoveryChoiceKind::InsertHidden:
    return 2u;
  case TerminalRecoveryChoiceKind::DeleteScan:
    return 3u;
  case TerminalRecoveryChoiceKind::None:
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
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
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
