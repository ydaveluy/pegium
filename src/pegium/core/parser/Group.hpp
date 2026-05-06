#pragma once

/// Parser expression representing an ordered sequence of child expressions.

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RecoveryEditSupport.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/SkipperWrapped.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/utils/TextUtils.hpp>
#include <string>
#include <string_view>

namespace pegium::parser {

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct Repetition;

namespace detail {

template <typename Element>
struct NullableSiblingOwnershipPolicy {
  static constexpr bool allowRecoverableContinuation = true;
};

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct NullableSiblingOwnershipPolicy<Repetition<min, max, Element>> {
  static constexpr bool allowRecoverableContinuation = max != 1u;
};

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct NullableSiblingOwnershipPolicy<
    SkipperWrapped<Repetition<min, max, Element>>> {
  static constexpr bool allowRecoverableContinuation = max != 1u;
};

template <typename Element>
inline constexpr bool nullable_sibling_allows_recoverable_continuation_v =
    NullableSiblingOwnershipPolicy<std::remove_cvref_t<Element>>::
        allowRecoverableContinuation;

} // namespace detail

template <Expression... Elements> struct Group : grammar::Group {
  static constexpr bool nullable =
      (... && std::remove_cvref_t<Elements>::nullable);
  static constexpr bool isFailureSafe = false;
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");

  constexpr explicit Group(std::tuple<Elements...> &&tupleElements)
      : elements{std::move(tupleElements)} {}

protected:
  constexpr explicit Group(const std::tuple<Elements...> &tupleElements)
      : elements{tupleElements} {}

public:
  constexpr Group(Group &&) noexcept = default;
  constexpr Group(const Group &) = default;
  constexpr Group &operator=(Group &&) noexcept = default;
  constexpr Group &operator=(const Group &) = default;

  bool probeRecoverable(RecoveryContext &ctx) const {
    return probe_recoverable_elements<0>(ctx);
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    return probe_recoverable_entry_elements<0>(ctx);
  }

  bool probeRecoverableAtEntryConsumesVisible(RecoveryContext &ctx) const {
    return probe_recoverable_entry_consumes_visible_elements<0>(ctx);
  }

  constexpr const char *terminal(const char *begin) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    return terminal_impl(begin);
  }
  constexpr const char *terminal(const std::string &text) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    return terminal(text.c_str());
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<std::tuple<Elements...>>
  auto skip(LocalSkipper &&localSkipper) const & {
    return with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto skip(LocalSkipper &&localSkipper) && {
    return std::move(*this).with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>))) &&
            std::copy_constructible<std::tuple<Elements...>>
  auto skip(SkipperParts &&...parts) const & {
    return with_skipper(parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>)))
  auto skip(SkipperParts &&...parts) && {
    return std::move(*this).with_skipper(
        parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<std::tuple<Elements...>>
  auto with_skipper(LocalSkipper &&localSkipper) const & {
    return SkipperWrapped<Group<Elements...>>{
        Group<Elements...>{elements},
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return SkipperWrapped<Group<Elements...>>{
        Group<Elements...>{std::move(elements)},
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  const AbstractElement *get(std::size_t elementIndex) const noexcept override {
    if (elementIndex >= sizeof...(Elements))
      return nullptr;
    return get_impl(elementIndex,
                    std::make_index_sequence<sizeof...(Elements)>());
  }

  std::size_t size() const noexcept override { return sizeof...(Elements); }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  std::tuple<Elements...> elements;

  [[nodiscard]] static constexpr bool
  element_is_terminal_like_for_missing_insert(
      const grammar::AbstractElement &element) noexcept {
    return element.getKind() == ElementKind::Literal ||
           element.getKind() == ElementKind::TerminalRule;
  }

  /// Per-call upper bound on temporary recovery candidates evaluated
  /// inside one dispatch site (one element index `I`) of
  /// `parse_elements`. At a single dispatch site the recovery flow
  /// considers, in mutually-exclusive paths:
  ///
  ///   - `select_missing_element_insert_replay` evaluates 2 candidates
  ///     internally (the synthetic insert + the actual parse).
  ///   - `select_current_failure_sequence_attempt` evaluates up to 4
  ///     candidates (2 terminal current-failure + 1 reparse-without-
  ///     delete + 1 skip-nullable).
  ///   - `recover_after_tail_parse_failure` evaluates one in-place
  ///     `RepairTail` attempt plus up to 2 fallback candidates
  ///     (reparse-without-delete + skip-nullable).
  ///
  /// The dispatch enters at most one of these per site, so the bound
  /// per site is `max(2, 4, 3) = 4`. The bound is the per-site
  /// figure: the recursive call `parse_elements<I+1>` opens a new
  /// runtime counter session, so cumulative growth across elements
  /// is bounded site-by-site rather than amortized into a single
  /// ceiling. Independent of arity and input length.
  static constexpr std::size_t kMaxRecoveryCandidatesPerCall = 4U;

public:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    return fast_probe_elements<0>(ctx);
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    return parse_elements<Context, 0>(ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    init_elements<0>(ctx);
  }

  /// Sequential dispatcher per-mode. Each mode's logic lives in a
  /// dedicated helper (`parse_elements_strict`, `parse_elements_recovery`,
  /// `parse_elements_expect`) so the strict-only path is physically
  /// separated from the recovery branch (the lint
  /// `check_strict_path` verifies the strict helper does not touch
  /// any recovery symbol).
  template <ParseModeContext Context, std::size_t I>
  bool parse_elements(
      Context &ctx,
      bool previousNullableSiblingConsumedVisible = false) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      const char *cursorBeforeSkip = nullptr;
      if constexpr (I > 0) {
        cursorBeforeSkip = ctx.cursor();
        ctx.skip();
      }
      if constexpr (StrictParseModeContext<Context>) {
        return parse_elements_strict<Context, I>(ctx);
      } else if constexpr (RecoveryParseModeContext<Context>) {
        return parse_elements_recovery<Context, I>(
            ctx, cursorBeforeSkip, previousNullableSiblingConsumedVisible);
      } else {
        return parse_elements_expect<Context, I>(ctx);
      }
    }
  }

private:
  template <StrictParseModeContext Context, std::size_t I>
  bool parse_elements_strict(Context &ctx) const {
    if (!parse(std::get<I>(elements), ctx)) {
      return false;
    }
    return parse_elements<Context, I + 1>(ctx);
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool parse_elements_recovery(Context &ctx,
                                const char *cursorBeforeSkip,
                                bool previousNullableSiblingConsumedVisible) const {
    if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows() &&
        !ctx.allowsCompletedWindowContinuationRecovery()) {
      // Track lastVisibleCursorOffset across the strict parse so we can
      // forward `currentNullableConsumedVisible` to element I+1: if element
      // I+1's recovery later consults `previous_nullable_sibling_owns_cursor`,
      // it needs to know whether element I (a nullable sibling) consumed
      // visible input. The full recovery branch below already does this via
      // `nullable_current_consumed_visible_since`; the fast-path must too.
      using CurrentElement =
          std::remove_cvref_t<decltype(std::get<I>(elements))>;
      const TextOffset visibleBefore =
          CurrentElement::nullable ? ctx.lastVisibleCursorOffset() : 0u;
      if (TrackedParseContext &strictCtx = ctx;
          !parse(std::get<I>(elements), strictCtx)) {
        return false;
      }
      const bool currentNullableConsumedVisible =
          CurrentElement::nullable &&
          ctx.lastVisibleCursorOffset() > visibleBefore;
      return parse_elements<Context, I + 1>(ctx,
                                            currentNullableConsumedVisible);
    }
    const bool nullableCurrentLooksStarted =
        this->template probe_nullable_current_looks_started<I>(ctx);
    const bool previousNullableSiblingOwnsCursor =
        this->template previous_nullable_sibling_owns_cursor<I>(
            ctx, previousNullableSiblingConsumedVisible);
    // Transition: InsertMissingCurrent (early-exit replay path).
    if (this->template select_missing_element_insert_replay<I>(ctx)) {
      bool matchedCleanTail = false;
      return this->template replay_insert_missing_element_attempt<I>(
          ctx, matchedCleanTail);
    }
    const auto checkpoint = ctx.mark();
    const auto &current = std::get<I>(elements);
    const auto terminalRecoveryState =
        (current.getKind() == ElementKind::Literal ||
         current.getKind() == ElementKind::TerminalRule)
            ? build_terminal_recovery_state<I>(
                  ctx, checkpoint, cursorBeforeSkip,
                  previousNullableSiblingOwnsCursor)
            : TerminalRecoveryState{};
    bool strictSuffixStartsAtEntry = false;
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      strictSuffixStartsAtEntry =
          this->template suffix_starts_without_edits<I + 1>(ctx);
    }
    const bool visibleNullableRepairCompetesWithStrictSuffix =
        this->template nullable_current_visible_repair_competes_with_strict_suffix<
            I>(ctx, strictSuffixStartsAtEntry);
    if (visibleNullableRepairCompetesWithStrictSuffix) {
      ctx.rewind(checkpoint);
      if (this->template parse_competing_nullable_current_repair<I>(
              ctx, terminalRecoveryState.facts)) {
        const bool currentCommittedProgress =
            ctx.cursor() != checkpoint.parseCheckpoint.parseCheckpoint.cursor ||
            ctx.currentEditCount() !=
                checkpoint.recoveryState.editBudget.editCount ||
            ctx.currentEditCost() !=
                checkpoint.recoveryState.editBudget.editCost;
        if (currentCommittedProgress) {
          const bool currentNullableConsumedVisible =
              this->template nullable_current_consumed_visible_since<I>(
                  ctx, checkpoint);
          if (parse_elements<Context, I + 1>(ctx,
                                             currentNullableConsumedVisible)) {
            return true;
          }
        }
      }
      ctx.rewind(checkpoint);
    }
    // Transition: SkipNullable.
    if (!visibleNullableRepairCompetesWithStrictSuffix &&
        this->template try_skip_nullable_transition<Context, I>(
            ctx, nullableCurrentLooksStarted, strictSuffixStartsAtEntry)) {
      return true;
    }
    // Transition: KeepCurrent (no-edit) when the strict suffix is
    // already visible at entry, otherwise the regular strict attempt
    // that may trigger RepairCurrent on failure.
    if (strictSuffixStartsAtEntry) {
      auto noEditGuard = ctx.withEditTrackingDisabled();
      (void)noEditGuard;
      if (!parse_current_sequence_element<I>(ctx,
                                              terminalRecoveryState.facts)) {
        return recover_after_current_parse_failure<Context, I>(
            ctx, checkpoint, terminalRecoveryState,
            nullableCurrentLooksStarted,
            previousNullableSiblingOwnsCursor);
      }
    } else if (!parse_current_sequence_element<I>(
                    ctx, terminalRecoveryState.facts)) {
      return recover_after_current_parse_failure<Context, I>(
          ctx, checkpoint, terminalRecoveryState,
          nullableCurrentLooksStarted,
          previousNullableSiblingOwnsCursor);
    }
    const auto checkpointAfterCurrent = ctx.mark();
    const bool currentCommittedProgress =
        ctx.cursor() != checkpoint.parseCheckpoint.parseCheckpoint.cursor ||
        ctx.currentEditCount() !=
            checkpoint.recoveryState.editBudget.editCount ||
        ctx.currentEditCost() !=
            checkpoint.recoveryState.editBudget.editCost;
    const bool currentNullableConsumedVisible =
        this->template nullable_current_consumed_visible_since<I>(
            ctx, checkpoint);
    if (parse_elements<Context, I + 1>(
            ctx, currentNullableConsumedVisible)) {
      return true;
    }
    // Transition: RepairTail — admissible iff Current actually committed
    // something (cursor advanced or edits accumulated). A nullable
    // Current that matched ε falls through here without admission so
    // that the equivalent SkipNullable path (already tried earlier) is
    // not wastefully retried.
    if (!currentCommittedProgress) {
      return false;
    }
    return recover_after_tail_parse_failure<Context, I>(
        ctx, checkpoint, checkpointAfterCurrent, terminalRecoveryState.facts);
  }

  template <ParseModeContext Context, std::size_t I>
  bool parse_elements_expect(Context &ctx) const {
    const auto checkpoint = ctx.mark();
    if (!parse(std::get<I>(elements), ctx)) {
      return false;
    }
    if (!ctx.frontierBlocked()) {
      return parse_elements<Context, I + 1>(ctx);
    }
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      auto currentFrontier = capture_frontier_since(ctx, checkpoint);
      ctx.clearFrontierBlock();
      if (parse_elements<Context, I + 1>(ctx)) {
        const bool tailBlocked = ctx.frontierBlocked();
        merge_captured_frontier(ctx, currentFrontier, !tailBlocked);
        return true;
      }
      ctx.rewind(checkpoint);
      merge_captured_frontier(ctx, currentFrontier, false);
    }
    return true;
  }

  struct SequenceRecoveryReplayPlan {
    bool valid = false;
    bool insertCurrentTerminal = false;
    std::uint32_t currentTerminalDeleteRunCount = 0u;
    bool currentTerminalDeleteRunEndsAfterHiddenTriviaExtension = false;
    bool reparseCurrentWithoutDelete = false;
    bool skipNullable = false;
    bool preferTailEntryInsert = false;
    bool allowDeleteRecovery = false;
    bool allowNullableTailStop = false;
  };

  struct TerminalDeleteRunObservation {
    std::uint32_t deleteCount = 0u;
    bool endsAfterHiddenTriviaExtension = false;
  };

  struct TerminalRecoveryState {
    detail::TerminalRecoveryFacts facts{};
    bool hasRecoveredPrefixBeforeCurrent = false;
    bool skippedFromRecoveryWindowBeforeCurrent = false;
    std::uint32_t sequenceEditCountBase = 0u;
  };

  struct SequenceFacts {
    bool suffixFullyNullable = false;
    bool strictSuffixStartsAtCurrentCursor = false;
    bool recoverableSuffixStartsAtCurrentCursor = false;
    bool currentOffsetWithinRecoveryWindow = false;
    bool previousNullableSiblingOwnsCursor = false;
  };

  struct SequenceSuffixEntryFacts {
    bool strictStartsAtCurrentCursor = false;
    bool recoverableStartsAtCurrentCursor = false;
  };

  struct RecoverableFollowConsumesVisibleProbe {
    const Group *self = nullptr;
    RecoveryContext::FollowProbeFn outerRecoverableConsumesVisible = nullptr;
    const void *outerRecoverableConsumesVisibleData = nullptr;
  };

  static void consider_sequence_recovery_candidate(
      SequenceRecoveryReplayPlan &bestPlan,
      detail::EditableRecoveryCandidate &bestCandidate,
      const SequenceRecoveryReplayPlan &candidatePlan,
      const detail::EditableRecoveryCandidate &candidate) noexcept {
    if (!candidate.matched) {
      return;
    }
    // Ranking via central `is_better_recovery_key` on the
    // candidate-derived `RecoveryKey`. Group has no family-redundancy
    // step (each plan is its own family), so the pipeline reduces to
    // admission + ranking.
    if (!bestCandidate.matched ||
        detail::is_better_recovery_key(
            detail::editable_recovery_key(candidate),
            detail::editable_recovery_key(bestCandidate))) {
      bestPlan = candidatePlan;
      bestCandidate = candidate;
    }
  }

  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return fast_probe_elements<0>(ctx);
  }

  template <std::size_t I, StrictParseModeContext Context>
  bool fast_probe_elements(Context &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (attempt_fast_probe(ctx, current)) {
        return true;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return fast_probe_elements<I + 1>(ctx);
      } else {
        return false;
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] constexpr bool
  nullable_element_allows_recovery_skip() const noexcept {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      // Predicates (NotPredicate / AndPredicate) are nullable — they
      // consume nothing — but they are guards, not optional content. The
      // recovery skip-nullable transition would treat them as ε and
      // bypass them entirely, which silently drops the lookahead check.
      // For example, `many(!"K"_kw + Item)` would let `Item` swallow `K`
      // because the surrounding skip-nullable path never evaluates the
      // NotPredicate. Same hazard applies to AndPredicate guards.
      return current.getKind() != ElementKind::Create &&
             current.getKind() != ElementKind::NotPredicate &&
             current.getKind() != ElementKind::AndPredicate;
    }
  }

  template <std::size_t I>
  bool probe_recoverable_elements(RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (attempt_fast_probe(ctx, current) ||
          probe_locally_recoverable(current, ctx)) {
        return true;
      }
      if (current.getKind() == ElementKind::AndPredicate) {
        return false;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return probe_recoverable_elements<I + 1>(ctx);
      } else {
        return probe_recoverable_suffix_after_missing_current<I + 1>(ctx);
      }
    }
  }

  template <std::size_t I>
  bool suffix_starts_without_edits(RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (attempt_fast_probe(ctx, current)) {
        return true;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return suffix_starts_without_edits<I + 1>(ctx);
      } else {
        return false;
      }
    }
  }

  template <std::size_t I>
  bool probe_recoverable_entry_elements(RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (probe_recoverable_at_entry(current, ctx)) {
        return true;
      }
      if (current.getKind() == ElementKind::AndPredicate) {
        return false;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return probe_recoverable_entry_elements<I + 1>(ctx);
      } else {
        return false;
      }
    }
  }

  template <std::size_t I>
  bool probe_recoverable_entry_consumes_visible_elements(
      RecoveryContext &ctx,
      bool allowSyntheticLeadingTerminalContinuation = false,
      RecoveryContext::FollowProbeFn outerRecoverableConsumesVisible = nullptr,
      const void *outerRecoverableConsumesVisibleData = nullptr) const {
    if constexpr (I == sizeof...(Elements)) {
      return outerRecoverableConsumesVisible != nullptr &&
             outerRecoverableConsumesVisible(
                 ctx, outerRecoverableConsumesVisibleData);
    } else {
      const auto &current = std::get<I>(elements);
      if (attempt_fast_probe(ctx, current) ||
          probe_recoverable_at_entry_consumes_visible(current, ctx)) {
        return true;
      }
      if (ctx.allowsScopedLeadingTerminalInsertRecovery() &&
          element_is_terminal_like_for_missing_insert(current) &&
          allows_synthetic_terminal_insert(ctx, current) &&
          detail::cursor_starts_visible_source(ctx)) {
        const bool suffixConsumesVisible =
            this->template suffix_starts_without_edits<I + 1>(ctx) ||
            (I + 1 == sizeof...(Elements) &&
             outerRecoverableConsumesVisible != nullptr &&
             outerRecoverableConsumesVisible(
                 ctx, outerRecoverableConsumesVisibleData));
        if (suffixConsumesVisible) {
          return true;
        }
      }
      if (allowSyntheticLeadingTerminalContinuation &&
          element_is_terminal_like_for_missing_insert(current) &&
          allows_synthetic_terminal_insert(
              ctx, current, /*allowSequenceLiteralInsert=*/true) &&
          detail::cursor_starts_visible_source(ctx)) {
        const bool suffixConsumesVisible =
            this->template suffix_starts_without_edits<I + 1>(ctx) ||
            (I + 1 == sizeof...(Elements) &&
             outerRecoverableConsumesVisible != nullptr &&
             outerRecoverableConsumesVisible(
                 ctx, outerRecoverableConsumesVisibleData));
        if (suffixConsumesVisible) {
          return true;
        }
      }
      if (current.getKind() == ElementKind::AndPredicate) {
        return false;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return probe_recoverable_entry_consumes_visible_elements<I + 1>(
            ctx, allowSyntheticLeadingTerminalContinuation,
            outerRecoverableConsumesVisible,
            outerRecoverableConsumesVisibleData);
      } else {
        return false;
      }
    }
  }

  template <std::size_t I>
  static bool recoverable_follow_consumes_visible_probe(RecoveryContext &ctx,
                                                        const void *data) {
    const auto &probe =
        *static_cast<const RecoverableFollowConsumesVisibleProbe *>(data);
    return probe.self
        ->template probe_recoverable_entry_consumes_visible_elements<I>(
            ctx, /*allowSyntheticLeadingTerminalContinuation=*/true,
            probe.outerRecoverableConsumesVisible,
            probe.outerRecoverableConsumesVisibleData);
  }

  template <std::size_t I>
  [[nodiscard]] SequenceSuffixEntryFacts
  analyze_suffix_entry_facts(RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return {};
    } else {
      const auto &current = std::get<I>(elements);
      const bool strictStartsAtCurrentCursor =
          attempt_fast_probe(ctx, current);
      const bool recoverableStartsAtCurrentCursor =
          probe_recoverable_at_entry(current, ctx);
      if (strictStartsAtCurrentCursor || recoverableStartsAtCurrentCursor) {
        return {
            .strictStartsAtCurrentCursor = strictStartsAtCurrentCursor,
            .recoverableStartsAtCurrentCursor =
                recoverableStartsAtCurrentCursor,
        };
      }
      if (current.getKind() == ElementKind::AndPredicate) {
        return {};
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return analyze_suffix_entry_facts<I + 1>(ctx);
      } else {
        return {};
      }
    }
  }

  template <std::size_t I>
  bool probe_recoverable_suffix_after_missing_current(
      RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (attempt_fast_probe(ctx, current) ||
          probe_recoverable_at_entry(current, ctx) ||
          probe_locally_recoverable(current, ctx)) {
        return true;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return probe_recoverable_suffix_after_missing_current<I + 1>(ctx);
      } else {
        return false;
      }
    }
  }

  /// Probe whether the nullable current element appears to have
  /// started accepting at the cursor. For non-nullable elements,
  /// structurally false. For predicate elements (`AndPredicate`,
  /// `NotPredicate`), false because predicates do not consume input.
  /// Probe is non-mutating: cursor and furthest explored cursor
  /// are restored.
  template <std::size_t I>
  [[nodiscard]] bool
  probe_nullable_current_looks_started(RecoveryContext &ctx) const {
    if constexpr (!std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      (void)ctx;
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (current.getKind() == ElementKind::AndPredicate ||
          current.getKind() == ElementKind::NotPredicate) {
        return false;
      }
      detail::ProbeRestoreScope guard{ctx};
      return attempt_fast_probe(ctx, current);
    }
  }

  template <std::size_t I>
  [[nodiscard]] SequenceFacts
  build_sequence_facts(
      RecoveryContext &ctx,
      bool previousNullableSiblingOwnsCursor = false) const {
    const auto suffixEntryFacts =
        this->template analyze_suffix_entry_facts<I + 1>(ctx);
    return {
        .suffixFullyNullable = suffix_after_is_fully_nullable<I + 1>(),
        .strictSuffixStartsAtCurrentCursor =
            suffixEntryFacts.strictStartsAtCurrentCursor,
        .recoverableSuffixStartsAtCurrentCursor =
            suffixEntryFacts.recoverableStartsAtCurrentCursor,
        .currentOffsetWithinRecoveryWindow =
            ctx.hasPendingRecoveryWindows() &&
            (ctx.cursorOffset() <= ctx.pendingRecoveryWindowMaxCursorOffset() ||
             ctx.isInRecoveryPhase()),
        .previousNullableSiblingOwnsCursor = previousNullableSiblingOwnsCursor,
    };
  }

  template <std::size_t I, typename Checkpoint>
  [[nodiscard]] TerminalRecoveryState build_terminal_recovery_state(
      const RecoveryContext &ctx, const Checkpoint &checkpoint,
      const char *cursorBeforeSkip,
      bool previousNullableSiblingOwnsCursor) const {
    constexpr bool suffixFullyNullable = suffix_after_is_fully_nullable<I + 1>();
    const bool hasCommittedVisiblePrefixBeforeCurrent =
        (cursorBeforeSkip != nullptr &&
         (cursorBeforeSkip > ctx.begin ||
          ctx.lastVisibleCursorOffset() >=
              static_cast<TextOffset>(cursorBeforeSkip - ctx.begin))) ||
        ctx.lastVisibleCursorOffset() > 0u;
    const bool hasRecoveredPrefixBeforeCurrent =
        (cursorBeforeSkip != nullptr &&
         (cursorBeforeSkip > ctx.begin ||
          ctx.lastVisibleCursorOffset() >=
              static_cast<TextOffset>(cursorBeforeSkip - ctx.begin))) ||
        checkpoint.recoveryState.editBudget.hadEdits;
    const char *const lastVisibleCursor = ctx.begin + ctx.lastVisibleCursorOffset();
    const char *triviaGapBegin = nullptr;
    if (cursorBeforeSkip != nullptr && ctx.cursor() > cursorBeforeSkip) {
      triviaGapBegin = cursorBeforeSkip;
    } else if (ctx.cursor() > lastVisibleCursor &&
               ctx.skip_without_builder(lastVisibleCursor) == ctx.cursor()) {
      triviaGapBegin = lastVisibleCursor;
    }
    auto triviaGap = detail::TriviaGapProfile{};
    if (hasRecoveredPrefixBeforeCurrent) {
      triviaGap =
          detail::make_trivia_gap_profile(ctx, triviaGapBegin, ctx.cursor());
    }
    bool previousIsTerminalish = false;
    if constexpr (I > 0) {
      const auto &previous = std::get<I - 1>(elements);
      previousIsTerminalish =
          previous.getKind() == ElementKind::Literal ||
          previous.getKind() == ElementKind::TerminalRule;
    }
    const bool skippedFromRecoveryWindowBeforeCurrent =
        cursorBeforeSkip != nullptr && ctx.hasPendingRecoveryWindows() &&
        ctx.cursor() > cursorBeforeSkip &&
        ctx.skip_without_builder(cursorBeforeSkip) == ctx.cursor() &&
        static_cast<TextOffset>(cursorBeforeSkip - ctx.begin) <=
            ctx.pendingRecoveryWindowMaxCursorOffset();
    return {
        .facts =
            {
                .triviaGap = triviaGap,
                .previousElementIsTerminalish = previousIsTerminalish,
                .allowStructuredVisibleContinuationInsert =
                    hasCommittedVisiblePrefixBeforeCurrent,
                .localRecoveryBlocked =
                    previousNullableSiblingOwnsCursor,
            },
        .hasRecoveredPrefixBeforeCurrent = hasRecoveredPrefixBeforeCurrent,
        .skippedFromRecoveryWindowBeforeCurrent =
            skippedFromRecoveryWindowBeforeCurrent,
        .sequenceEditCountBase = checkpoint.recoveryState.editBudget.editCount,
    };
  }

  template <std::size_t I, typename Fn>
  bool with_current_follow_probe(RecoveryContext &ctx, Fn &&fn) const {
    // Install the current element's follow only while the current element
    // itself is parsed or probed. Keeping this guard alive while parsing the
    // recursive tail would make a last child pass through to the previous
    // sibling's local follow instead of the enclosing follow.
    if constexpr (I + 1 == sizeof...(Elements)) {
      auto followGuard = ctx.withPassThroughOuterFollowProbe();
      (void)followGuard;
      return std::forward<Fn>(fn)();
    } else {
      constexpr auto strictProbeFn =
          +[](RecoveryContext &c, const void *data) -> bool {
        const auto *self = static_cast<const Group *>(data);
        return self->template suffix_starts_without_edits<I + 1>(c);
      };
      constexpr auto recoverableProbeFn =
          +[](RecoveryContext &c, const void *data) -> bool {
        const auto *self = static_cast<const Group *>(data);
        return self->template probe_recoverable_entry_elements<I + 1>(c);
      };
      constexpr auto recoverableConsumesVisibleProbeFn =
          &Group::template recoverable_follow_consumes_visible_probe<I + 1>;
      RecoverableFollowConsumesVisibleProbe recoverableConsumesVisibleProbe{
          .self = this,
          .outerRecoverableConsumesVisible =
              ctx._recoverableFollowConsumesVisibleProbeFn,
          .outerRecoverableConsumesVisibleData =
              ctx._recoverableFollowConsumesVisibleProbeData,
      };
      auto followGuard = ctx.withFollowProbe(
          strictProbeFn, this, recoverableProbeFn, this,
          recoverableConsumesVisibleProbeFn, &recoverableConsumesVisibleProbe);
      (void)followGuard;
      return std::forward<Fn>(fn)();
    }
  }

  template <std::size_t I>
  [[nodiscard]] bool
  previous_nullable_sibling_owns_cursor(
      RecoveryContext &ctx,
      bool previousNullableSiblingConsumedVisible) const {
    if constexpr (I == 0) {
      (void)ctx;
      return false;
    } else {
      const auto &previous = std::get<I - 1>(elements);
      using Previous = std::remove_cvref_t<decltype(previous)>;
      if constexpr (!Previous::nullable) {
        (void)ctx;
        (void)previousNullableSiblingConsumedVisible;
        return false;
      } else {
        detail::ProbeRestoreScope guard{ctx};
        return this->template with_current_follow_probe<I - 1>(ctx, [&]() {
          auto deleteObservationGuard =
              ctx.withEditPermissions(ctx.allowInsert, true);
          (void)deleteObservationGuard;
          if (previousNullableSiblingConsumedVisible &&
              !detail::nullable_sibling_allows_recoverable_continuation_v<
                  Previous>) {
            return false;
          }
          bool previousStrictProgress = false;
          {
            detail::ProbeRestoreScope strictGuard{ctx};
            const TextOffset previousStrictStartOffset = ctx.cursorOffset();
            previousStrictProgress =
                attempt_parse_no_edits(ctx, previous) &&
                ctx.cursorOffset() > previousStrictStartOffset;
          }
          if (previousStrictProgress) {
            return true;
          }
          const auto &current = std::get<I>(elements);
          if (element_is_terminal_like_for_missing_insert(current) &&
              allows_synthetic_terminal_insert(
                  ctx, current, /*allowSequenceLiteralInsert=*/true) &&
              detail::cursor_starts_visible_source(ctx)) {
            const auto suffixEntryFacts =
                this->template analyze_suffix_entry_facts<I + 1>(ctx);
            if (suffix_after_is_fully_nullable<I + 1>() ||
                suffixEntryFacts.strictStartsAtCurrentCursor ||
                suffixEntryFacts.recoverableStartsAtCurrentCursor) {
              return false;
            }
          }
          if (attempt_fast_probe(ctx, previous)) {
            return true;
          }
          return probe_recoverable_at_entry_consumes_visible(previous, ctx);
        });
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] bool nullable_current_consumed_visible_since(
      const RecoveryContext &ctx,
      const RecoveryContext::Checkpoint &checkpoint) const noexcept {
    if constexpr (!std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      (void)ctx;
      (void)checkpoint;
      return false;
    } else {
      return ctx.lastVisibleCursorOffset() >
             static_cast<TextOffset>(
                 checkpoint.parseCheckpoint.parseCheckpoint.lastVisibleCursor -
                 ctx.begin);
    }
  }

  template <std::size_t I>
  bool parse_current_sequence_element(
      RecoveryContext &ctx, const detail::TerminalRecoveryFacts &facts) const {
    return with_current_follow_probe<I>(ctx, [&]() {
      const auto &current = std::get<I>(elements);
      if constexpr (requires {
                      current.parse_terminal_recovery_impl(ctx, facts);
                    }) {
        return current.parse_terminal_recovery_impl(ctx, facts);
      } else {
        return parse(current, ctx);
      }
    });
  }

  template <std::size_t I>
  bool parse_competing_nullable_current_repair(
      RecoveryContext &ctx, const detail::TerminalRecoveryFacts &facts) const {
    if constexpr (!std::remove_cvref_t<decltype(std::get<I>(
                      elements))>::nullable) {
      (void)ctx;
      (void)facts;
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      auto noFollowGuard = ctx.withFollowProbe(nullptr, nullptr, nullptr,
                                               nullptr, nullptr, nullptr);
      (void)noFollowGuard;
      if constexpr (requires {
                      current.parse_terminal_recovery_impl(ctx, facts);
                    }) {
        return current.parse_terminal_recovery_impl(ctx, facts);
      } else {
        return parse(current, ctx);
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] bool
  nullable_current_visible_repair_competes_with_strict_suffix(
      RecoveryContext &ctx, bool strictSuffixStartsAtEntry) const {
    if constexpr (!std::remove_cvref_t<decltype(std::get<I>(
                      elements))>::nullable) {
      (void)ctx;
      (void)strictSuffixStartsAtEntry;
      return false;
    } else {
      if (!strictSuffixStartsAtEntry) {
        return false;
      }
      detail::ProbeRestoreScope guard{ctx};
      const auto &current = std::get<I>(elements);
      auto noFollowGuard = ctx.withFollowProbe(nullptr, nullptr, nullptr,
                                               nullptr, nullptr, nullptr);
      (void)noFollowGuard;
      return probe_recoverable_at_entry_consumes_visible(current, ctx);
    }
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  detail::EditableRecoveryCandidate evaluate_skip_nullable_attempt(
      Context &ctx, const Checkpoint &checkpoint,
      bool preferTailEntryInsert = false) const {
    return detail::evaluate_editable_recovery_candidate(
        ctx, checkpoint, checkpoint.recoveryState.editBudget.editCost,
        checkpoint.recoveryState.editBudget.editCount,
        [this, &ctx, preferTailEntryInsert]() {
          return this->template replay_skip_nullable_attempt<Context, I>(
              ctx, preferTailEntryInsert);
        });
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_skip_nullable_attempt(Context &ctx,
                                    bool preferTailEntryInsert = false) const {
    const auto &current = std::get<I>(elements);
    const auto parseTail = [this, &ctx]() {
      return this->template parse_elements<Context, I + 1>(ctx);
    };
    if (!preferTailEntryInsert) {
      return this->template nullable_element_allows_recovery_skip<I>()
                 ? parseTail()
                 : (this->template parse_current_sequence_element<I>(ctx, {}) &&
                    parseTail());
    }
    auto noDeleteGuard = ctx.withEditPermissions(ctx.allowInsert, false);
    (void)noDeleteGuard;
    const auto *tailFirstRequired = first_required_tail_element<I + 1>();
    const bool tailStartsWithTerminal =
        tailFirstRequired != nullptr &&
        (tailFirstRequired->getKind() == ElementKind::Literal ||
         tailFirstRequired->getKind() == ElementKind::TerminalRule);
    if (!tailStartsWithTerminal) {
      return this->template nullable_element_allows_recovery_skip<I>()
                 ? parseTail()
                 : (this->template parse_current_sequence_element<I>(ctx, {}) &&
                    parseTail());
    }
    detail::ScopedBoolOverride leadingInsertGuard{
        ctx.allowLeadingTerminalInsertScope, true};
    (void)leadingInsertGuard;
    return this->template nullable_element_allows_recovery_skip<I>()
               ? parseTail()
               : (this->template parse_current_sequence_element<I>(ctx, {}) &&
                  parseTail());
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  detail::EditableRecoveryCandidate evaluate_current_without_delete_attempt(
      Context &ctx, const Checkpoint &checkpoint,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts,
      bool allowDeleteRecovery) const {
    return detail::evaluate_editable_recovery_candidate(
        ctx, checkpoint, checkpoint.recoveryState.editBudget.editCost,
        checkpoint.recoveryState.editBudget.editCount,
        [this, &ctx, &terminalRecoveryFacts, allowDeleteRecovery]() {
          return replay_current_without_delete_attempt<Context, I>(
              ctx, terminalRecoveryFacts, allowDeleteRecovery);
        });
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_current_without_delete_attempt(
      Context &ctx,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts,
      bool allowDeleteRecovery) const {
    if (allowDeleteRecovery) {
      return this->template parse_current_sequence_element<I>(
                 ctx, terminalRecoveryFacts) &&
             this->template parse_elements<Context, I + 1>(ctx);
    }
    {
      auto noDeleteGuard = ctx.withEditPermissions(ctx.allowInsert, false);
      (void)noDeleteGuard;
      if (!this->template parse_current_sequence_element<I>(
              ctx, terminalRecoveryFacts)) {
        return false;
      }
    }
    return this->template parse_elements<Context, I + 1>(ctx);
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_tail_after_current_attempt(Context &ctx) const {
    auto noDeleteGuard = ctx.withEditPermissions(ctx.allowInsert, false);
    (void)noDeleteGuard;
    return this->template parse_elements<Context, I + 1>(ctx);
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_current_failure_sequence_attempt(
      Context &ctx, const TerminalRecoveryState &terminalRecoveryState,
      const SequenceRecoveryReplayPlan &plan) const {
    if (!plan.valid) {
      return false;
    }
    if (plan.insertCurrentTerminal) {
      return replay_terminal_current_failure_attempt<I>(ctx, plan);
    }
    if (plan.reparseCurrentWithoutDelete) {
      return replay_current_without_delete_attempt<Context, I>(
          ctx, terminalRecoveryState.facts, plan.allowDeleteRecovery);
    }
    if (plan.skipNullable) {
      return replay_skip_nullable_attempt<Context, I>(
          ctx, plan.preferTailEntryInsert);
    }
    return false;
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_tail_failure_sequence_attempt(
      Context &ctx,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts,
      const SequenceRecoveryReplayPlan &plan) const {
    if (!plan.valid) {
      return false;
    }
    if (plan.reparseCurrentWithoutDelete) {
      return replay_current_without_delete_attempt<Context, I>(
          ctx, terminalRecoveryFacts, plan.allowDeleteRecovery);
    }
    if (plan.skipNullable) {
      return replay_skip_nullable_attempt<Context, I>(
          ctx, plan.preferTailEntryInsert);
    }
    return false;
  }

  [[nodiscard]] static bool terminal_allows_nullable_tail_stop(
      const TerminalRecoveryState &state,
      const SequenceFacts &sequenceFacts) noexcept {
    return state.facts.triviaGap.hasHiddenGap() ||
           sequenceFacts.suffixFullyNullable;
  }

  template <std::size_t I>
  [[nodiscard]] std::pair<bool, bool> build_terminal_sequence_legality(
      RecoveryContext &ctx, const TerminalRecoveryState &state,
      const SequenceFacts &sequenceFacts) const {
    const auto &current = std::get<I>(elements);
    const bool cursorStartsVisibleSource =
        detail::cursor_starts_visible_source(ctx);
    const bool sequenceLiteralInsertHasContinuation =
        ctx.cursor() >= ctx.end || sequenceFacts.suffixFullyNullable ||
        sequenceFacts.strictSuffixStartsAtCurrentCursor ||
        sequenceFacts.recoverableSuffixStartsAtCurrentCursor;
    const bool currentAllowsSyntheticInsert =
        allows_synthetic_terminal_insert(
            ctx, current, sequenceLiteralInsertHasContinuation);
    const bool currentSiteWithinRecoveryWindow =
        sequenceFacts.currentOffsetWithinRecoveryWindow ||
        state.skippedFromRecoveryWindowBeforeCurrent;
    const bool hasLocalSequenceEdits =
        ctx.currentEditCount() != state.sequenceEditCountBase;
    const bool scopedLeadingInitialInsertAllowed =
        I == 0u && ctx.currentWindowEditCount() == 0u &&
        ctx.allowsScopedLeadingTerminalInsertRecovery();
    const bool visibleSourceAvailableForScopedContinuation =
        cursorStartsVisibleSource ||
        state.facts.triviaGap.visibleSourceAfterLocalSkip ||
        detail::cursor_reaches_visible_source_after_local_skip(ctx);
    const bool recoveredPrefixImmediateInsertAllowed =
        ((state.hasRecoveredPrefixBeforeCurrent &&
          currentSiteWithinRecoveryWindow) ||
         (state.hasRecoveredPrefixBeforeCurrent &&
          sequenceFacts.suffixFullyNullable &&
          (state.facts.triviaGap.hasHiddenGap() || cursorStartsVisibleSource ||
           ctx.cursor() >= ctx.end))) &&
        (state.facts.triviaGap.hasHiddenGap() || !hasLocalSequenceEdits);
    const bool scopedLeadingContinuationInsertAllowed =
        I == 0u && ctx.allowsScopedLeadingTerminalInsertRecovery() &&
        !ctx.allowDelete && sequenceFacts.currentOffsetWithinRecoveryWindow &&
        visibleSourceAvailableForScopedContinuation;
    const bool committedPrefixContinuationInsertAllowed =
        state.facts.allowStructuredVisibleContinuationInsert &&
        !hasLocalSequenceEdits && visibleSourceAvailableForScopedContinuation &&
        (sequenceFacts.strictSuffixStartsAtCurrentCursor ||
         sequenceFacts.recoverableSuffixStartsAtCurrentCursor);
    const bool continuedRecoveredPrefixInsertAllowed =
        state.hasRecoveredPrefixBeforeCurrent &&
        ctx.currentWindowEditCount() > 0u && !hasLocalSequenceEdits &&
        visibleSourceAvailableForScopedContinuation &&
        sequenceFacts.strictSuffixStartsAtCurrentCursor &&
        (I != 0u || (!ctx.allowDelete &&
                     ctx.allowsScopedLeadingTerminalInsertRecovery()));
    const bool currentTerminalInsertionAllowed =
        currentAllowsSyntheticInsert &&
        !sequenceFacts.previousNullableSiblingOwnsCursor;
    return {
        currentTerminalInsertionAllowed &&
            (recoveredPrefixImmediateInsertAllowed ||
             continuedRecoveredPrefixInsertAllowed ||
             scopedLeadingInitialInsertAllowed ||
             scopedLeadingContinuationInsertAllowed ||
             committedPrefixContinuationInsertAllowed) &&
            (sequenceFacts.suffixFullyNullable ||
             sequenceFacts.strictSuffixStartsAtCurrentCursor ||
             sequenceFacts.recoverableSuffixStartsAtCurrentCursor),
        currentTerminalInsertionAllowed && I != 0u &&
            sequenceFacts.suffixFullyNullable &&
            state.hasRecoveredPrefixBeforeCurrent &&
            currentSiteWithinRecoveryWindow &&
            !(cursorStartsVisibleSource && ctx.currentWindowEditCount() > 0u),
    };
  }

  template <typename Current>
  [[nodiscard]] static constexpr bool
  allows_synthetic_terminal_insert(
      RecoveryContext &ctx, const Current &current,
      bool allowSequenceLiteralInsert = false) noexcept {
    const auto allowsEofLiteralInsert =
        [&ctx](detail::TerminalShape shape) constexpr noexcept {
          return ctx.cursor() >= ctx.end && shape.hasCanonicalText;
        };
    const auto allowsSequenceLiteralInsert =
        [allowSequenceLiteralInsert](
            detail::TerminalShape shape) constexpr noexcept {
          return allowSequenceLiteralInsert && shape.hasCanonicalText;
        };
    const auto allowsScopedCompactInsert =
        [&ctx](detail::TerminalShape shape) constexpr noexcept {
          return ctx.allowsScopedLeadingTerminalInsertRecovery() &&
                 shape.hasCanonicalText &&
                 shape.canonicalTextLength <=
                     detail::kMaxScopedSyntheticInsertCanonicalTextLength;
        };
    using CurrentType = std::remove_cvref_t<Current>;
    if constexpr (requires { CurrentType::terminalShape; }) {
      (void)current;
      return CurrentType::terminalShape.allowsInsert() ||
             allowsSequenceLiteralInsert(CurrentType::terminalShape) ||
             allowsEofLiteralInsert(CurrentType::terminalShape) ||
             allowsScopedCompactInsert(CurrentType::terminalShape);
    } else if constexpr (requires {
                           current._literalRecoveryMetadata;
                           current._terminalShape;
                         }) {
      if (current._literalRecoveryMetadata.has_value() &&
          !current._terminalShape.allowsInsert() &&
          !allowsSequenceLiteralInsert(current._terminalShape) &&
          !allowsEofLiteralInsert(current._terminalShape) &&
          !allowsScopedCompactInsert(current._terminalShape)) {
        return false;
      }
      return detail::allows_terminal_rule_insert(ctx,
                                                 current._terminalShape);
    }
    return true;
  }

  template <std::size_t I, typename Checkpoint>
  bool current_recoverable_entry_signal(RecoveryContext &ctx,
                                        const Checkpoint &checkpoint) const {
    const auto &current = std::get<I>(elements);
    ctx.rewind(checkpoint);
    detail::ProbeRestoreScope guard{ctx};
    return this->template with_current_follow_probe<I>(ctx, [&]() {
      return attempt_fast_probe(ctx, current) ||
             probe_recoverable_at_entry(current, ctx) ||
             probe_locally_recoverable(current, ctx);
    });
  }

  template <std::size_t I, typename Checkpoint>
  [[nodiscard]] std::pair<bool, bool>
  build_current_failure_terminal_legality(
      RecoveryContext &ctx, const Checkpoint &checkpoint,
      const TerminalRecoveryState &terminalRecoveryState,
      const SequenceFacts &terminalEntryFacts) const {
    const auto &current = std::get<I>(elements);
    if (current.getKind() == ElementKind::Literal ||
        current.getKind() == ElementKind::TerminalRule) {
      ctx.rewind(checkpoint);
      return build_terminal_sequence_legality<I>(
          ctx, terminalRecoveryState, terminalEntryFacts);
    }
    return {};
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  SequenceRecoveryReplayPlan select_current_failure_sequence_attempt(
      Context &ctx, const Checkpoint &checkpoint,
      const TerminalRecoveryState &terminalRecoveryState,
      bool allowInsertCurrentTerminal,
      bool allowInsertCurrentAfterDeleteRun,
      bool allowCurrentWithoutDeleteAttempt,
      bool allowSkipNullableAttempt,
      bool preferTailEntryInsert,
      bool allowTerminalNullableTailStop) const {
    const auto &current = std::get<I>(elements);
    SequenceRecoveryReplayPlan bestPlan{};
    detail::EditableRecoveryCandidate bestCandidate{};
    if (allowInsertCurrentTerminal || allowInsertCurrentAfterDeleteRun) {
      ctx.rewind(checkpoint);
      this->template consider_terminal_current_failure_attempts<I>(
          bestPlan, bestCandidate, ctx, allowInsertCurrentTerminal,
          allowInsertCurrentAfterDeleteRun, allowTerminalNullableTailStop);
    }
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      if (allowCurrentWithoutDeleteAttempt) {
        ctx.rewind(checkpoint);
        consider_sequence_recovery_candidate(
            bestPlan, bestCandidate,
            {.valid = true,
             .reparseCurrentWithoutDelete = true,
             .allowDeleteRecovery = true},
            evaluate_current_without_delete_attempt<Context, I>(
                ctx, checkpoint, terminalRecoveryState.facts,
                /*allowDeleteRecovery=*/true));
      }
      if (allowSkipNullableAttempt) {
        ctx.rewind(checkpoint);
        consider_sequence_recovery_candidate(
            bestPlan, bestCandidate,
            {.valid = true,
             .skipNullable = true,
             .preferTailEntryInsert = preferTailEntryInsert},
            evaluate_skip_nullable_attempt<Context, I>(
                ctx, checkpoint, preferTailEntryInsert));
      }
    }
    return bestPlan;
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  bool recover_after_current_parse_failure(
      Context &ctx, const Checkpoint &checkpoint,
      const TerminalRecoveryState &terminalRecoveryState,
      bool nullableCurrentLooksStarted,
      bool previousNullableSiblingOwnsCursor) const {
    const auto &current = std::get<I>(elements);
    const auto sequenceFacts =
        build_sequence_facts<I>(ctx, previousNullableSiblingOwnsCursor);
    const auto [allowInsertCurrentTerminal, allowInsertCurrentAfterDeleteRun] =
        this->template build_current_failure_terminal_legality<I>(
            ctx, checkpoint, terminalRecoveryState, sequenceFacts);
    const bool preferTailEntryInsert =
        sequenceFacts.recoverableSuffixStartsAtCurrentCursor;
    const bool allowTerminalNullableTailStop =
        terminal_allows_nullable_tail_stop(terminalRecoveryState, sequenceFacts);
    const bool allowSkipNullableAttempt =
        (!nullableCurrentLooksStarted ||
         sequenceFacts.strictSuffixStartsAtCurrentCursor ||
         sequenceFacts.recoverableSuffixStartsAtCurrentCursor) &&
        current.getKind() != ElementKind::AndPredicate &&
        current.getKind() != ElementKind::NotPredicate;
    bool allowCurrentWithoutDeleteAttempt = false;
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      if (nullableCurrentLooksStarted) {
        allowCurrentWithoutDeleteAttempt =
            !allowSkipNullableAttempt ||
            this->template current_recoverable_entry_signal<I>(
                ctx, checkpoint);
      }
    }
    if constexpr (!std::remove_cvref_t<decltype(std::get<I>(
                      elements))>::nullable) {
      if (allowInsertCurrentTerminal && !allowInsertCurrentAfterDeleteRun) {
        ctx.rewind(checkpoint);
        if (this->template replay_insert_current_terminal_attempt<I>(
                ctx, {}, allowTerminalNullableTailStop)) {
          return true;
        }
        ctx.rewind(checkpoint);
      }
    }
    const auto bestPlan =
        this->template select_current_failure_sequence_attempt<Context, I>(
            ctx, checkpoint, terminalRecoveryState,
            allowInsertCurrentTerminal,
            allowInsertCurrentAfterDeleteRun,
            allowCurrentWithoutDeleteAttempt,
            allowSkipNullableAttempt, preferTailEntryInsert,
            allowTerminalNullableTailStop);
    ctx.rewind(checkpoint);
    return replay_current_failure_sequence_attempt<Context, I>(
        ctx, terminalRecoveryState, bestPlan);
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  SequenceRecoveryReplayPlan select_tail_failure_sequence_attempt(
      Context &ctx, const Checkpoint &checkpoint,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts,
      bool preferTailEntryInsert,
      bool allowSkipNullableAttempt) const {
    SequenceRecoveryReplayPlan bestPlan{};
    detail::EditableRecoveryCandidate bestCandidate{};
    ctx.rewind(checkpoint);
    this->consider_sequence_recovery_candidate(
        bestPlan, bestCandidate,
        {.valid = true,
         .reparseCurrentWithoutDelete = true},
        evaluate_current_without_delete_attempt<Context, I>(
            ctx, checkpoint, terminalRecoveryFacts,
            /*allowDeleteRecovery=*/false));
    if (allowSkipNullableAttempt) {
      ctx.rewind(checkpoint);
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .skipNullable = true,
           .preferTailEntryInsert = preferTailEntryInsert},
          evaluate_skip_nullable_attempt<Context, I>(
              ctx, checkpoint, preferTailEntryInsert));
    }
    return bestPlan;
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  bool recover_after_tail_parse_failure(
      Context &ctx, const Checkpoint &checkpoint,
      const Checkpoint &checkpointAfterCurrent,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts) const {
    if constexpr (!std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      return false;
    } else {
      ctx.rewind(checkpointAfterCurrent);
      if (this->template replay_tail_after_current_attempt<Context, I>(ctx)) {
        return true;
      }
      ctx.rewind(checkpointAfterCurrent);
      const auto sequenceFacts = build_sequence_facts<I>(ctx);
      const bool preferTailEntryInsert =
          sequenceFacts.recoverableSuffixStartsAtCurrentCursor;
      const auto bestPlan =
          this->template select_tail_failure_sequence_attempt<Context, I>(
              ctx, checkpoint, terminalRecoveryFacts,
              preferTailEntryInsert,
              /*allowSkipNullableAttempt=*/
              sequenceFacts.recoverableSuffixStartsAtCurrentCursor);
      if (!bestPlan.valid) {
        ctx.rewind(checkpointAfterCurrent);
        return false;
      }
      ctx.rewind(checkpoint);
      if (replay_tail_failure_sequence_attempt<Context, I>(
              ctx, terminalRecoveryFacts, bestPlan)) {
        return true;
      }
      ctx.rewind(checkpointAfterCurrent);
      return false;
    }
  }


  template <std::size_t I>
  static constexpr bool suffix_after_is_fully_nullable() noexcept {
    if constexpr (I >= sizeof...(Elements)) {
      return true;
    } else {
      using Current = std::remove_cvref_t<decltype(std::get<I>(
          std::declval<const std::tuple<Elements...> &>()))>;
      if constexpr (!Current::nullable) {
        return false;
      } else {
        return suffix_after_is_fully_nullable<I + 1>();
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] constexpr const AbstractElement *
  first_required_tail_element() const noexcept {
    if constexpr (I >= sizeof...(Elements)) {
      return nullptr;
    } else {
      const auto &current = std::get<I>(elements);
      using Current = std::remove_cvref_t<decltype(current)>;
      if constexpr (!Current::nullable) {
        return std::addressof(current);
      } else {
        return first_required_tail_element<I + 1>();
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] constexpr bool
  tail_supports_single_terminal_recovery() const noexcept {
    if constexpr (I >= sizeof...(Elements)) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      using Current = std::remove_cvref_t<decltype(current)>;
      if constexpr (!Current::nullable) {
        return element_is_terminal_like_for_missing_insert(current) &&
               suffix_after_is_fully_nullable<I + 1>();
      } else {
        return tail_supports_single_terminal_recovery<I + 1>();
      }
    }
  }

  /// Encapsulates the `SkipNullable` transition. Returns `true` iff
  /// the skip succeeded; returns `false` when the current is non-nullable,
  /// already started visibly, the strict suffix entry trigger is not
  /// available, or the skip attempt failed (in which case the cursor
  /// was rewound).
  template <ParseModeContext Context, std::size_t I>
  [[nodiscard]] bool
  try_skip_nullable_transition(Context &ctx,
                                bool nullableCurrentLooksStarted,
                                bool strictSuffixStartsAtEntry) const {
    if constexpr (!std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      (void)ctx;
      (void)nullableCurrentLooksStarted;
      (void)strictSuffixStartsAtEntry;
      return false;
    } else {
      if (nullableCurrentLooksStarted || !strictSuffixStartsAtEntry) {
        return false;
      }
      const auto skipNullableCheckpoint = ctx.mark();
      const auto &current = std::get<I>(elements);
      const bool matched =
          this->template nullable_element_allows_recovery_skip<I>()
              ? this->template parse_elements<Context, I + 1>(ctx)
              : (parse(current, ctx) &&
                 this->template parse_elements<Context, I + 1>(ctx));
      if (matched) {
        return true;
      }
      ctx.rewind(skipNullableCheckpoint);
      return false;
    }
  }

  template <std::size_t I>
  [[nodiscard]] bool
  select_missing_element_insert_replay(RecoveryContext &ctx) const {
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (element_is_terminal_like_for_missing_insert(current)) {
        return false;
      }
      if constexpr (requires { current.getElement(); }) {
        const auto &inner = *current.getElement();
        if (inner.getKind() == ElementKind::Literal) {
          return false;
        }
      }
      const bool currentMatchesStrict =
          detail::attempt_parse_without_side_effects(ctx, current);
      if (currentMatchesStrict) {
        if constexpr (I + 1 == sizeof...(Elements)) {
          return false;
        }
        const auto suffixEntryFacts =
            this->template analyze_suffix_entry_facts<I + 1>(ctx);
        if (!suffixEntryFacts.strictStartsAtCurrentCursor) {
          return false;
        }
      }
      const auto checkpoint = ctx.mark();
      const auto baseEditCost = ctx.currentEditCost();
      const auto baseRecoveryEditCount = ctx.recoveryEditCount();
      bool insertMatchedCleanTail = false;
      const auto insertCandidate = detail::evaluate_editable_recovery_candidate(
          ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
          [this, &ctx, &insertMatchedCleanTail]() {
            return this->template replay_insert_missing_element_attempt<I>(
                ctx, insertMatchedCleanTail);
          });
      if (!insertCandidate.matched ||
          insertCandidate.postSkipCursorOffset <=
              insertCandidate.firstEditOffset) {
        return false;
      }
      const bool singleInsertedElementWithCleanTail =
          insertMatchedCleanTail && insertCandidate.editCount == 1u;
      const auto parseCandidate = detail::evaluate_editable_recovery_candidate(
          ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
          [this, &ctx, &current, singleInsertedElementWithCleanTail]() {
            const auto editGuard =
                singleInsertedElementWithCleanTail
                    ? ctx.withEditPermissions(ctx.allowInsert, false)
                    : ctx.withEditPermissions(ctx.allowInsert, ctx.allowDelete);
            (void)editGuard;
            const bool previousAllowDeleteRetry = ctx.allowDeleteRetry;
            if (singleInsertedElementWithCleanTail) {
              ctx.allowDeleteRetry = false;
            }
            const bool matched =
                this->template parse_current_sequence_element<I>(ctx, {}) &&
                this->template parse_elements<RecoveryContext, I + 1>(ctx);
            ctx.allowDeleteRetry = previousAllowDeleteRetry;
            return matched;
          });
      const bool selectInsertReplay =
          !parseCandidate.matched ||
          detail::is_better_recovery_key(
              detail::editable_recovery_key(insertCandidate),
              detail::editable_recovery_key(parseCandidate));
      return selectInsertReplay;
    }
  }

  template <std::size_t I>
  bool replay_insert_missing_element_attempt(RecoveryContext &ctx,
                                             bool &matchedCleanTail) const {
    const auto &current = std::get<I>(elements);
    matchedCleanTail = false;
    if (!ctx.insertSynthetic(std::addressof(current))) {
      return false;
    }
    if (this->template parse_clean_tail_without_edits<I + 1>(ctx)) {
      matchedCleanTail = true;
      return true;
    }
    if (!this->template tail_supports_single_terminal_recovery<I + 1>()) {
      return false;
    }
    bool tailRecoverable = false;
    {
      detail::ProbeRestoreScope guard{ctx};
      tailRecoverable =
          this->template probe_recoverable_entry_elements<I + 1>(ctx);
    }
    return tailRecoverable &&
           this->template parse_elements<RecoveryContext, I + 1>(ctx);
  }

  template <std::size_t I>
  bool parse_clean_tail_without_edits(RecoveryContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      return attempt_parse_no_edits(ctx, std::get<I>(elements)) &&
             this->template parse_clean_tail_without_edits<I + 1>(ctx);
    }
  }

  template <std::size_t I>
  bool try_insert_current_terminal_with_clean_tail(
      RecoveryContext &ctx, bool allowNullableTailStop) const {
    const auto &current = std::get<I>(elements);
    const auto checkpoint = ctx.mark();
    const auto parseStartOffset = ctx.cursorOffset();
    auto allowLocalInsertGuard =
        ctx.withEditPermissions(true, ctx.allowDelete);
    (void)allowLocalInsertGuard;
    const bool inserted = ctx.insertSynthetic(std::addressof(current));
    if (!inserted) {
      ctx.rewind(checkpoint);
      return false;
    }
    ctx.leaf(ctx.cursor(), std::addressof(current), false, true);
    const auto editCountAfterCurrent = ctx.recoveryEditCount();
    const auto editCostAfterCurrent = ctx.currentEditCost();
    const bool tailMatched =
        this->template parse_clean_tail_without_edits<I + 1>(ctx);
    if (!tailMatched || ctx.recoveryEditCount() != editCountAfterCurrent ||
        ctx.currentEditCost() != editCostAfterCurrent) {
      ctx.rewind(checkpoint);
      return false;
    }
    const auto postMatchCheckpoint = ctx.mark();
    ctx.skip();
    const auto postSkipCursorOffset = ctx.cursorOffset();
    ctx.rewind(postMatchCheckpoint);
    if (!allowNullableTailStop) {
      if (postSkipCursorOffset <= parseStartOffset) {
        ctx.rewind(checkpoint);
        return false;
      }
    }
    return true;
  }

  template <std::size_t I>
  [[nodiscard]] std::optional<TerminalDeleteRunObservation>
  observe_current_terminal_delete_run(RecoveryContext &ctx) const {
    const auto checkpoint = ctx.mark();
    const bool previousSkipAfterDelete = ctx.skipAfterDelete;
    const char *const cursorStart = ctx.cursor();
    ctx.skipAfterDelete = false;
    std::optional<TerminalDeleteRunObservation> observation;
    (void)detail::visit_guarded_delete_scan_positions(
        ctx,
        [&ctx, cursorStart]() noexcept {
          return detail::allows_extended_terminal_delete_scan_match(
              ctx, cursorStart, ctx.cursor());
        },
        [&observation](const detail::DeleteScanVisitState &state) {
          observation = {
              .deleteCount = state.deleteCount,
              .endsAfterHiddenTriviaExtension = state.hiddenTriviaExtended,
          };
          return detail::DeleteScanVisitResult::Continue;
        },
        {.extendThroughHiddenTrivia = true,
         .stopAtHiddenTriviaBoundary = true,
         .visitAfterHiddenTriviaExtension = true});
    ctx.skipAfterDelete = previousSkipAfterDelete;
    ctx.rewind(checkpoint);
    return observation;
  }

  template <std::size_t I>
  bool replay_current_terminal_delete_run(
      RecoveryContext &ctx,
      const TerminalDeleteRunObservation &observation) const {
    if (observation.deleteCount == 0u) {
      return true;
    }
    const bool previousSkipAfterDelete = ctx.skipAfterDelete;
    const char *const cursorStart = ctx.cursor();
    ctx.skipAfterDelete = false;
    const bool replayed =
        detail::visit_guarded_delete_scan_positions(
            ctx,
            [&ctx, cursorStart]() noexcept {
              return detail::allows_extended_terminal_delete_scan_match(
                  ctx, cursorStart, ctx.cursor());
            },
            [&observation](const detail::DeleteScanVisitState &state) {
              if (state.deleteCount == observation.deleteCount &&
                  state.hiddenTriviaExtended ==
                      observation.endsAfterHiddenTriviaExtension) {
                return detail::DeleteScanVisitResult::Accept;
              }
              return detail::DeleteScanVisitResult::Continue;
            },
            {.scan = {.maxDeletes = observation.deleteCount},
             .extendThroughHiddenTrivia = true,
             .stopAtHiddenTriviaBoundary = true,
             .visitAfterHiddenTriviaExtension = true}) ==
        detail::DeleteScanVisitResult::Accept;
    ctx.skipAfterDelete = previousSkipAfterDelete;
    return replayed;
  }

  template <std::size_t I>
  bool replay_insert_current_terminal_attempt(
      RecoveryContext &ctx, const TerminalDeleteRunObservation &deleteRun,
      bool allowNullableTailStop) const {
    const auto checkpoint = ctx.mark();
    if (!this->template replay_current_terminal_delete_run<I>(ctx, deleteRun)) {
      ctx.rewind(checkpoint);
      return false;
    }
    if (this->template try_insert_current_terminal_with_clean_tail<I>(
            ctx, allowNullableTailStop)) {
      return true;
    }
    ctx.rewind(checkpoint);
    return false;
  }

  template <std::size_t I>
  void consider_terminal_current_failure_attempts(
      SequenceRecoveryReplayPlan &bestPlan,
      detail::EditableRecoveryCandidate &bestCandidate,
      RecoveryContext &ctx, bool allowInsertCurrentTerminal,
      bool allowInsertCurrentAfterDeleteRun,
      bool allowNullableTailStop) const {
    if (!allowInsertCurrentTerminal && !allowInsertCurrentAfterDeleteRun) {
      return;
    }
    const auto &current = std::get<I>(elements);
    const auto checkpoint = ctx.mark();
    const auto baseEditCost = ctx.currentEditCost();
    const auto baseRecoveryEditCount = ctx.recoveryEditCount();
    if (allowInsertCurrentTerminal) {
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .insertCurrentTerminal = true,
           .allowNullableTailStop = allowNullableTailStop},
          detail::evaluate_editable_recovery_candidate(
              ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
              [this, &ctx, allowNullableTailStop]() {
                return this->template replay_insert_current_terminal_attempt<I>(
                    ctx, {}, allowNullableTailStop);
              }));
    }
    if (allowInsertCurrentAfterDeleteRun) {
      const auto deleteRun =
          this->template observe_current_terminal_delete_run<I>(ctx);
      if (!deleteRun.has_value()) {
        return;
      }
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .insertCurrentTerminal = true,
           .currentTerminalDeleteRunCount = deleteRun->deleteCount,
           .currentTerminalDeleteRunEndsAfterHiddenTriviaExtension =
               deleteRun->endsAfterHiddenTriviaExtension,
           .allowNullableTailStop = allowNullableTailStop},
          detail::evaluate_editable_recovery_candidate(
              ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
              [this, &ctx, deleteRun, allowNullableTailStop]() {
                return this->template replay_insert_current_terminal_attempt<I>(
                    ctx, *deleteRun, allowNullableTailStop);
              }));
    }
  }

  template <std::size_t I>
  bool replay_terminal_current_failure_attempt(
      RecoveryContext &ctx,
      const SequenceRecoveryReplayPlan &plan) const {
    if (!plan.valid) {
      return false;
    }
    if (plan.insertCurrentTerminal) {
      return this->template replay_insert_current_terminal_attempt<I>(
          ctx,
          {.deleteCount = plan.currentTerminalDeleteRunCount,
           .endsAfterHiddenTriviaExtension =
               plan.currentTerminalDeleteRunEndsAfterHiddenTriviaExtension},
          plan.allowNullableTailStop);
    }
    return false;
  }

  template <std::size_t I>
  void init_elements(AstReflectionInitContext &ctx) const {
    if constexpr (I < sizeof...(Elements)) {
      parser::init(std::get<I>(elements), ctx);
      init_elements<I + 1>(ctx);
    }
  }

  template <std::size_t... Is>
  const AbstractElement *get_impl(std::size_t elementIndex,
                                  std::index_sequence<Is...>) const noexcept {
    using AccessorFn = const AbstractElement *(*)(const Group *) noexcept;

    static constexpr std::array<AccessorFn, sizeof...(Elements)> accessors = {
        +[](const Group *self) noexcept -> const AbstractElement * {
          return std::addressof(std::get<Is>(self->elements));
        }...};

    return accessors[elementIndex](this);
  }

  template <std::size_t I = 0>
  constexpr const char *terminal_impl(const char *cursor) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    if constexpr (I == sizeof...(Elements)) {
      return cursor;
    } else {
      const char *matchEnd = std::get<I>(elements).terminal(cursor);
      return matchEnd != nullptr ? terminal_impl<I + 1>(matchEnd)
                                 : nullptr;
    }
  }
};

template <Expression... Elements>
using GroupWithSkipper = SkipperWrapped<Group<Elements...>>;

namespace detail {

template <typename T> struct IsGroupRaw : std::false_type {};
template <Expression... E> struct IsGroupRaw<Group<E...>> : std::true_type {};

template <typename G>
  requires IsGroupRaw<std::remove_cvref_t<G>>::value
constexpr decltype(auto) as_group_tuple(G &&group) {
  return std::forward<G>(group).elements;
}

template <Expression Expr>
  requires(!IsGroupRaw<std::remove_cvref_t<Expr>>::value)
constexpr auto as_group_tuple(Expr &&expr) {
  return std::tuple<ExpressionHolder<Expr>>{std::forward<Expr>(expr)};
}

template <typename... Ts>
constexpr auto make_group(std::tuple<Ts...> &&elements) {
  return Group<Ts...>{std::move(elements)};
}

} // namespace detail

template <Expression Lhs, Expression Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return detail::make_group(std::tuple_cat(
      detail::as_group_tuple(std::forward<Lhs>(lhs)),
      detail::as_group_tuple(std::forward<Rhs>(rhs))));
}
} // namespace pegium::parser
