#pragma once

/// Terminal-level recovery candidate evaluation helpers.

#include <algorithm>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryCost.hpp>
#include <pegium/core/parser/RecoveryEditSupport.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>

namespace pegium::parser::detail {

enum class LexicalRecoveryProfileKind : std::uint8_t {
  WordLikeLiteral,
  WordLikeFreeForm,
  Separator,
  Delimiter,
  OperatorLike,
  OpaqueSymbol,
};

inline constexpr std::uint32_t kCompactHiddenTriviaGapSpan = 8u;

struct LexicalRecoveryProfile {
  LexicalRecoveryProfileKind kind = LexicalRecoveryProfileKind::OpaqueSymbol;
  bool singleCodepoint = false;
  std::uint32_t textLength = 0u;

  [[nodiscard]] constexpr bool allowsMatchedInsert() const noexcept {
    return kind == LexicalRecoveryProfileKind::WordLikeLiteral;
  }

  [[nodiscard]] constexpr bool allowsReplace() const noexcept {
    return kind == LexicalRecoveryProfileKind::WordLikeLiteral ||
           (kind == LexicalRecoveryProfileKind::OperatorLike &&
            !singleCodepoint);
  }

  [[nodiscard]] constexpr bool allowsInsert() const noexcept {
    return kind == LexicalRecoveryProfileKind::Separator ||
           kind == LexicalRecoveryProfileKind::Delimiter ||
           (kind == LexicalRecoveryProfileKind::OperatorLike &&
            singleCodepoint) ||
           (kind == LexicalRecoveryProfileKind::WordLikeLiteral &&
            singleCodepoint);
  }
};

struct TriviaGapProfile {
  std::uint32_t hiddenCodepointSpan = 0u;
  bool visibleSourceAfterLocalSkip = false;

  [[nodiscard]] constexpr bool hasHiddenGap() const noexcept {
    return hiddenCodepointSpan > 0u;
  }

