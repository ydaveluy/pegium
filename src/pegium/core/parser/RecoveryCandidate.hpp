#pragma once

/// Candidate structures and ranking helpers used by parser recovery.

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <pegium/core/parser/RecoveryCost.hpp>
#include <pegium/core/syntax-tree/CstNode.hpp>

namespace pegium::parser::detail {

template <typename T>
[[nodiscard]] constexpr bool prefer_higher(const T &lhs,
                                           const T &rhs) noexcept {
  return lhs > rhs;
}

template <typename T>
[[nodiscard]] constexpr bool prefer_lower(const T &lhs, const T &rhs) noexcept {
  return lhs < rhs;
}

template <typename T, typename PreferFn>
[[nodiscard]] constexpr bool compare_axis(const T &lhs, const T &rhs,
                                          PreferFn prefer,
                                          bool &decided) noexcept {
  if (lhs == rhs) {
    return false;
  }
  decided = true;
  return prefer(lhs, rhs);
}

struct NormalizedRecoveryOrderKey;

[[nodiscard]] constexpr bool compare_matched_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept;

[[nodiscard]] constexpr bool compare_edit_cost_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept;

[[nodiscard]] constexpr bool compare_edit_count_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept;

enum class TerminalRecoveryChoiceKind : std::uint8_t {
  None,
  Insert,
  Replace,
  DeleteScan,
};

enum class TerminalAnchorQuality : std::uint8_t {
  DirectVisible,
  AfterHiddenTrivia,
};

struct RecoveryPrefixAxis {
  bool entryRuleMatched = false;
  bool stable = false;
  bool credible = false;
  bool fullMatch = false;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
};

struct RecoveryContinuationAxis {
  bool continuesAfterFirstEdit = false;
  std::uint8_t strength = 0u;
  std::size_t consumedVisible = 0u;
  TextOffset postSkipCursorOffset = 0;
  TextOffset cursorOffset = 0;
  TerminalAnchorQuality anchorQuality =
      TerminalAnchorQuality::AfterHiddenTrivia;
};

struct RecoverySafetyAxis {
  bool matched = false;
  bool replaySafe = true;
  bool overflowed = false;
  std::uint8_t strategyPriority = 0u;
};

struct RecoveryEditAxis {
  std::uint32_t primaryRankCost = 0u;
  std::uint32_t secondaryRankCost = 0u;
  std::uint32_t distance = 0u;
  std::uint32_t substitutionCount = 0u;
  std::uint32_t operationCount = 0u;
  std::uint32_t editCost = 0u;
  std::uint32_t editCount = 0u;
  TextOffset editSpan = 0;
  std::uint32_t entryCount = 0u;
};

struct RecoveryProgressAxis {
  TextOffset parsedLength = 0;
  TextOffset maxCursorOffset = 0;
  TextOffset cursorOffset = 0;
};

struct NormalizedRecoveryOrderKey {
  RecoveryPrefixAxis prefix{};
  RecoveryContinuationAxis continuation{};
  RecoverySafetyAxis safety{};
  RecoveryEditAxis edits{};
  RecoveryProgressAxis progress{};
};

enum class RecoveryOrderAxis : std::uint8_t {
  Matched,
  Stable,
  Credible,
  FullMatch,
  PrimaryRankCost,
  SecondaryRankCost,
  Distance,
  SubstitutionCount,
  ConsumedVisible,
  OperationCount,
  AnchorQuality,
  StrategyPriority,
  EditCost,
  EditCount,
  PostSkipCursorOffset,
  FirstEditOffset,
  CursorOffset,
  ParsedLength,
  MaxCursorOffset,
  EditSpan,
  EntryCount,
  ReplaySafe,
  ContinuationStrength,
  ContinuesAfterFirstEdit,
};

[[nodiscard]] constexpr bool compare_recovery_order_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, RecoveryOrderAxis axis,
    bool &decided) noexcept {
  switch (axis) {
  case RecoveryOrderAxis::Matched:
    return compare_matched_axis(lhs, rhs, decided);
  case RecoveryOrderAxis::Stable:
    return compare_axis(lhs.prefix.stable, rhs.prefix.stable,
                        prefer_higher<bool>, decided);
  case RecoveryOrderAxis::Credible:
    return compare_axis(lhs.prefix.credible, rhs.prefix.credible,
                        prefer_higher<bool>, decided);
  case RecoveryOrderAxis::FullMatch:
    return compare_axis(lhs.prefix.fullMatch, rhs.prefix.fullMatch,
                        prefer_higher<bool>, decided);
  case RecoveryOrderAxis::PrimaryRankCost:
    return compare_axis(lhs.edits.primaryRankCost, rhs.edits.primaryRankCost,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::SecondaryRankCost:
    return compare_axis(lhs.edits.secondaryRankCost,
                        rhs.edits.secondaryRankCost,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::Distance:
    return compare_axis(lhs.edits.distance, rhs.edits.distance,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::SubstitutionCount:
    return compare_axis(lhs.edits.substitutionCount,
                        rhs.edits.substitutionCount,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::ConsumedVisible:
    return compare_axis(lhs.continuation.consumedVisible,
                        rhs.continuation.consumedVisible,
                        prefer_higher<std::size_t>, decided);
  case RecoveryOrderAxis::OperationCount:
    return compare_axis(lhs.edits.operationCount, rhs.edits.operationCount,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::AnchorQuality:
    return compare_axis(lhs.continuation.anchorQuality,
                        rhs.continuation.anchorQuality,
                        prefer_lower<TerminalAnchorQuality>, decided);
  case RecoveryOrderAxis::StrategyPriority:
    return compare_axis(lhs.safety.strategyPriority,
                        rhs.safety.strategyPriority,
                        prefer_lower<std::uint8_t>, decided);
  case RecoveryOrderAxis::EditCost:
    return compare_edit_cost_axis(lhs, rhs, decided);
  case RecoveryOrderAxis::EditCount:
    return compare_edit_count_axis(lhs, rhs, decided);
  case RecoveryOrderAxis::PostSkipCursorOffset:
    return compare_axis(lhs.continuation.postSkipCursorOffset,
                        rhs.continuation.postSkipCursorOffset,
                        prefer_higher<TextOffset>, decided);
  case RecoveryOrderAxis::FirstEditOffset:
    return compare_axis(lhs.prefix.firstEditOffset, rhs.prefix.firstEditOffset,
                        prefer_higher<TextOffset>, decided);
  case RecoveryOrderAxis::CursorOffset:
    return compare_axis(lhs.progress.cursorOffset, rhs.progress.cursorOffset,
                        prefer_higher<TextOffset>, decided);
  case RecoveryOrderAxis::ParsedLength:
    return compare_axis(lhs.progress.parsedLength, rhs.progress.parsedLength,
                        prefer_higher<TextOffset>, decided);
  case RecoveryOrderAxis::MaxCursorOffset:
    return compare_axis(lhs.progress.maxCursorOffset,
                        rhs.progress.maxCursorOffset,
                        prefer_higher<TextOffset>, decided);
  case RecoveryOrderAxis::EditSpan:
    return compare_axis(lhs.edits.editSpan, rhs.edits.editSpan,
                        prefer_lower<TextOffset>, decided);
  case RecoveryOrderAxis::EntryCount:
    return compare_axis(lhs.edits.entryCount, rhs.edits.entryCount,
                        prefer_lower<std::uint32_t>, decided);
  case RecoveryOrderAxis::ReplaySafe:
    return compare_axis(lhs.safety.replaySafe, rhs.safety.replaySafe,
                        prefer_higher<bool>, decided);
  case RecoveryOrderAxis::ContinuationStrength:
    return compare_axis(lhs.continuation.strength, rhs.continuation.strength,
                        prefer_higher<std::uint8_t>, decided);
  case RecoveryOrderAxis::ContinuesAfterFirstEdit:
    return compare_axis(lhs.continuation.continuesAfterFirstEdit,
                        rhs.continuation.continuesAfterFirstEdit,
                        prefer_higher<bool>, decided);
  }
  return false;
}

template <std::size_t AxisCount>
[[nodiscard]] constexpr bool compare_recovery_order_key_axes(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs,
    const std::array<RecoveryOrderAxis, AxisCount> &axes) noexcept {
  bool decided = false;
  for (const auto axis : axes) {
    if (compare_recovery_order_axis(lhs, rhs, axis, decided)) {
      return true;
    }
    if (decided) {
      return false;
    }
  }
  return false;
}

template <std::size_t AxisCount>
[[nodiscard]] constexpr bool decide_recovery_order_key_axes(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs,
    const std::array<RecoveryOrderAxis, AxisCount> &axes,
    bool &decided) noexcept {
  if (compare_recovery_order_key_axes(lhs, rhs, axes)) {
    decided = true;
    return true;
  }
  if (compare_recovery_order_key_axes(rhs, lhs, axes)) {
    decided = true;
    return false;
  }
  decided = false;
  return false;
}

[[nodiscard]] constexpr bool compare_matched_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  return compare_axis(lhs.safety.matched, rhs.safety.matched,
                      prefer_higher<bool>, decided);
}

[[nodiscard]] constexpr bool compare_edit_cost_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  return compare_axis(lhs.edits.editCost, rhs.edits.editCost,
                      prefer_lower<std::uint32_t>, decided);
}

[[nodiscard]] constexpr bool compare_edit_count_axis(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  return compare_axis(lhs.edits.editCount, rhs.edits.editCount,
                      prefer_lower<std::uint32_t>, decided);
}

enum class RecoveryOrderProfile : std::uint8_t {
  Terminal,
  Editable,
  Choice,
  Progress,
  StructuralProgress,
  Attempt,
};

struct TerminalRecoveryCandidate {
  TerminalRecoveryChoiceKind kind = TerminalRecoveryChoiceKind::None;
  TerminalAnchorQuality anchorQuality =
      TerminalAnchorQuality::AfterHiddenTrivia;
  RecoveryCost cost{};
  std::uint32_t distance = std::numeric_limits<std::uint32_t>::max();
  std::size_t consumed = 0;
  std::uint32_t substitutionCount = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t operationCount = std::numeric_limits<std::uint32_t>::max();
};

[[nodiscard]] constexpr std::uint8_t
terminal_recovery_choice_priority(TerminalRecoveryChoiceKind kind) noexcept;

[[nodiscard]] constexpr NormalizedRecoveryOrderKey terminal_recovery_order_key(
    const TerminalRecoveryCandidate &candidate) noexcept {
  NormalizedRecoveryOrderKey key;
  key.safety.matched = candidate.kind != TerminalRecoveryChoiceKind::None;
  key.safety.strategyPriority =
      terminal_recovery_choice_priority(candidate.kind);
  key.continuation.anchorQuality = candidate.anchorQuality;
  key.continuation.consumedVisible = candidate.consumed;
  key.edits.primaryRankCost = candidate.cost.primaryRankCost;
  key.edits.secondaryRankCost = candidate.cost.secondaryRankCost;
  key.edits.distance = candidate.distance;
  key.edits.substitutionCount = candidate.substitutionCount;
  key.edits.operationCount = candidate.operationCount;
  key.edits.editCost = candidate.cost.budgetCost;
  return key;
}

[[nodiscard]] constexpr std::uint8_t
terminal_recovery_choice_priority(TerminalRecoveryChoiceKind kind) noexcept {
  using enum TerminalRecoveryChoiceKind;
  switch (kind) {
  case Insert:
    return 0u;
  case Replace:
    return 1u;
  case DeleteScan:
    return 2u;
  case None:
    return 3u;
  }
  return 3u;
}

[[nodiscard]] constexpr bool compare_terminal_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  constexpr auto kAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::PrimaryRankCost,
      RecoveryOrderAxis::SecondaryRankCost,
      RecoveryOrderAxis::Distance,
      RecoveryOrderAxis::SubstitutionCount,
      RecoveryOrderAxis::ConsumedVisible,
      RecoveryOrderAxis::OperationCount,
      RecoveryOrderAxis::AnchorQuality,
      RecoveryOrderAxis::StrategyPriority,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kAxes);
}

[[nodiscard]] constexpr bool is_better_normalized_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey,
    RecoveryOrderProfile profile) noexcept;

