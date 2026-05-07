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
#include <pegium/core/parser/TerminalShape.hpp>

namespace pegium::parser::detail {

struct TriviaGapProfile {
  std::uint32_t hiddenCodepointSpan = 0u;
  bool visibleSourceAfterLocalSkip = false;

  [[nodiscard]] constexpr bool hasHiddenGap() const noexcept {
    return hiddenCodepointSpan > 0u;
  }
};

[[nodiscard]] constexpr TerminalShape
classify_literal_recovery_profile(std::string_view value) noexcept {
  return make_terminal_shape_from_literal(
      value, /*startsLikeWord=*/false, /*endsLikeWord=*/false);
}

template <typename Element>
[[nodiscard]] constexpr TerminalShape
infer_terminal_rule_recovery_profile(const Element &element) noexcept {
  if constexpr (requires {
                  {
                    element.getValue()
                  } -> std::convertible_to<std::string_view>;
                }) {
    return classify_literal_recovery_profile(element.getValue());
  } else {
    (void)element;
    return {};
  }
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
  /// Direct strict terminal matches still win before this fact is read.
  /// When true, local edit recovery for this terminal is suppressed because
  /// the surrounding sequence has established that another sibling owns the
  /// current cursor.
  bool localRecoveryBlocked = false;
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
                            const TerminalShape &shape) noexcept {
  return shape.hasCanonicalText || !cursor_starts_visible_source(ctx);
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

template <typename Context>
[[nodiscard]] TerminalRecoveryFacts effective_terminal_recovery_facts(
    const Context &ctx, TerminalRecoveryFacts facts) noexcept {
  if (!facts.triviaGap.hasHiddenGap() &&
      !facts.triviaGap.visibleSourceAfterLocalSkip) {
    facts.triviaGap = current_local_skip_trivia_gap_profile(ctx);
  }
  return facts;
}

[[nodiscard]] constexpr bool
allows_nearby_delete_scan(const TerminalRecoveryFacts &facts,
                          const TerminalShape &shape) noexcept {
  if (!facts.triviaGap.hasHiddenGap()) {
    return true;
  }
  return shape.hasCanonicalText && !shape.allowsInsert();
}

template <typename Context>
[[nodiscard]] constexpr bool
allows_terminal_entry_probe(const Context &ctx,
                            const TerminalShape &shape) noexcept {
  if (!shape.allowsInsert()) {
    return false;
  }
  if (cursor_starts_visible_source(ctx)) {
    return true;
  }
  const auto localGap = current_local_skip_trivia_gap_profile(ctx);
  return localGap.visibleSourceAfterLocalSkip && localGap.hasHiddenGap();
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
cursor_starts_structured_visible_source(const Context &ctx) noexcept {
  // True iff the cursor is on a non-trivia codepoint that could initiate
  // an identifier or word-like token of the target language. Gates fuzzy
  // substitutions on long keywords and disambiguates insert-vs-delete-scan
  // candidate selection: a fuzzy keyword swap or a synthetic insert is
  // only sensible on a word-like codepoint, not on bare punctuation that
  // is more likely to be stray noise.
  if (!cursor_starts_visible_source(ctx)) {
    return false;
  }
  // Bounds-check the multi-byte UTF-8 read: `decode_utf8_codepoint`
  // reads up to 4 bytes from `cursor`, and a lead byte sitting at
  // `end - 1` would have it read past the buffer. Pegium's text
  // buffers carry a `\0` sentinel inside the size, but we can't
  // depend on the runtime arena layout (ASan flagged this on a
  // truncated multi-byte tail in adversarial fuzz input). When the
  // codepoint extends past `end`, treat the cursor as not on a
  // structured visible-source codepoint — same conservative answer
  // as the lead-byte pre-check elsewhere in the recovery path.
  const auto *const cursor = ctx.cursor();
  const auto length = utils::utf8_codepoint_length(*cursor);
  if (length == 0 ||
      length > static_cast<std::size_t>(ctx.end - cursor)) {
    return false;
  }
  return is_identifier_like_codepoint(utils::decode_utf8_codepoint(cursor));
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
    return current_local_skip_trivia_gap_profile(ctx).hasHiddenGap();
  }
  return true;
}

[[nodiscard]] constexpr bool allows_entry_probe_fuzzy_candidate(
    const LiteralFuzzyCandidate &candidate) noexcept {
  return candidate.distance == 1u && candidate.operationCount == 1u;
}

[[nodiscard]] constexpr std::uint32_t literal_fuzzy_primary_rank_limit(
    const TerminalShape &shape) noexcept {
  return std::min<std::uint32_t>(5u, shape.canonicalTextLength + 3u);
}

[[nodiscard]] constexpr bool
allows_fuzzy_replace_after_prior_edits(const TerminalShape &shape) noexcept {
  return shape.allowsReplace();
}

[[nodiscard]] inline bool short_literal_fuzzy_candidate_possible(
    std::string_view literalValue, std::string_view input,
    bool caseSensitive) noexcept {
  if (literalValue.empty() || input.empty()) {
    return false;
  }
  // Fast pre-filter for ≤2-codepoint literals: gate the Levenshtein DP on
  // whether the inspected window of the input shares at least one byte
  // with the literal under the same case-sensitivity contract used by
  // the full matcher. Byte-comparing without honouring `caseSensitive`
  // would miss legitimate fuzzy candidates (`'A'` in input vs `'a'` in
  // literal). ASCII tolower covers the typical short-keyword shape; the
  // matcher itself does the full Unicode work on candidates we let
  // through.
  const auto fold = [caseSensitive](char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    if (caseSensitive || byte >= 0x80u) {
      return byte;
    }
    return static_cast<unsigned char>(byte | 0x20u);
  };
  const auto inspectedInputLength =
      std::min(input.size(), literalValue.size() + 1u);
  for (std::size_t inputIndex = 0u; inputIndex < inspectedInputLength;
       ++inputIndex) {
    const auto inputByte = fold(input[inputIndex]);
    for (const auto literalChar : literalValue) {
      if (inputByte == fold(literalChar)) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] constexpr bool allows_literal_fuzzy_candidate(
    const LiteralFuzzyCandidate &candidate, const TerminalShape &shape,
    const TerminalRecoveryFacts &facts,
    bool cursorStartsStructuredVisibleSource) noexcept {
  TerminalLegalityFacts legality;
  legality.budgetAllowsReplace =
      candidate.cost.primaryRankCost <= literal_fuzzy_primary_rank_limit(shape);
  if (!is_terminal_replace_legal(shape, legality)) {
    return false;
  }
  if (shape.canonicalTextLength <= 2u) {
    return candidate.distance == 1u && candidate.operationCount == 1u &&
           candidate.substitutionCount == 0u;
  }
  if (!cursorStartsStructuredVisibleSource) {
    return false;
  }
  if (facts.triviaGap.hasHiddenGap() && candidate.substitutionCount != 0u) {
    return false;
  }
  // For longer keywords (5+ codepoints), require the candidate to consume
  // at least half of the literal length. Without this, a desperate
  // 2-character match like `re` -> `environment` (cost ≤ rank limit but
  // 9 inserts) can be accepted on par with a tighter 1-edit candidate at
  // a different position, and the grammar-order tiebreaker can route
  // around the real typo. The half-length floor leaves all single-edit
  // truncation / insertion / substitution cases intact (those consume
  // `len` or `len±1` codepoints).
  if (shape.canonicalTextLength >= 5u) {
    const std::uint32_t halfLength = shape.canonicalTextLength / 2u;
    if (candidate.consumed < halfLength) {
      return false;
    }
  }
  return true;
}

template <EditableParseModeContext Context>
[[nodiscard]] const char *literal_fuzzy_input_end(
    const Context &ctx, const char *cursor, const char *limit,
    std::uint32_t textLength) noexcept {
  // Route the fuzzy window through the canonical formula
  // `maxLookahead = canonicalTextLength + affordableDeleteSpan`. The
  // affordable-delete-span input is fixed so a future migration to a
  // real budget-derived span only changes the input, not the formula.
  constexpr std::uint32_t kAffordableDeleteSpan = 4U;
  if (cursor == nullptr || cursor >= limit) {
    return cursor;
  }

  TerminalShape windowShape;
  windowShape.hasCanonicalText = textLength > 0U;
  windowShape.canonicalTextLength = textLength;
  const auto maxLookahead =
      compute_terminal_max_lookahead(windowShape, kAffordableDeleteSpan);
  const auto span = static_cast<std::size_t>(limit - cursor);
  const char *const boundedLimit =
      cursor +
      std::min(span, static_cast<std::size_t>(maxLookahead));
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

template <typename Context>
[[nodiscard]] LiteralFuzzyCandidatesCache *
literal_fuzzy_candidates_cache_for(Context &ctx) noexcept {
  if constexpr (requires {
                  { ctx.literalFuzzyCandidatesCache } ->
                      std::same_as<LiteralFuzzyCandidatesCache &>;
                }) {
    return &ctx.literalFuzzyCandidatesCache;
  } else {
    (void)ctx;
    return nullptr;
  }
}

template <EditableParseModeContext Context>
[[nodiscard]] LiteralFuzzyCandidates collect_literal_replace_candidates(
    Context &ctx, const char *cursorStart, std::string_view literalValue,
    bool caseSensitive, const TerminalShape &shape,
    TerminalRecoveryFacts facts = {}) noexcept {
  LiteralFuzzyCandidates filteredCandidates;
  if (!shape.allowsReplace()) {
    return filteredCandidates;
  }

  const auto inputView = literal_fuzzy_input_view(
      ctx, cursorStart, literal_fuzzy_input_limit(ctx),
      shape.canonicalTextLength);
  if (shape.canonicalTextLength <= 2u &&
      !short_literal_fuzzy_candidate_possible(literalValue, inputView,
                                              caseSensitive)) {
    return filteredCandidates;
  }

  // Recovery contexts always carry a cache; the `_view` overload returns a
  // non-owning span pointing into the cache slot, so the dominant cache-hit
  // path skips the per-call `LiteralFuzzyCandidates` copy that the by-value
  // overload would otherwise force.
  auto *const cache = literal_fuzzy_candidates_cache_for(ctx);
  const std::span<const LiteralFuzzyCandidate> candidates =
      cache != nullptr
          ? find_literal_fuzzy_candidates_view(literalValue, inputView,
                                               caseSensitive, *cache)
          : std::span<const LiteralFuzzyCandidate>{};
  const bool cursorStartsStructuredVisibleSource =
      cursor_starts_structured_visible_source(ctx);
  filteredCandidates.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    if (!allows_literal_fuzzy_candidate(candidate, shape, facts,
                                        cursorStartsStructuredVisibleSource)) {
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

    if (!position_starts_structured_visible_source(ctx, scanStart)) {
      return true;
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
                               TerminalShape shape = {}) {
  if (!allows_nearby_delete_scan(facts, shape)) {
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
  return false;
}

template <EditableParseModeContext Context, typename ApplyFn>
[[nodiscard]] TerminalRecoveryCandidate evaluate_terminal_recovery_candidate(
    Context &ctx, const char *cursorStart, TerminalRecoveryChoiceKind kind,
    std::uint32_t distance, std::uint32_t substitutionCount,
    std::uint32_t operationCount, ApplyFn &&applyFn,
    std::uint32_t extraPrimaryRankCost = 0u,
    std::uint32_t extraSecondaryRankCost = 0u) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (std::forward<ApplyFn>(applyFn)()) {
    const auto budgetCost = ctx.editCostDelta(checkpoint);
    candidate = {
        .kind = kind,
        .cost = make_recovery_cost(budgetCost,
                                   distance + extraPrimaryRankCost,
                                   budgetCost + extraSecondaryRankCost),
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
    std::uint32_t extraPrimaryRankCost = 0u) {
  return evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Insert, 1u, 0u, 1u,
      [&ctx, element]() { return ctx.insertSynthetic(element); },
      extraPrimaryRankCost);
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
      extraPrimaryRankCost, extraSecondaryRankCost);
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] TerminalRecoveryCandidate
evaluate_replace_leaf_terminal_candidate(
    Context &ctx, const char *cursorStart, const char *endPtr,
    const Element *element, RecoveryCost cost, std::uint32_t distance,
    std::uint32_t substitutionCount, std::uint32_t operationCount) {
  auto candidate = evaluate_terminal_recovery_candidate(
      ctx, cursorStart, TerminalRecoveryChoiceKind::Replace, distance,
      substitutionCount, operationCount,
      [&ctx, endPtr, element, budgetCost = cost.budgetCost]() {
        return ctx.replaceLeaf(endPtr, element, budgetCost);
      });
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
    TerminalShape shape = {});

template <EditableParseModeContext Context, typename Element, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate complete_terminal_recovery_choice(
    Context &ctx, const char *cursorStart, const Element *element,
    TerminalRecoveryFacts facts, TerminalShape shape,
    bool allowInsert, TerminalRecoveryCandidate bestChoice, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn) {
  auto considerChoice = [&bestChoice](const TerminalRecoveryCandidate &choice) {
    if (is_better_recovery_key(terminal_recovery_key(choice),
                                terminal_recovery_key(bestChoice))) {
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
  const bool preferStructuredVisibleInsertOverDeleteScan =
      structuredVisibleContinuationInsertAllowed && allowInsert;

  TerminalRecoveryCandidate deleteScanChoice;
  if (!preferStructuredVisibleInsertOverDeleteScan &&
      probe_nearby_delete_scan_match(ctx, matchFn, facts, shape)) {
    deleteScanChoice = evaluate_delete_scan_terminal_candidate(
        ctx, cursorStart, std::forward<MatchFn>(matchFn),
        std::forward<OnMatchFn>(onMatchFn), facts, shape);
    considerChoice(deleteScanChoice);
  }

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
    const auto insertChoice =
        evaluate_insert_synthetic_terminal_candidate(ctx, cursorStart, element);
    considerChoice(insertChoice);
  }

  return bestChoice;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] bool recover_by_terminal_delete_scan(
    Context &ctx, MatchFn &&matchFn, OnMatchFn &&onMatchFn,
    TerminalRecoveryFacts facts = {}, TerminalShape shape = {}) {
  if (!allows_nearby_delete_scan(facts, shape)) {
    return false;
  }
  bool budgetAllowsDeleteScan = true;
  if constexpr (requires { ctx.canDelete(); }) {
    budgetAllowsDeleteScan = ctx.canDelete();
    if (!budgetAllowsDeleteScan) {
      return false;
    }
  }

  const char *const cursorStart = ctx.cursor();
  // Wrap the `skipAfterDelete = false` flip in an RAII guard so a
  // cancellation throw out of `visit_guarded_delete_scan_positions`
  // cannot leak the flag. The conditional ScopedBoolOverride is held
  // in an optional so the constexpr-branch can omit it for context
  // types that don't expose `skipAfterDelete`.
  std::optional<detail::ScopedBoolOverride> skipAfterDeleteGuard;
  if constexpr (requires { ctx.skipAfterDelete; }) {
    skipAfterDeleteGuard.emplace(ctx.skipAfterDelete, false);
  }
  auto &&match = matchFn;
  auto &&onMatch = onMatchFn;
  const char *matchedEnd = nullptr;
  std::uint32_t matchedDeleteCount = 0u;
  const auto result = detail::visit_guarded_delete_scan_positions(
      ctx,
      [&ctx, cursorStart]() noexcept {
        return detail::allows_extended_terminal_delete_scan_match(
            ctx, cursorStart, ctx.cursor());
      },
      [&](const detail::DeleteScanVisitState &state) {
        const char *const scanCursor = ctx.cursor();
        if constexpr (requires { ctx.skip_without_builder(scanCursor); }) {
          if (state.hiddenTriviaBoundary) {
            const char *const skipped = ctx.skip_without_builder(scanCursor);
            if (const char *const visibleMatch =
                    detail::invoke_delete_scan_match(match, skipped,
                                                     state.deleteCount);
                visibleMatch != nullptr) {
              if constexpr (requires { ctx.skip(); }) {
                ctx.skip();
              }
              matchedEnd = visibleMatch;
              matchedDeleteCount = state.deleteCount;
              return detail::DeleteScanVisitResult::Accept;
            }
            return detail::DeleteScanVisitResult::Continue;
          }
        }
        if (facts.previousElementIsTerminalish && scanCursor > cursorStart) {
          return detail::DeleteScanVisitResult::Continue;
        }
        if (const char *const scanMatch =
                detail::invoke_delete_scan_match(match, scanCursor,
                                                 state.deleteCount);
            scanMatch != nullptr) {
          matchedEnd = scanMatch;
          matchedDeleteCount = state.deleteCount;
          return detail::DeleteScanVisitResult::Accept;
        }
        return detail::DeleteScanVisitResult::Continue;
      },
      {.extendThroughHiddenTrivia = true,
       .stopAtHiddenTriviaBoundary = true,
       .visitAfterHiddenTriviaExtension = false});
  skipAfterDeleteGuard.reset();
  if (result != detail::DeleteScanVisitResult::Accept) {
    return false;
  }
  TerminalLegalityFacts legality;
  legality.budgetAllowsDeleteScan = budgetAllowsDeleteScan;
  legality.strictTerminalOrFollowAfterScan = matchedEnd != nullptr;
  if (!is_terminal_delete_scan_legal(shape, legality)) {
    return false;
  }
  detail::invoke_delete_scan_on_match(onMatch, matchedEnd, matchedDeleteCount);
  return true;
}

template <EditableParseModeContext Context, typename MatchFn,
          typename OnMatchFn>
[[nodiscard]] TerminalRecoveryCandidate evaluate_delete_scan_terminal_candidate(
    Context &ctx, const char *cursorStart, MatchFn &&matchFn,
    OnMatchFn &&onMatchFn, TerminalRecoveryFacts facts,
    TerminalShape shape) {
  TerminalRecoveryCandidate candidate;
  const auto checkpoint = ctx.mark();
  if (recover_by_terminal_delete_scan(ctx, std::forward<MatchFn>(matchFn),
                                      std::forward<OnMatchFn>(onMatchFn), facts,
                                      shape)) {
    const auto cost = ctx.editCostDelta(checkpoint);
    const auto deletedCost = default_edit_cost(ParseDiagnosticKind::Deleted);
    const auto distance = deletedCost == 0u ? 0u : cost / deletedCost;
    candidate.kind = TerminalRecoveryChoiceKind::DeleteScan;
    candidate.cost = make_recovery_cost(cost, distance, cost);
    candidate.distance = distance;
    candidate.consumed = static_cast<std::size_t>(ctx.cursor() - cursorStart);
    candidate.substitutionCount = 0u;
    candidate.operationCount = deletedCost == 0u ? 0u : cost / deletedCost;
  }
  ctx.rewind(checkpoint);
  return candidate;
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
    if (!ctx.insertSynthetic(element)) {
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