  [[nodiscard]] constexpr bool isCompact(
      std::uint32_t maxSpan = kCompactHiddenTriviaGapSpan) const noexcept {
    return hasHiddenGap() && hiddenCodepointSpan <= maxSpan;
  }
};

[[nodiscard]] constexpr TerminalAnchorQuality
terminal_anchor_quality(const TriviaGapProfile &profile) noexcept {
  return profile.hasHiddenGap() ? TerminalAnchorQuality::AfterHiddenTrivia
                                : TerminalAnchorQuality::DirectVisible;
}

[[nodiscard]] constexpr std::uint32_t
terminal_anchor_rank_penalty(const TriviaGapProfile &profile) noexcept {
  switch (terminal_anchor_quality(profile)) {
  case TerminalAnchorQuality::DirectVisible:
    return 0u;
  case TerminalAnchorQuality::AfterHiddenTrivia:
    return 1u;
  }
  return 1u;
}

[[nodiscard]] constexpr bool
is_separator_recovery_symbol(std::string_view value) noexcept {
  return value.size() == 1u &&
         (value[0] == ',' || value[0] == ';' || value[0] == ':');
}

[[nodiscard]] constexpr bool
is_delimiter_recovery_symbol(std::string_view value) noexcept {
  return value.size() == 1u &&
         (value[0] == '(' || value[0] == ')' || value[0] == '[' ||
          value[0] == ']' || value[0] == '{' || value[0] == '}');
}

[[nodiscard]] constexpr LexicalRecoveryProfile
classify_literal_recovery_profile(std::string_view value) noexcept {
  if (value.empty()) {
    return {};
  }
  if (is_word_like_terminal(value)) {
    return {.kind = LexicalRecoveryProfileKind::WordLikeLiteral,
            .singleCodepoint = value.size() == 1u,
            .textLength = static_cast<std::uint32_t>(value.size())};
  }
  if (is_separator_recovery_symbol(value)) {
    return {.kind = LexicalRecoveryProfileKind::Separator,
            .singleCodepoint = true,
            .textLength = static_cast<std::uint32_t>(value.size())};
  }
  if (is_delimiter_recovery_symbol(value)) {
    return {.kind = LexicalRecoveryProfileKind::Delimiter,
            .singleCodepoint = true,
            .textLength = static_cast<std::uint32_t>(value.size())};
  }
  return {.kind = LexicalRecoveryProfileKind::OperatorLike,
          .singleCodepoint = value.size() == 1u,
          .textLength = static_cast<std::uint32_t>(value.size())};
}

template <typename Element>
[[nodiscard]] constexpr LexicalRecoveryProfile
infer_terminal_rule_recovery_profile(const Element &element) noexcept {
  if constexpr (requires {
                  {
                    element.getValue()
                  } -> std::convertible_to<std::string_view>;
                }) {
    return classify_literal_recovery_profile(element.getValue());
  } else {
    return {.kind = detail::element_is_word_like_terminal(element)
                        ? LexicalRecoveryProfileKind::WordLikeFreeForm
                        : LexicalRecoveryProfileKind::OpaqueSymbol,
            .singleCodepoint = false,
            .textLength = 0u};
  }
}

[[nodiscard]] constexpr bool
literal_has_word_boundary_violation(std::string_view literalValue,
                                    const char *end) noexcept {
  return !literalValue.empty() && is_word_like_terminal(literalValue) &&
         end != nullptr &&
         is_identifier_like_codepoint(static_cast<unsigned char>(*end));
}

struct DirectLiteralRecoveryMetadata {
  std::string_view value{};
  bool caseSensitive = true;
};

template <typename Element>
[[nodiscard]] constexpr std::optional<DirectLiteralRecoveryMetadata>
infer_direct_literal_recovery_metadata(const Element &element) noexcept {
  if constexpr (requires {
                  {
                    element.getValue()
                  } -> std::convertible_to<std::string_view>;
                  { element.isCaseSensitive() } -> std::convertible_to<bool>;
                }) {
    return DirectLiteralRecoveryMetadata{
        .value = element.getValue(),
        .caseSensitive = element.isCaseSensitive(),
    };
  } else {
    return std::nullopt;
  }
}

struct TerminalRecoveryFacts {
  TriviaGapProfile triviaGap{};
  bool previousElementIsTerminalish = false;
  bool allowStructuredVisibleContinuationInsert = false;
};

template <typename Context>
[[nodiscard]] constexpr bool
cursor_starts_visible_source(const Context &ctx) noexcept;

template <typename Context>
[[nodiscard]] TriviaGapProfile
make_trivia_gap_profile(const Context &ctx, const char *gapBegin,
                        const char *gapEnd) noexcept;

template <typename Context>
[[nodiscard]] constexpr bool
allows_terminal_rule_insert(const Context &ctx,
                            const LexicalRecoveryProfile &profile) noexcept {
  switch (profile.kind) {
  case LexicalRecoveryProfileKind::WordLikeFreeForm:
  case LexicalRecoveryProfileKind::OpaqueSymbol:
    return !cursor_starts_visible_source(ctx);
  case LexicalRecoveryProfileKind::WordLikeLiteral:
  case LexicalRecoveryProfileKind::Separator:
  case LexicalRecoveryProfileKind::Delimiter:
  case LexicalRecoveryProfileKind::OperatorLike:
    return true;
  }
  return true;
}

template <typename Context>
[[nodiscard]] TriviaGapProfile
current_local_skip_trivia_gap_profile(const Context &ctx) noexcept {
  if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
    const char *const skipped = ctx.skip_without_builder(ctx.cursor());
    return make_trivia_gap_profile(ctx, ctx.cursor(), skipped);
  } else {
    return {};
  }
}

[[nodiscard]] constexpr bool
allows_nearby_delete_scan(const TerminalRecoveryFacts &facts,
                          const LexicalRecoveryProfile &profile) noexcept {
  if (!facts.triviaGap.hasHiddenGap()) {
    return true;
  }
  if (!facts.triviaGap.isCompact()) {
    return false;
  }
  switch (profile.kind) {
  case LexicalRecoveryProfileKind::OperatorLike:
  case LexicalRecoveryProfileKind::OpaqueSymbol:
    return true;
  case LexicalRecoveryProfileKind::WordLikeLiteral:
  case LexicalRecoveryProfileKind::WordLikeFreeForm:
  case LexicalRecoveryProfileKind::Separator:
  case LexicalRecoveryProfileKind::Delimiter:
    return false;
  }
  return false;
}