struct EditableRecoveryCandidate {
  bool matched = false;
  bool hasDeleteEdit = false;
  TextOffset cursorOffset = 0;
  // Cursor after one skipper pass from the matched position. This lets
  // choice recovery ignore progress that only comes from trailing trivia.
  TextOffset postSkipCursorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  TextOffset editSpan = 0;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  const grammar::AbstractElement *firstEditElement = nullptr;
};

struct ChoiceRecoveryOrderConstraints {
  TextOffset parseStartOffset = std::numeric_limits<TextOffset>::max();
  const grammar::AbstractElement *preferredBoundaryElement = nullptr;
};

[[nodiscard]] constexpr TextOffset editable_recovery_progress(
    const EditableRecoveryCandidate &candidate) noexcept {
  return candidate.postSkipCursorOffset;
}

[[nodiscard]] constexpr bool continues_after_first_edit(
    const EditableRecoveryCandidate &candidate) noexcept {
  return candidate.firstEditOffset != std::numeric_limits<TextOffset>::max() &&
         editable_recovery_progress(candidate) >
             saturating_add(candidate.firstEditOffset, candidate.editSpan);
}

[[nodiscard]] constexpr NormalizedRecoveryOrderKey editable_recovery_order_key(
    const EditableRecoveryCandidate &candidate) noexcept {
  NormalizedRecoveryOrderKey key;
  key.safety.matched = candidate.matched;
  key.prefix.firstEditOffset = candidate.firstEditOffset;
  key.continuation.continuesAfterFirstEdit =
      continues_after_first_edit(candidate);
  key.continuation.postSkipCursorOffset = editable_recovery_progress(candidate);
  key.continuation.cursorOffset = candidate.cursorOffset;
  key.edits.editCost = candidate.editCost;
  key.edits.editCount = candidate.editCount;
  key.edits.editSpan = candidate.editSpan;
  key.progress.cursorOffset = candidate.cursorOffset;
  return key;
}

