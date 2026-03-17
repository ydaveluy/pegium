#pragma once

#include <cstddef>
#include <cstdint>

#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/RecoveryEditSupport.hpp>
#include <pegium/parser/RecoveryCandidate.hpp>
#include <pegium/parser/RecoveryUtils.hpp>

namespace pegium::parser::detail {

template <EditableParseModeContext Context, typename ApplyFn>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_terminal_recovery_candidate(Context &ctx, const char *cursorStart,
                                     TerminalRecoveryChoiceKind kind,
                                     std::uint32_t distance,
                                     std::uint32_t substitutionCount,
                                     std::uint32_t operationCount,
                                     ApplyFn &&applyFn,
                                     std::uint32_t extraNormalizedCost = 0u) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (std::forward<ApplyFn>(applyFn)()) {
    candidate = {
        .kind = kind,
        .normalizedEditCost =
            ctx.editCostDelta(checkpoint) + extraNormalizedCost,
        .distance = distance,
        .consumed = static_cast<std::size_t>(ctx.cursor() - cursorStart),
        .substitutionCount = substitutionCount,
        .operationCount = operationCount,
    };
  }
  ctx.rewind(checkpoint);
  return candidate;
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_insert_hidden_terminal_candidate(Context &ctx,
                                          const char *cursorStart,
                                          const Element *element) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::InsertHidden, 1u, 0u, 1u,
      [&]() { return apply_insert_hidden_recovery_edit(ctx, element); });
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_insert_hidden_gap_terminal_candidate(Context &ctx,
                                              const char *cursorStart,
                                              const char *position,
                                              const Element *element,
                                              std::uint32_t extraPenalty = 0u) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::WordBoundarySplit, 1u, 0u,
      1u,
      [&]() {
        return apply_insert_hidden_gap_recovery_edit(ctx, position, element);
      },
      extraPenalty);
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_replace_leaf_terminal_candidate(Context &ctx,
                                         const char *cursorStart,
                                         const char *endPtr,
                                         const Element *element,
                                         std::uint32_t editCost,
                                         std::uint32_t distance,
                                         std::uint32_t substitutionCount,
                                         std::uint32_t operationCount) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Fuzzy, distance,
      substitutionCount, operationCount,
      [&]() {
        return apply_replace_leaf_recovery_edit(ctx, endPtr, element, editCost);
      });
}

template <EditableParseModeContext Context, typename MatchFn, typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_delete_scan_terminal_candidate(Context &ctx, const char *cursorStart,
                                        MatchFn &&matchFn,
                                        OnMatchFn &&onMatchFn) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (recover_by_delete_scan(ctx, std::forward<MatchFn>(matchFn),
                             std::forward<OnMatchFn>(onMatchFn))) {
    const auto cost = ctx.editCostDelta(checkpoint);
    const auto deletedCost =
        default_edit_cost(ParseDiagnosticKind::Deleted);
    candidate = {
        .kind = TerminalRecoveryChoiceKind::DeleteScan,
        .normalizedEditCost = cost,
        .distance = deletedCost == 0u ? 0u : cost / deletedCost,
        .consumed = static_cast<std::size_t>(ctx.cursor() - cursorStart),
        .substitutionCount = 0u,
        .operationCount = deletedCost == 0u ? 0u : cost / deletedCost,
    };
  }
  ctx.rewind(checkpoint);
  return candidate;
}

template <EditableParseModeContext Context, typename MatchFn, typename OnMatchFn>
[[nodiscard]] bool apply_delete_scan_terminal_candidate(Context &ctx,
                                                        MatchFn &&matchFn,
                                                        OnMatchFn &&onMatchFn) {
  return recover_by_delete_scan(ctx, std::forward<MatchFn>(matchFn),
                                std::forward<OnMatchFn>(onMatchFn));
}

} // namespace pegium::parser::detail