template <typename Context>
[[nodiscard]] constexpr bool
operator_like_delete_scan_crosses_identifier_gap(
    const Context &ctx, const char *visibleCursor,
    const LexicalRecoveryProfile &profile) noexcept {
  if (visibleCursor == nullptr || visibleCursor >= ctx.end) {
    return false;
  }
  if (profile.kind != LexicalRecoveryProfileKind::OperatorLike &&
      profile.kind != LexicalRecoveryProfileKind::OpaqueSymbol) {
    return false;
  }
  return is_identifier_like_codepoint(
      static_cast<unsigned char>(*visibleCursor));
}

template <typename Context>
[[nodiscard]] constexpr bool
allows_terminal_entry_probe(const Context &ctx,
                            const LexicalRecoveryProfile &profile) noexcept {
  if (profile.kind != LexicalRecoveryProfileKind::Separator &&
      profile.kind != LexicalRecoveryProfileKind::Delimiter) {
    return false;
  }
  if (cursor_starts_visible_source(ctx)) {
    return true;
  }
  const auto localGap = current_local_skip_trivia_gap_profile(ctx);
  return localGap.visibleSourceAfterLocalSkip && localGap.isCompact();
}

template <typename Context>
[[nodiscard]] constexpr bool
cursor_starts_visible_source(const Context &ctx) noexcept {
  if (ctx.cursor() >= ctx.end) {
    return false;
  }
  if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
    return ctx.skip_without_builder(ctx.cursor()) == ctx.cursor();
  }
  return true;
}

template <typename Context>
[[nodiscard]] constexpr bool
cursor_starts_identifier_like_visible_source(const Context &ctx) noexcept {
  return cursor_starts_visible_source(ctx) && ctx.cursor() < ctx.end &&
         is_identifier_like_codepoint(
             static_cast<unsigned char>(*ctx.cursor()));
}

template <typename Context>
[[nodiscard]] constexpr bool
cursor_starts_structured_visible_source(const Context &ctx) noexcept {
  if (!cursor_starts_visible_source(ctx) || ctx.cursor() >= ctx.end) {
    return false;
  }
  const auto codepoint = static_cast<unsigned char>(*ctx.cursor());
  return is_identifier_like_codepoint(codepoint) ||
         (codepoint >= static_cast<unsigned char>('0') &&
          codepoint <= static_cast<unsigned char>('9')) ||
         codepoint == '"' || codepoint == '\'' || codepoint == '(' ||
         codepoint == '[' || codepoint == '{';
}

template <typename Context>
[[nodiscard]] constexpr bool
cursor_reaches_visible_source_after_local_skip(const Context &ctx) noexcept {
  if (ctx.cursor() >= ctx.end) {
    return false;
  }
  if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
    const char *const skipped = ctx.skip_without_builder(ctx.cursor());
    return skipped > ctx.cursor() && skipped < ctx.end;
  }
  return false;
}

template <typename Context>
[[nodiscard]] TriviaGapProfile
make_trivia_gap_profile(const Context &ctx, const char *gapBegin,
                        const char *gapEnd) noexcept {
  TriviaGapProfile profile{
      .visibleSourceAfterLocalSkip =
          detail::cursor_reaches_visible_source_after_local_skip(ctx),
  };
  if (gapBegin == nullptr || gapEnd == nullptr || gapEnd <= gapBegin) {
    return profile;
  }
  profile.hiddenCodepointSpan = static_cast<std::uint32_t>(gapEnd - gapBegin);
  return profile;
}

template <typename Context>
[[nodiscard]] bool
allows_compact_local_gap_terminal_recovery(const Context &ctx) noexcept {
  if (!cursor_starts_visible_source(ctx) &&
      cursor_reaches_visible_source_after_local_skip(ctx)) {
    return current_local_skip_trivia_gap_profile(ctx).isCompact();
  }
  return true;
}

[[nodiscard]] constexpr std::uint32_t literal_fuzzy_primary_rank_limit(
    const LexicalRecoveryProfile &profile) noexcept {
  return std::min<std::uint32_t>(5u, saturating_add(profile.textLength, 3u));
}