[[nodiscard]] constexpr bool compare_editable_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  constexpr auto kAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::EditCount,
      RecoveryOrderAxis::PostSkipCursorOffset,
      RecoveryOrderAxis::FirstEditOffset,
      RecoveryOrderAxis::CursorOffset,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kAxes);
}

[[nodiscard]] constexpr bool decide_choice_base_axes(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  constexpr auto kBaseAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::ContinuesAfterFirstEdit,
  };
  return decide_recovery_order_key_axes(lhs, rhs, kBaseAxes, decided);
}

[[nodiscard]] constexpr bool decide_same_start_choice_continuation_first_edit(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  const bool lhsCanAffordLaterFirstEdit =
      lhs.edits.editCost <= rhs.edits.editCost &&
      lhs.edits.editCount <= rhs.edits.editCount;
  const bool rhsCanAffordLaterFirstEdit =
      rhs.edits.editCost <= lhs.edits.editCost &&
      rhs.edits.editCount <= lhs.edits.editCount;
  if (!lhs.continuation.continuesAfterFirstEdit ||
      !rhs.continuation.continuesAfterFirstEdit ||
      lhsCanAffordLaterFirstEdit != rhsCanAffordLaterFirstEdit) {
    return false;
  }
  return compare_axis(lhs.prefix.firstEditOffset, rhs.prefix.firstEditOffset,
                      prefer_higher<TextOffset>, decided);
}