[[nodiscard]] constexpr bool allows_entry_probe_fuzzy_candidate(
    const LiteralFuzzyCandidate &candidate) noexcept {
  return candidate.distance == 1u && candidate.operationCount == 1u;
}

[[nodiscard]] constexpr bool
allows_fuzzy_replace_after_prior_edits(
    const LexicalRecoveryProfile &profile) noexcept {
  return profile.kind == LexicalRecoveryProfileKind::OperatorLike;
}

[[nodiscard]] constexpr bool allows_literal_fuzzy_candidate(
    const LiteralFuzzyCandidate &candidate,
    const LexicalRecoveryProfile &profile, const TerminalRecoveryFacts &facts,
    bool cursorStartsIdentifierLikeVisibleSource) noexcept {
  switch (profile.kind) {
  case LexicalRecoveryProfileKind::WordLikeLiteral:
    if (facts.triviaGap.hasHiddenGap() &&
        !cursorStartsIdentifierLikeVisibleSource) {
      return false;
    }
    if (facts.triviaGap.hasHiddenGap() && candidate.substitutionCount != 0u) {
      return false;
    }
    return candidate.cost.primaryRankCost <=
           literal_fuzzy_primary_rank_limit(profile);
  case LexicalRecoveryProfileKind::OperatorLike:
    return candidate.distance == 1u && candidate.operationCount == 1u &&
           candidate.substitutionCount == 0u;
  case LexicalRecoveryProfileKind::WordLikeFreeForm:
  case LexicalRecoveryProfileKind::Separator:
  case LexicalRecoveryProfileKind::Delimiter:
  case LexicalRecoveryProfileKind::OpaqueSymbol:
    return false;
  }
  return false;
}

template <EditableParseModeContext Context>
[[nodiscard]] const char *literal_fuzzy_input_end(
    const Context &ctx, const char *cursor, const char *limit,
    std::uint32_t textLength) noexcept {
  constexpr std::size_t kExtraWindow = 4u;
  if (cursor == nullptr || cursor >= limit) {
    return cursor;
  }

  const auto span = static_cast<std::size_t>(limit - cursor);
  const char *const boundedLimit =
      cursor + std::min(span, static_cast<std::size_t>(textLength) +
                                  kExtraWindow);
  if constexpr (requires { ctx.skip_without_builder(cursor); }) {
    const char *it = cursor;
    while (it < boundedLimit) {
      if (ctx.skip_without_builder(it) > it) {
        return it;
      }
      ++it;
    }
    return boundedLimit;
  } else {
    return boundedLimit;
  }
}

template <EditableParseModeContext Context>
[[nodiscard]] std::string_view literal_fuzzy_input_view(
    const Context &ctx, const char *cursor, const char *limit,
    std::uint32_t textLength) noexcept {
  const auto *viewEnd = literal_fuzzy_input_end(ctx, cursor, limit, textLength);
  return {cursor, static_cast<std::size_t>(viewEnd - cursor)};
}

template <EditableParseModeContext Context>
[[nodiscard]] constexpr const char *
literal_fuzzy_input_limit(const Context &ctx) noexcept {
  if constexpr (RecoveryParseModeContext<Context>) {
    return ctx.end;
  } else {
    return ctx.anchor;
  }
}

template <EditableParseModeContext Context>
[[nodiscard]] LiteralFuzzyCandidates collect_literal_replace_candidates(
    Context &ctx, const char *cursorStart, std::string_view literalValue,
    bool caseSensitive, const LexicalRecoveryProfile &profile,
    TerminalRecoveryFacts facts = {}) noexcept {
  LiteralFuzzyCandidates filteredCandidates;
  if (!profile.allowsReplace()) {
    return filteredCandidates;
  }

  const auto candidates = find_literal_fuzzy_candidates(
      literalValue,
      literal_fuzzy_input_view(ctx, cursorStart, literal_fuzzy_input_limit(ctx),
                               profile.textLength),
      caseSensitive);
  const bool cursorStartsIdentifierLikeVisibleSource =
      cursor_starts_identifier_like_visible_source(ctx);
  filteredCandidates.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    if (!allows_literal_fuzzy_candidate(
            candidate, profile, facts,
            cursorStartsIdentifierLikeVisibleSource)) {
      continue;
    }
    filteredCandidates.push_back(candidate);
  }
  return filteredCandidates;
}