[[nodiscard]] constexpr bool decide_same_start_choice_continuation_strength(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  if (lhs.prefix.firstEditOffset != rhs.prefix.firstEditOffset ||
      !lhs.continuation.continuesAfterFirstEdit ||
      !rhs.continuation.continuesAfterFirstEdit) {
    return false;
  }
  const auto lhsGain =
      lhs.continuation.postSkipCursorOffset - lhs.prefix.firstEditOffset;
  const auto rhsGain =
      rhs.continuation.postSkipCursorOffset - rhs.prefix.firstEditOffset;
  const auto lhsCost = std::max<std::uint32_t>(1u, lhs.edits.editCost);
  const auto rhsCost = std::max<std::uint32_t>(1u, rhs.edits.editCost);
  if (compare_axis(static_cast<std::uint64_t>(lhsGain) * rhsCost,
                   static_cast<std::uint64_t>(rhsGain) * lhsCost,
                   prefer_higher<std::uint64_t>, decided)) {
    return true;
  }
  if (decided) {
    return false;
  }
  constexpr auto kContinuationAxes = std::array{
      RecoveryOrderAxis::PostSkipCursorOffset,
  };
  return decide_recovery_order_key_axes(lhs, rhs, kContinuationAxes, decided);
}

[[nodiscard]] constexpr bool decide_same_start_choice_rewrite_axes(
    const NormalizedRecoveryOrderKey &lhs,
    const NormalizedRecoveryOrderKey &rhs, bool &decided) noexcept {
  if (lhs.prefix.firstEditOffset != rhs.prefix.firstEditOffset) {
    return false;
  }
  constexpr auto kRewriteAxes = std::array{
      RecoveryOrderAxis::EditSpan,
      RecoveryOrderAxis::PostSkipCursorOffset,
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::EditCount,
  };
  return decide_recovery_order_key_axes(lhs, rhs, kRewriteAxes, decided);
}

[[nodiscard]] constexpr bool compare_choice_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  bool decided = false;
  if (decide_choice_base_axes(lhsKey, rhsKey, decided)) {
    return true;
  }
  if (decided) {
    return false;
  }
  if (decide_same_start_choice_continuation_first_edit(lhsKey, rhsKey,
                                                       decided)) {
    return true;
  }
  if (decided) {
    return false;
  }
  if (decide_same_start_choice_continuation_strength(lhsKey, rhsKey,
                                                     decided)) {
    return true;
  }
  if (decided) {
    return false;
  }
  if (decide_same_start_choice_rewrite_axes(lhsKey, rhsKey, decided)) {
    return true;
  }
  if (decided) {
    return false;
  }
  constexpr auto kFinalAxes = std::array{
      RecoveryOrderAxis::PostSkipCursorOffset,
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::EditCount,
      RecoveryOrderAxis::FirstEditOffset,
      RecoveryOrderAxis::CursorOffset,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kFinalAxes);
}

[[nodiscard]] constexpr bool preserves_clean_no_edit_choice_boundary(
    const EditableRecoveryCandidate &candidate,
    TextOffset parseStartOffset) noexcept {
  return candidate.matched && candidate.editCount == 0u &&
         candidate.postSkipCursorOffset > parseStartOffset;
}

[[nodiscard]] constexpr bool rewrites_clean_no_edit_choice_boundary(
    const EditableRecoveryCandidate &candidate,
    TextOffset cleanBoundaryOffset) noexcept {
  return candidate.matched && candidate.editCount != 0u &&
         candidate.firstEditOffset <= cleanBoundaryOffset;
}

[[nodiscard]] constexpr bool decide_clean_no_edit_choice_boundary(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs, TextOffset parseStartOffset,
    bool &decided) noexcept {
  if (preserves_clean_no_edit_choice_boundary(lhs, parseStartOffset) &&
      rewrites_clean_no_edit_choice_boundary(rhs, lhs.postSkipCursorOffset)) {
    decided = true;
    return true;
  }
  if (preserves_clean_no_edit_choice_boundary(rhs, parseStartOffset) &&
      rewrites_clean_no_edit_choice_boundary(lhs, rhs.postSkipCursorOffset)) {
    decided = true;
    return false;
  }
  return false;
}