template <EditableParseModeContext Context>
[[nodiscard]] constexpr bool
allows_extended_terminal_delete_scan_match(const Context &ctx,
                                           const char *cursorStart,
                                           const char *scanStart) noexcept {
  if constexpr (requires { ctx.maxConsecutiveCodepointDeletes; }) {
    const auto deletedCount = static_cast<std::size_t>(scanStart - cursorStart);
    const auto localLimit =
        static_cast<std::size_t>(ctx.maxConsecutiveCodepointDeletes) + 1u;
    if (deletedCount <= localLimit) {
      return true;
    }

    if constexpr (requires { ctx.allowExtendedDeleteScan; }) {
      if (!ctx.allowExtendedDeleteScan) {
        return false;
      }
    }

    if constexpr (requires { ctx.currentEditCount(); }) {
      if (ctx.currentEditCount() > 0) {
        return true;
      }
    }

    if constexpr (requires { ctx.maxEditCost; }) {
      if (ctx.maxEditCost > ParseOptions{}.maxRecoveryEditCost) {
        return true;
      }
    }

    return false;
  }

  return true;
}

template <EditableParseModeContext Context, typename MatchFn>
[[nodiscard]] inline bool
probe_nearby_delete_scan_match(Context &ctx, MatchFn &&matchFn,
                               TerminalRecoveryFacts facts = {},
                               LexicalRecoveryProfile profile = {}) {
  if (!allows_nearby_delete_scan(facts, profile)) {
    return false;
  }
  if constexpr (requires { ctx.canDelete(); }) {
    if (!ctx.canDelete()) {
      return false;
    }
  }

  const char *const cursorStart = ctx.cursor();
  const char *scanCursor = cursorStart;
  for (; scanCursor < ctx.end; ++scanCursor) {
    if constexpr (requires { ctx.skip_without_builder(scanCursor); }) {
      const char *const skipped = ctx.skip_without_builder(scanCursor);
      if (skipped > scanCursor) {
        if (std::forward<MatchFn>(matchFn)(skipped) != nullptr) {
          return true;
        }
        if (operator_like_delete_scan_crosses_identifier_gap(
                ctx, skipped, profile)) {
          return false;
        }
        break;
      }
    }
    if (facts.previousElementIsTerminalish && scanCursor > cursorStart) {
      continue;
    }
    if (!allows_extended_terminal_delete_scan_match(ctx, cursorStart,
                                                    scanCursor)) {
      break;
    }
    if (std::forward<MatchFn>(matchFn)(scanCursor) != nullptr) {
      return true;
    }
  }
  if (!detail::allows_extended_delete_scan(ctx)) {
    return false;
  }
  for (; scanCursor < ctx.end; ++scanCursor) {
    if (std::forward<MatchFn>(matchFn)(scanCursor) != nullptr) {
      return true;
    }
  }
  return false;
}