[[nodiscard]] constexpr bool decide_preferred_same_start_choice_boundary_edit(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs,
    const grammar::AbstractElement &preferredBoundaryElement,
    bool &decided) noexcept {
  if (!lhs.matched || !rhs.matched || lhs.editCount == 0u ||
      rhs.editCount == 0u || lhs.firstEditOffset != rhs.firstEditOffset) {
    return false;
  }

  const bool lhsTouchesPreferredBoundary =
      lhs.firstEditElement == std::addressof(preferredBoundaryElement);
  const bool rhsTouchesPreferredBoundary =
      rhs.firstEditElement == std::addressof(preferredBoundaryElement);
  if (lhsTouchesPreferredBoundary == rhsTouchesPreferredBoundary) {
    return false;
  }

  const bool lhsNoWorseRewrite =
      lhs.editSpan <= rhs.editSpan && lhs.editCost <= rhs.editCost &&
      lhs.editCount <= rhs.editCount;
  const bool rhsNoWorseRewrite =
      rhs.editSpan <= lhs.editSpan && rhs.editCost <= lhs.editCost &&
      rhs.editCount <= lhs.editCount;
  if (compare_axis(lhsTouchesPreferredBoundary && lhsNoWorseRewrite,
                   rhsTouchesPreferredBoundary && rhsNoWorseRewrite,
                   prefer_higher<bool>, decided)) {
    return true;
  }
  if (decided) {
    return false;
  }

  if (lhs.editSpan == rhs.editSpan && lhs.editCost == rhs.editCost &&
      lhs.editCount == rhs.editCount &&
      compare_axis(lhsTouchesPreferredBoundary, rhsTouchesPreferredBoundary,
                   prefer_higher<bool>, decided)) {
    return true;
  }
  return false;
}

[[nodiscard]] constexpr bool is_better_choice_recovery_candidate(
    const EditableRecoveryCandidate &lhs,
    const EditableRecoveryCandidate &rhs,
    ChoiceRecoveryOrderConstraints constraints = {}) noexcept {
  // A replay that preserves an already visible no-edit suffix keeps the choice
  // boundary committed unless the competing replay starts strictly after it.
  bool decided = false;
  if (constraints.parseStartOffset !=
      std::numeric_limits<TextOffset>::max()) {
    if (decide_clean_no_edit_choice_boundary(
            lhs, rhs, constraints.parseStartOffset, decided)) {
      return true;
    }
    if (decided) {
      return false;
    }
  }
  if (constraints.preferredBoundaryElement != nullptr) {
    if (decide_preferred_same_start_choice_boundary_edit(
            lhs, rhs, *constraints.preferredBoundaryElement, decided)) {
      return true;
    }
    if (decided) {
      return false;
    }
  }
  return is_better_normalized_recovery_order_key(
      editable_recovery_order_key(lhs), editable_recovery_order_key(rhs),
      RecoveryOrderProfile::Choice);
}

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
  reachedEndOfInput(TextOffset endOffset) const noexcept {
    return furthestExploredOffset == endOffset;
  }

  [[nodiscard]] constexpr bool
  exploredSingleVisibleLeafOrLess() const noexcept {
    return exploredVisibleLeafCount <= 1u;
  }
};

struct ProgressRecoveryCandidate {
  bool matched = false;
  TextOffset cursorOffset = 0;
  std::uint32_t editCost = std::numeric_limits<std::uint32_t>::max();
};

struct StructuralProgressRecoveryCandidate {
  bool matched = false;
  TextOffset cursorOffset = 0;
  std::uint32_t editCost = std::numeric_limits<std::uint32_t>::max();
  std::uint8_t strategyPriority = 0u;
  bool hadEdits = false;
  bool continuesAfterFirstEdit = true;
  bool rewritesParseStartBoundary = false;
  ParseDiagnosticKind firstEditKind = ParseDiagnosticKind::Incomplete;
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  const grammar::AbstractElement *firstEditElement = nullptr;
};

struct StructuralProgressRecoveryOrderConstraints {
  TextOffset parseStartOffset = std::numeric_limits<TextOffset>::max();
};

[[nodiscard]] constexpr bool
is_same_start_boundary_literal_insert(
    const StructuralProgressRecoveryCandidate &candidate,
    TextOffset parseStartOffset) noexcept {
  return candidate.matched && candidate.hadEdits &&
         candidate.firstEditKind == ParseDiagnosticKind::Inserted &&
         candidate.firstEditElement != nullptr &&
         candidate.firstEditElement->getKind() == grammar::ElementKind::Literal &&
         candidate.firstEditOffset == parseStartOffset;
}

[[nodiscard]] constexpr StructuralProgressRecoveryCandidate
make_structural_progress_recovery_candidate(
    const ProgressRecoveryCandidate &candidate, std::uint8_t strategyPriority,
    bool hadEdits = false,
    bool continuesAfterFirstEdit = true,
    bool rewritesParseStartBoundary = false,
    ParseDiagnosticKind firstEditKind = ParseDiagnosticKind::Incomplete,
    TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max(),
    const grammar::AbstractElement *firstEditElement = nullptr) noexcept {
  return {
      .matched = candidate.matched,
      .cursorOffset = candidate.cursorOffset,
      .editCost = candidate.editCost,
      .strategyPriority = strategyPriority,
      .hadEdits = hadEdits,
      .continuesAfterFirstEdit = continuesAfterFirstEdit,
      .rewritesParseStartBoundary = rewritesParseStartBoundary,
      .firstEditKind = firstEditKind,
      .firstEditOffset = firstEditOffset,
      .firstEditElement = firstEditElement,
  };
}

[[nodiscard]] constexpr NormalizedRecoveryOrderKey progress_recovery_order_key(
    const ProgressRecoveryCandidate &candidate) noexcept {
  NormalizedRecoveryOrderKey key;
  key.safety.matched = candidate.matched;
  key.edits.editCost = candidate.editCost;
  key.progress.cursorOffset = candidate.cursorOffset;
  return key;
}

[[nodiscard]] constexpr NormalizedRecoveryOrderKey
structural_progress_recovery_order_key(
    const StructuralProgressRecoveryCandidate &candidate) noexcept {
  auto key = progress_recovery_order_key(
      {.matched = candidate.matched,
       .cursorOffset = candidate.cursorOffset,
       .editCost = candidate.editCost});
  key.safety.strategyPriority = candidate.strategyPriority;
  key.safety.replaySafe = !candidate.rewritesParseStartBoundary;
  key.continuation.continuesAfterFirstEdit =
      candidate.continuesAfterFirstEdit;
  key.continuation.strength = candidate.hadEdits
                                  ? (candidate.continuesAfterFirstEdit ? 2u : 0u)
                                  : 1u;
  return key;
}

[[nodiscard]] constexpr bool compare_progress_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  constexpr auto kAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::CursorOffset,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kAxes);
}

[[nodiscard]] constexpr bool compare_structural_progress_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  constexpr auto kAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::ReplaySafe,
      RecoveryOrderAxis::ContinuationStrength,
      RecoveryOrderAxis::StrategyPriority,
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::CursorOffset,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kAxes);
}

[[nodiscard]] constexpr bool is_better_structural_progress_recovery_candidate(
    const StructuralProgressRecoveryCandidate &lhs,
    const StructuralProgressRecoveryCandidate &rhs,
    StructuralProgressRecoveryOrderConstraints constraints = {}) noexcept {
  if (constraints.parseStartOffset !=
      std::numeric_limits<TextOffset>::max()) {
    const bool lhsBoundaryLiteralInsert =
        is_same_start_boundary_literal_insert(lhs, constraints.parseStartOffset);
    const bool rhsBoundaryLiteralInsert =
        is_same_start_boundary_literal_insert(rhs, constraints.parseStartOffset);
    if (lhsBoundaryLiteralInsert != rhsBoundaryLiteralInsert &&
        lhs.matched && rhs.matched && lhs.hadEdits && rhs.hadEdits &&
        lhs.firstEditOffset == rhs.firstEditOffset &&
        lhs.firstEditOffset == constraints.parseStartOffset) {
      const bool lhsNoWorseBoundaryInsert =
          lhsBoundaryLiteralInsert && lhs.editCost <= rhs.editCost &&
          (lhs.editCost < rhs.editCost || !lhs.continuesAfterFirstEdit);
      const bool rhsNoWorseBoundaryInsert =
          rhsBoundaryLiteralInsert && rhs.editCost <= lhs.editCost &&
          (rhs.editCost < lhs.editCost || !rhs.continuesAfterFirstEdit);
      if (lhsNoWorseBoundaryInsert != rhsNoWorseBoundaryInsert) {
        return lhsNoWorseBoundaryInsert;
      }
    }
  }
  return is_better_normalized_recovery_order_key(
      structural_progress_recovery_order_key(lhs),
      structural_progress_recovery_order_key(rhs),
      RecoveryOrderProfile::StructuralProgress);
}