template <EditableParseModeContext Context, typename ApplyFn>
[[nodiscard]] TerminalRecoveryCandidate evaluate_terminal_recovery_candidate(
    Context &ctx, const char *cursorStart, TerminalRecoveryChoiceKind kind,
    std::uint32_t distance, std::uint32_t substitutionCount,
    std::uint32_t operationCount, ApplyFn &&applyFn,
    TerminalAnchorQuality anchorQuality = TerminalAnchorQuality::DirectVisible,
    std::uint32_t extraPrimaryRankCost = 0u,
    std::uint32_t extraSecondaryRankCost = 0u) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (std::forward<ApplyFn>(applyFn)()) {
    const auto budgetCost = ctx.editCostDelta(checkpoint);
    candidate = {
        .kind = kind,
        .anchorQuality = anchorQuality,
        .cost = make_recovery_cost(
            budgetCost, saturating_add(distance, extraPrimaryRankCost),
            saturating_add(budgetCost, extraSecondaryRankCost)),
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
evaluate_insert_synthetic_terminal_candidate(
    Context &ctx, const char *cursorStart, const Element *element,
    TerminalAnchorQuality anchorQuality = TerminalAnchorQuality::DirectVisible,
    std::uint32_t extraPrimaryRankCost = 0u) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Insert, 1u, 0u, 1u,
      [&ctx, element]() {
        return apply_insert_synthetic_recovery_edit(ctx, element);
      },
      anchorQuality, extraPrimaryRankCost);
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_insert_synthetic_gap_terminal_candidate(
    Context &ctx, const char *cursorStart, const char *position,
    const Element *element, std::uint32_t extraPrimaryRankCost = 0u,
    std::uint32_t extraSecondaryRankCost = 0u) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Insert, 1u, 0u, 1u,
      [&ctx, position, element]() {
        return apply_insert_synthetic_gap_and_match_recovery_edit(ctx, position,
                                                                  element);
      },
      TerminalAnchorQuality::DirectVisible, extraPrimaryRankCost,
      extraSecondaryRankCost);
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_replace_leaf_terminal_candidate(
    Context &ctx, const char *cursorStart, const char *endPtr,
    const Element *element, RecoveryCost cost, std::uint32_t distance,
    std::uint32_t substitutionCount, std::uint32_t operationCount,
    TerminalAnchorQuality anchorQuality =
        TerminalAnchorQuality::DirectVisible) {
  auto candidate = evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Replace, distance,
      substitutionCount, operationCount,
      [&ctx, endPtr, element, budgetCost = cost.budgetCost]() {
        return apply_replace_leaf_recovery_edit(ctx, endPtr, element,
                                                budgetCost);
      },
      anchorQuality);
  if (candidate.kind == TerminalRecoveryChoiceKind::Replace) {
    candidate.cost = cost;
  }
  return candidate;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate evaluate_delete_scan_terminal_candidate(
    Context &ctx, const char *cursorStart, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn, TerminalRecoveryFacts facts = {},
    LexicalRecoveryProfile profile = {});

template <EditableParseModeContext Context, typename Element, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate complete_terminal_recovery_choice(
    Context &ctx, const char *cursorStart, const Element *element,
    TerminalRecoveryFacts facts, LexicalRecoveryProfile lexicalProfile,
    bool allowInsert, TerminalRecoveryCandidate bestChoice, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn) {
  auto considerChoice = [&bestChoice](const TerminalRecoveryCandidate &choice) {
    if (is_better_normalized_recovery_order_key(
            terminal_recovery_order_key(choice),
            terminal_recovery_order_key(bestChoice),
            RecoveryOrderProfile::Terminal)) {
      bestChoice = choice;
    }
  };

  const bool cursorStartsVisibleSource =
      detail::cursor_starts_visible_source(ctx);
  const auto visibleSourceEditCount = [&]() constexpr noexcept {
    if constexpr (requires { ctx.currentWindowEditCount(); }) {
      return ctx.currentWindowEditCount();
    } else if constexpr (requires { ctx.currentEditCount(); }) {
      return ctx.currentEditCount();
    } else {
      return 0u;
    }
  }();
  const bool visibleSourceAvailableForScopedContinuation =
      cursorStartsVisibleSource ||
      facts.triviaGap.visibleSourceAfterLocalSkip ||
      detail::cursor_reaches_visible_source_after_local_skip(ctx);
  const bool scopedLeadingContinuationInsertAllowed = [&]() constexpr noexcept {
    if constexpr (requires {
                    ctx.allowsScopedLeadingTerminalInsertRecovery();
                    ctx.allowDelete;
                  }) {
      return ctx.allowsScopedLeadingTerminalInsertRecovery() &&
             !ctx.allowDelete && visibleSourceAvailableForScopedContinuation;
    } else {
      return false;
    }
  }();
  const bool structuredVisibleContinuationInsertAllowed =
      facts.allowStructuredVisibleContinuationInsert &&
      cursorStartsVisibleSource &&
      detail::cursor_starts_structured_visible_source(ctx);
  const bool structuredVisibleContinuationBlocksOpaqueDeleteScan =
      structuredVisibleContinuationInsertAllowed &&
      lexicalProfile.kind == LexicalRecoveryProfileKind::OpaqueSymbol;
  const bool structuredVisibleContinuationBlocksWordLikeDeleteScan =
      structuredVisibleContinuationInsertAllowed &&
      visibleSourceEditCount != 0u &&
      lexicalProfile.kind == LexicalRecoveryProfileKind::WordLikeLiteral &&
      lexicalProfile.textLength > 1u;
  const bool preferStructuredVisibleInsertOverDeleteScan =
      (structuredVisibleContinuationInsertAllowed &&
      (lexicalProfile.kind == LexicalRecoveryProfileKind::Separator ||
       lexicalProfile.kind == LexicalRecoveryProfileKind::Delimiter)) ||
      structuredVisibleContinuationBlocksWordLikeDeleteScan ||
      structuredVisibleContinuationBlocksOpaqueDeleteScan;

  TerminalRecoveryCandidate deleteScanChoice;
  if (!preferStructuredVisibleInsertOverDeleteScan &&
      probe_nearby_delete_scan_match(ctx, matchFn, facts, lexicalProfile)) {
    deleteScanChoice = evaluate_delete_scan_terminal_candidate(
        ctx, cursorStart, std::forward<MatchFn>(matchFn),
        std::forward<OnMatchFn>(onMatchFn), facts, lexicalProfile);
    considerChoice(deleteScanChoice);
  }

  // Structural separators and delimiters still compete as local inserts when
  // the visible source starts a structured continuation, but not when the
  // visible source is only stray punctuation that delete-scan can absorb.
  const bool deleteScanBlocksVisibleInsert =
      deleteScanChoice.kind == TerminalRecoveryChoiceKind::DeleteScan &&
      cursorStartsVisibleSource &&
      !cursor_starts_structured_visible_source(ctx);
  const bool allowInsertCandidate =
      allowInsert &&
      (!cursorStartsVisibleSource ||
       ((visibleSourceEditCount == 0u ||
         scopedLeadingContinuationInsertAllowed ||
         structuredVisibleContinuationInsertAllowed) &&
        !deleteScanBlocksVisibleInsert));
  if (allowInsertCandidate) {
    const auto insertChoice = evaluate_insert_synthetic_terminal_candidate(
        ctx, cursorStart, element, terminal_anchor_quality(facts.triviaGap),
        terminal_anchor_rank_penalty(facts.triviaGap));
    considerChoice(insertChoice);
  }

  return bestChoice;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] bool recover_by_terminal_delete_scan(
    Context &ctx, MatchFn &&matchFn, OnMatchFn &&onMatchFn,
    TerminalRecoveryFacts facts = {}, LexicalRecoveryProfile profile = {}) {
  if (!allows_nearby_delete_scan(facts, profile)) {
    return false;
  }
  if constexpr (requires { ctx.canDelete(); }) {
    if (!ctx.canDelete()) {
      return false;
    }
  }

  const auto recoveryCheckpoint = ctx.mark();
  const char *const cursorStart = ctx.cursor();
  bool previousSkipAfterDelete = false;
  if constexpr (requires { ctx.skipAfterDelete; }) {
    previousSkipAfterDelete = ctx.skipAfterDelete;
    ctx.skipAfterDelete = false;
  }
  detail::ExtendedDeleteScanBudgetScope overflowBudgetScope{ctx};
  const auto restore_skip_after_delete = [&]() {
    if constexpr (requires { ctx.skipAfterDelete; }) {
      ctx.skipAfterDelete = previousSkipAfterDelete;
    }
  };
  const auto try_delete_scan_pass = [&](bool overflowBudget) {
    while (ctx.deleteOneCodepoint()) {
      const char *const scanCursor = ctx.cursor();
      auto apply_matched_end = [&](const char *matchedEnd) {
        restore_skip_after_delete();
        if (overflowBudget) {
          overflowBudgetScope.commitOverflowEdits();
        }
        std::forward<OnMatchFn>(onMatchFn)(matchedEnd);
        return true;
      };
      if constexpr (requires { ctx.skip_without_builder(scanCursor); }) {
        const char *const skipped = ctx.skip_without_builder(scanCursor);
        if (skipped > scanCursor) {
          if (const char *const matchedEnd =
                  std::forward<MatchFn>(matchFn)(skipped);
              matchedEnd != nullptr) {
            if constexpr (requires { ctx.skip(); }) {
              ctx.skip();
            }
            return apply_matched_end(matchedEnd);
          }
          if (operator_like_delete_scan_crosses_identifier_gap(
                  ctx, skipped, profile)) {
            break;
          }
          if constexpr (requires {
                          ctx.extendLastDeleteThroughHiddenTrivia();
                        }) {
            if (ctx.extendLastDeleteThroughHiddenTrivia()) {
              continue;
            }
          }
          break;
        }
      }
      if (facts.previousElementIsTerminalish && scanCursor > cursorStart) {
        continue;
      }
      if (const char *const matchedEnd =
              std::forward<MatchFn>(matchFn)(scanCursor);
          matchedEnd != nullptr) {
        return apply_matched_end(matchedEnd);
      }
    }
    return false;
  };
  if (try_delete_scan_pass(false)) {
    return true;
  }
  if (!overflowBudgetScope.tryEnable()) {
    restore_skip_after_delete();
    ctx.rewind(recoveryCheckpoint);
    return false;
  }
  if (try_delete_scan_pass(true)) {
    return true;
  }
  restore_skip_after_delete();
  ctx.rewind(recoveryCheckpoint);
  return false;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate evaluate_delete_scan_terminal_candidate(
    Context &ctx, const char *cursorStart, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn, TerminalRecoveryFacts facts,
    LexicalRecoveryProfile profile) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (recover_by_terminal_delete_scan(ctx, std::forward<MatchFn>(matchFn),
                                      std::forward<OnMatchFn>(onMatchFn), facts,
                                      profile)) {
    const auto cost = ctx.editCostDelta(checkpoint);
    const auto deletedCost = default_edit_cost(ParseDiagnosticKind::Deleted);
    const auto distance = deletedCost == 0u ? 0u : cost / deletedCost;
    candidate.kind = TerminalRecoveryChoiceKind::DeleteScan;
    candidate.anchorQuality = terminal_anchor_quality(facts.triviaGap);
    candidate.cost = make_recovery_cost(cost, distance, cost);
    candidate.distance = distance;
    candidate.consumed = static_cast<std::size_t>(ctx.cursor() - cursorStart);
    candidate.substitutionCount = 0u;
    candidate.operationCount = deletedCost == 0u ? 0u : cost / deletedCost;
  }
  ctx.rewind(checkpoint);
  return candidate;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] bool apply_delete_scan_terminal_candidate(
    Context &ctx, MatchFn &&matchFn, OnMatchFn &&onMatchFn,
    TerminalRecoveryFacts facts = {}, LexicalRecoveryProfile profile = {}) {
  return recover_by_terminal_delete_scan(ctx, std::forward<MatchFn>(matchFn),
                                         std::forward<OnMatchFn>(onMatchFn),
                                         facts, profile);
}