[[nodiscard]] constexpr bool compare_attempt_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey) noexcept {
  bool decided = false;
  constexpr auto kSelectionAxes = std::array{
      RecoveryOrderAxis::Matched,
      RecoveryOrderAxis::Stable,
      RecoveryOrderAxis::Credible,
      RecoveryOrderAxis::FullMatch,
  };
  if (decide_recovery_order_key_axes(lhsKey, rhsKey, kSelectionAxes,
                                     decided)) {
    return true;
  }
  if (decided) {
    return false;
  }

  constexpr auto kProgressAxes = std::array{
      RecoveryOrderAxis::ParsedLength,
      RecoveryOrderAxis::MaxCursorOffset,
  };
  if (decide_recovery_order_key_axes(lhsKey, rhsKey, kProgressAxes,
                                     decided)) {
    return true;
  }
  if (decided) {
    return false;
  }

  const bool lhsCanAffordLaterFirstEdit =
      lhsKey.prefix.firstEditOffset > rhsKey.prefix.firstEditOffset &&
      lhsKey.edits.editCost <= rhsKey.edits.editCost &&
      lhsKey.edits.editSpan <= rhsKey.edits.editSpan;
  const bool rhsCanAffordLaterFirstEdit =
      rhsKey.prefix.firstEditOffset > lhsKey.prefix.firstEditOffset &&
      rhsKey.edits.editCost <= lhsKey.edits.editCost &&
      rhsKey.edits.editSpan <= lhsKey.edits.editSpan;
  if (compare_axis(lhsCanAffordLaterFirstEdit, rhsCanAffordLaterFirstEdit,
                   prefer_higher<bool>, decided)) {
    return true;
  }
  if (decided) {
    return false;
  }

  constexpr auto kEditAxes = std::array{
      RecoveryOrderAxis::EditCost,
      RecoveryOrderAxis::EntryCount,
      RecoveryOrderAxis::EditSpan,
      RecoveryOrderAxis::FirstEditOffset,
  };
  return compare_recovery_order_key_axes(lhsKey, rhsKey, kEditAxes);
}

[[nodiscard]] constexpr bool is_better_normalized_recovery_order_key(
    const NormalizedRecoveryOrderKey &lhsKey,
    const NormalizedRecoveryOrderKey &rhsKey,
    RecoveryOrderProfile profile) noexcept {
  switch (profile) {
  case RecoveryOrderProfile::Terminal:
    return compare_terminal_recovery_order_key(lhsKey, rhsKey);
  case RecoveryOrderProfile::Editable:
    return compare_editable_recovery_order_key(lhsKey, rhsKey);
  case RecoveryOrderProfile::Choice:
    return compare_choice_recovery_order_key(lhsKey, rhsKey);
  case RecoveryOrderProfile::Progress:
    return compare_progress_recovery_order_key(lhsKey, rhsKey);
  case RecoveryOrderProfile::StructuralProgress:
    return compare_structural_progress_recovery_order_key(lhsKey, rhsKey);
  case RecoveryOrderProfile::Attempt:
    return compare_attempt_recovery_order_key(lhsKey, rhsKey);
  }
  return false;
}

} // namespace pegium::parser::detail