template <EditableParseModeContext Context, typename Element,
          typename ApplyMatchedInsertFn, typename ApplyReplaceFn,
          typename OnInsertFn, typename ApplyDeleteScanFn>
[[nodiscard]] bool apply_terminal_recovery_choice(
    Context &ctx, const TerminalRecoveryCandidate &choice,
    const Element *element, ApplyMatchedInsertFn &&applyMatchedInsert,
    ApplyReplaceFn &&applyReplace, OnInsertFn &&onInsert,
    ApplyDeleteScanFn &&applyDeleteScan) {
  switch (choice.kind) {
  case TerminalRecoveryChoiceKind::Insert:
    if (choice.consumed > 0u) {
      return std::forward<ApplyMatchedInsertFn>(applyMatchedInsert)();
    }
    if (!apply_insert_synthetic_recovery_edit(ctx, element)) {
      return false;
    }
    ctx.leaf(ctx.cursor(), element, false, true);
    std::forward<OnInsertFn>(onInsert)();
    return true;
  case TerminalRecoveryChoiceKind::Replace:
    return std::forward<ApplyReplaceFn>(applyReplace)();
  case TerminalRecoveryChoiceKind::DeleteScan:
    return std::forward<ApplyDeleteScanFn>(applyDeleteScan)();
  case TerminalRecoveryChoiceKind::None:
    return false;
  }
  return false;
}

} // namespace pegium::parser::detail
