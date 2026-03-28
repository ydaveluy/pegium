#pragma once

/// Parser expression representing an ordered sequence of child expressions.

#include <array>
#include <concepts>
#include <cstdint>
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
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/TextUtils.hpp>
#include <string>
#include <string_view>

namespace pegium::parser {

template <Expression... Elements> struct GroupWithSkipper;

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

  [[nodiscard]] constexpr bool isWordLike() const noexcept {
    return is_word_like_impl(std::make_index_sequence<sizeof...(Elements)>{});
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
    return GroupWithSkipper<Elements...>{
        elements,
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return GroupWithSkipper<Elements...>{
        std::move(elements),
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

  template <ParseModeContext Context, std::size_t I>
  bool parse_elements(Context &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      const char *cursorBeforeSkip = nullptr;
      if constexpr (I > 0) {
        cursorBeforeSkip = ctx.cursor();
        ctx.skip();
      }
      if constexpr (StrictParseModeContext<Context>) {
        if (!parse(std::get<I>(elements), ctx)) {
          return false;
        }
        return parse_elements<Context, I + 1>(ctx);
      } else if constexpr (RecoveryParseModeContext<Context>) {
        if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows()) {
          if (TrackedParseContext &strictCtx = ctx;
              !parse(std::get<I>(elements), strictCtx)) {
            return false;
          }
          return parse_elements<Context, I + 1>(ctx);
        }
        if (try_insert_missing_element<I>(ctx)) {
          return true;
        }
        const auto checkpoint = ctx.mark();
        const auto &current = std::get<I>(elements);
        const bool nullableCurrentLooksStarted =
            [&]() {
              if constexpr (!std::remove_cvref_t<
                                decltype(std::get<I>(elements))>::nullable) {
                return false;
              } else {
                if (current.getKind() == ElementKind::AndPredicate ||
                    current.getKind() == ElementKind::NotPredicate) {
                  return false;
                }
                const auto probeCheckpoint = ctx.mark();
                const char *const savedFurthestExploredCursor =
                    ctx.furthestExploredCursor();
                const bool started = attempt_fast_probe(ctx, current);
                ctx.rewind(probeCheckpoint);
                ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
                return started;
              }
            }();
        const auto terminalRecoveryState =
            (current.getKind() == ElementKind::Literal ||
             current.getKind() == ElementKind::TerminalRule)
                ? build_terminal_recovery_state<I>(ctx, checkpoint, cursorBeforeSkip)
                : TerminalRecoveryState{};
        if constexpr (std::remove_cvref_t<
                          decltype(std::get<I>(elements))>::nullable) {
          if (!nullableCurrentLooksStarted &&
              this->template suffix_starts_without_edits<I + 1>(ctx)) {
            const auto skipNullableCheckpoint = ctx.mark();
            const bool matched =
                this->template nullable_element_allows_recovery_skip<I>()
                    ? this->template parse_elements<Context, I + 1>(ctx)
                    : (parse(current, ctx) &&
                       this->template parse_elements<Context, I + 1>(ctx));
            if (matched) {
              return true;
            }
            ctx.rewind(skipNullableCheckpoint);
          }
        }
        if (!parse_current_sequence_element<I>(ctx, terminalRecoveryState.facts)) {
          return recover_after_current_parse_failure<Context, I>(
              ctx, checkpoint, terminalRecoveryState, nullableCurrentLooksStarted);
        }
        const auto checkpointAfterCurrent = ctx.mark();
        const bool currentCommittedProgress =
            ctx.cursor() != checkpoint.parseCheckpoint.parseCheckpoint.cursor ||
            ctx.currentEditCount() !=
                checkpoint.recoveryState.editBudget.editCount ||
            ctx.currentEditCost() !=
                checkpoint.recoveryState.editBudget.editCost;
        if (parse_elements<Context, I + 1>(ctx)) {
          return true;
        }
        return recover_after_tail_parse_failure<Context, I>(
            ctx, checkpoint, checkpointAfterCurrent, terminalRecoveryState.facts,
            currentCommittedProgress);
      } else {
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
    }
  }

private:
  struct SequenceRecoveryReplayPlan {
    bool valid = false;
    bool insertCurrentWithCleanTail = false;
    bool deleteThenInsertCurrent = false;
    bool reparseCurrentWithoutDelete = false;
    bool skipNullable = false;
    bool preferTailEntryInsert = false;
    bool allowDeleteRecovery = false;
    bool allowNullableTailStop = false;
  };

  struct TerminalRecoveryState {
    detail::TerminalRecoveryFacts facts{};
    bool hasRecoveredPrefixBeforeCurrent = false;
    std::uint32_t sequenceEditCountBase = 0u;
  };

  struct SequenceFacts {
    bool suffixFullyNullable = false;
    bool strictSuffixStartsAtCurrentCursor = false;
    bool recoverableSuffixStartsAtCurrentCursor = false;
    bool currentOffsetWithinRecoveryWindow = false;
  };

  struct SequenceSuffixEntryFacts {
    bool strictStartsAtCurrentCursor = false;
    bool recoverableStartsAtCurrentCursor = false;
  };

  static void consider_sequence_recovery_candidate(
      SequenceRecoveryReplayPlan &bestPlan,
      detail::EditableRecoveryCandidate &bestCandidate,
      const SequenceRecoveryReplayPlan &candidatePlan,
      const detail::EditableRecoveryCandidate &candidate,
      const AbstractElement *preferredFirstEditElement) noexcept {
    if (!candidate.matched) {
      return;
    }
    if (!bestCandidate.matched ||
        detail::is_better_choice_recovery_candidate(
            candidate, bestCandidate,
            {.preferredBoundaryElement = preferredFirstEditElement})) {
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
      return current.getKind() != ElementKind::Create;
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
      if (attempt_fast_probe(ctx, current)) {
        return true;
      }
      if constexpr (std::remove_cvref_t<decltype(current)>::nullable) {
        return probe_recoverable_suffix_after_missing_current<I + 1>(ctx);
      } else {
        return false;
      }
    }
  }

  template <std::size_t I>
  [[nodiscard]] SequenceFacts
  build_sequence_facts(RecoveryContext &ctx) const {
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
            ctx.cursorOffset() <= ctx.pendingRecoveryWindowMaxCursorOffset(),
    };
  }

  template <std::size_t I, typename Checkpoint>
  [[nodiscard]] TerminalRecoveryState build_terminal_recovery_state(
      const RecoveryContext &ctx, const Checkpoint &checkpoint,
      const char *cursorBeforeSkip) const {
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
    return {
        .facts =
            {
                .triviaGap = triviaGap,
                .previousElementIsTerminalish = previousIsTerminalish,
                .allowStructuredVisibleContinuationInsert =
                    hasCommittedVisiblePrefixBeforeCurrent,
            },
        .hasRecoveredPrefixBeforeCurrent = hasRecoveredPrefixBeforeCurrent,
        .sequenceEditCountBase = checkpoint.recoveryState.editBudget.editCount,
    };
  }

  template <std::size_t I>
  bool parse_current_sequence_element(
      RecoveryContext &ctx, const detail::TerminalRecoveryFacts &facts) const {
    const auto &current = std::get<I>(elements);
    if constexpr (requires { current.parse_terminal_recovery_impl(ctx, facts); }) {
      return current.parse_terminal_recovery_impl(ctx, facts);
    } else {
      return parse(current, ctx);
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
                 : (parse(current, ctx) && parseTail());
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
                 : (parse(current, ctx) && parseTail());
    }
    auto leadingInsertGuard = ctx.withLeadingTerminalInsertScope();
    (void)leadingInsertGuard;
    return this->template nullable_element_allows_recovery_skip<I>()
               ? parseTail()
               : (parse(current, ctx) && parseTail());
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
    auto noDeleteGuard = ctx.withEditPermissions(ctx.allowInsert, false);
    (void)noDeleteGuard;
    return this->template parse_current_sequence_element<I>(
               ctx, terminalRecoveryFacts) &&
           this->template parse_elements<Context, I + 1>(ctx);
  }

  template <RecoveryParseModeContext Context, std::size_t I>
  bool replay_current_failure_sequence_attempt(
      Context &ctx, const TerminalRecoveryState &terminalRecoveryState,
      const SequenceRecoveryReplayPlan &plan) const {
    if (!plan.valid) {
      return false;
    }
    if (plan.insertCurrentWithCleanTail || plan.deleteThenInsertCurrent) {
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
    const bool currentAllowsSyntheticInsert =
        allows_synthetic_terminal_insert(ctx, current);
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
          sequenceFacts.currentOffsetWithinRecoveryWindow) ||
         (state.hasRecoveredPrefixBeforeCurrent &&
          sequenceFacts.suffixFullyNullable &&
          (state.facts.triviaGap.hasHiddenGap() ||
           cursorStartsVisibleSource))) &&
        (state.facts.triviaGap.hasHiddenGap() || !hasLocalSequenceEdits);
    const bool scopedLeadingContinuationInsertAllowed =
        I == 0u && ctx.allowsScopedLeadingTerminalInsertRecovery() &&
        !ctx.allowDelete && sequenceFacts.currentOffsetWithinRecoveryWindow &&
        visibleSourceAvailableForScopedContinuation;
    const bool continuedRecoveredPrefixInsertAllowed =
        state.hasRecoveredPrefixBeforeCurrent &&
        ctx.currentWindowEditCount() > 0u && !hasLocalSequenceEdits &&
        visibleSourceAvailableForScopedContinuation &&
        sequenceFacts.strictSuffixStartsAtCurrentCursor &&
        (I != 0u || (!ctx.allowDelete &&
                     ctx.allowsScopedLeadingTerminalInsertRecovery()));
    return {
        currentAllowsSyntheticInsert &&
            (recoveredPrefixImmediateInsertAllowed ||
             continuedRecoveredPrefixInsertAllowed ||
             scopedLeadingInitialInsertAllowed ||
             scopedLeadingContinuationInsertAllowed) &&
            (sequenceFacts.suffixFullyNullable ||
             sequenceFacts.strictSuffixStartsAtCurrentCursor ||
             sequenceFacts.recoverableSuffixStartsAtCurrentCursor),
        currentAllowsSyntheticInsert && I != 0u &&
            sequenceFacts.suffixFullyNullable &&
            state.hasRecoveredPrefixBeforeCurrent &&
            sequenceFacts.currentOffsetWithinRecoveryWindow &&
            !(cursorStartsVisibleSource && ctx.currentWindowEditCount() > 0u),
    };
  }

  template <std::size_t I>
  [[nodiscard]] const AbstractElement *
  preferred_tail_failure_first_edit_element(
      bool currentCommittedProgress,
      const SequenceFacts &sequenceFacts) const noexcept {
    const auto &current = std::get<I>(elements);
    const auto *preferredFirstEditElement =
        (!currentCommittedProgress ||
         sequenceFacts.recoverableSuffixStartsAtCurrentCursor)
            ? first_required_tail_element<I + 1>()
            : std::addressof(current);
    return preferredFirstEditElement != nullptr ? preferredFirstEditElement
                                                : std::addressof(current);
  }

  template <typename Current>
  [[nodiscard]] static constexpr bool
  allows_synthetic_terminal_insert(RecoveryContext &ctx,
                                   const Current &current) noexcept {
    if constexpr (requires { current._lexicalRecoveryProfile; }) {
      return detail::allows_terminal_rule_insert(ctx,
                                                 current._lexicalRecoveryProfile);
    } else if constexpr (requires {
                           { current.isWordLike() } -> std::convertible_to<bool>;
                         }) {
      if (current.isWordLike()) {
        return !detail::cursor_starts_visible_source(ctx);
      }
    }
    return true;
  }

  template <std::size_t I, typename Checkpoint>
  bool current_locally_recoverable_from_entry(RecoveryContext &ctx,
                                              const Checkpoint &checkpoint) const {
    const auto &current = std::get<I>(elements);
    ctx.rewind(checkpoint);
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    const bool recoverable = attempt_fast_probe(ctx, current) ||
                             probe_recoverable_at_entry(current, ctx) ||
                             probe_locally_recoverable(current, ctx);
    ctx.rewind(checkpoint);
    ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    return recoverable;
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

  template <std::size_t I>
  [[nodiscard]] bool current_failure_allows_skip_nullable_attempt(
      bool nullableCurrentLooksStarted,
      const SequenceFacts &sequenceFacts) const {
    const auto &current = std::get<I>(elements);
    return (!nullableCurrentLooksStarted ||
            sequenceFacts.strictSuffixStartsAtCurrentCursor ||
            sequenceFacts.recoverableSuffixStartsAtCurrentCursor) &&
           current.getKind() != ElementKind::AndPredicate &&
           current.getKind() != ElementKind::NotPredicate;
  }

  template <std::size_t I, typename Checkpoint>
  [[nodiscard]] bool current_failure_allows_reparse_current_without_delete(
      RecoveryContext &ctx, const Checkpoint &checkpoint,
      bool nullableCurrentLooksStarted,
      bool allowSkipNullableAttempt) const {
    if (!nullableCurrentLooksStarted) {
      return false;
    }
    if (!allowSkipNullableAttempt) {
      return true;
    }
    return this->template current_locally_recoverable_from_entry<I>(ctx,
                                                                    checkpoint);
  }

  template <std::size_t I, typename Checkpoint>
  [[nodiscard]] bool tail_failure_allows_reparse_current_attempt(
      RecoveryContext &ctx, const Checkpoint &checkpoint,
      bool currentCommittedProgress,
      const SequenceFacts &sequenceFacts) const {
    const bool currentLocallyRecoverableFromEntry =
        !currentCommittedProgress &&
        this->template current_locally_recoverable_from_entry<I>(
            ctx, checkpoint);
    return currentCommittedProgress ||
           sequenceFacts.strictSuffixStartsAtCurrentCursor ||
           currentLocallyRecoverableFromEntry;
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  SequenceRecoveryReplayPlan select_current_failure_sequence_attempt(
      Context &ctx, const Checkpoint &checkpoint,
      const TerminalRecoveryState &terminalRecoveryState,
      bool allowInsertCurrentWithCleanTail,
      bool allowDeleteThenInsertCurrent,
      bool allowCurrentWithoutDeleteAttempt,
      bool allowSkipNullableAttempt,
      bool preferTailEntryInsert,
      bool allowTerminalNullableTailStop) const {
    const auto &current = std::get<I>(elements);
    SequenceRecoveryReplayPlan bestPlan{};
    detail::EditableRecoveryCandidate bestCandidate{};
    if (allowInsertCurrentWithCleanTail || allowDeleteThenInsertCurrent) {
      ctx.rewind(checkpoint);
      this->template consider_terminal_current_failure_attempts<I>(
          bestPlan, bestCandidate, ctx, allowInsertCurrentWithCleanTail,
          allowDeleteThenInsertCurrent, allowTerminalNullableTailStop);
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
                /*allowDeleteRecovery=*/true),
            std::addressof(current));
      }
      if (allowSkipNullableAttempt) {
        ctx.rewind(checkpoint);
        consider_sequence_recovery_candidate(
            bestPlan, bestCandidate,
            {.valid = true,
             .skipNullable = true,
             .preferTailEntryInsert = preferTailEntryInsert},
            evaluate_skip_nullable_attempt<Context, I>(
                ctx, checkpoint, preferTailEntryInsert),
            std::addressof(current));
      }
    }
    return bestPlan;
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  bool recover_after_current_parse_failure(
      Context &ctx, const Checkpoint &checkpoint,
      const TerminalRecoveryState &terminalRecoveryState,
      bool nullableCurrentLooksStarted) const {
    const auto sequenceFacts = build_sequence_facts<I>(ctx);
    const auto [allowInsertCurrentWithCleanTail,
                allowDeleteThenInsertCurrent] =
        this->template build_current_failure_terminal_legality<I>(
            ctx, checkpoint, terminalRecoveryState, sequenceFacts);
    const bool preferTailEntryInsert =
        sequenceFacts.recoverableSuffixStartsAtCurrentCursor;
    const bool allowTerminalNullableTailStop =
        terminal_allows_nullable_tail_stop(terminalRecoveryState, sequenceFacts);
    const bool allowSkipNullableAttempt =
        this->template current_failure_allows_skip_nullable_attempt<I>(
            nullableCurrentLooksStarted, sequenceFacts);
    bool allowCurrentWithoutDeleteAttempt = false;
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      allowCurrentWithoutDeleteAttempt =
          this->template current_failure_allows_reparse_current_without_delete<I>(
              ctx, checkpoint, nullableCurrentLooksStarted,
              allowSkipNullableAttempt);
    }
    const auto bestPlan =
        this->template select_current_failure_sequence_attempt<Context, I>(
            ctx, checkpoint, terminalRecoveryState,
            allowInsertCurrentWithCleanTail,
            allowDeleteThenInsertCurrent,
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
      const AbstractElement *preferredFirstEditElement,
      bool preferTailEntryInsert, bool allowDeleteRecovery,
      bool allowReparseCurrentAttempt,
      bool allowSkipNullableAttempt) const {
    SequenceRecoveryReplayPlan bestPlan{};
    detail::EditableRecoveryCandidate bestCandidate{};
    if (allowReparseCurrentAttempt) {
      ctx.rewind(checkpoint);
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .reparseCurrentWithoutDelete = true,
           .allowDeleteRecovery = allowDeleteRecovery},
          evaluate_current_without_delete_attempt<Context, I>(
              ctx, checkpoint, terminalRecoveryFacts,
              allowDeleteRecovery),
          preferredFirstEditElement);
    }
    if (allowSkipNullableAttempt) {
      ctx.rewind(checkpoint);
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .skipNullable = true,
           .preferTailEntryInsert = preferTailEntryInsert},
          evaluate_skip_nullable_attempt<Context, I>(
              ctx, checkpoint, preferTailEntryInsert),
          preferredFirstEditElement);
    }
    return bestPlan;
  }

  template <RecoveryParseModeContext Context, std::size_t I,
            typename Checkpoint>
  bool recover_after_tail_parse_failure(
      Context &ctx, const Checkpoint &checkpoint,
      const Checkpoint &checkpointAfterCurrent,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts,
      bool currentCommittedProgress) const {
    if constexpr (!std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      return false;
    } else {
      const auto sequenceFacts = build_sequence_facts<I>(ctx);
      const auto *preferredFirstEditElement =
          this->template preferred_tail_failure_first_edit_element<I>(
              currentCommittedProgress, sequenceFacts);
      const bool preferTailEntryInsert =
          sequenceFacts.recoverableSuffixStartsAtCurrentCursor;
      const bool allowDeleteRecovery = !currentCommittedProgress;
      const bool allowReparseCurrentAttempt =
          this->template tail_failure_allows_reparse_current_attempt<I>(
              ctx, checkpoint, currentCommittedProgress, sequenceFacts);
      const bool allowSkipNullableAttempt =
          sequenceFacts.recoverableSuffixStartsAtCurrentCursor ||
          (!currentCommittedProgress && !allowReparseCurrentAttempt);
      const auto bestPlan =
          this->template select_tail_failure_sequence_attempt<Context, I>(
              ctx, checkpoint, terminalRecoveryFacts,
              preferredFirstEditElement, preferTailEntryInsert,
              allowDeleteRecovery, allowReparseCurrentAttempt,
              allowSkipNullableAttempt);
      if (!bestPlan.valid &&
          currentCommittedProgress) {
        ctx.rewind(checkpointAfterCurrent);
        return false;
      }
      ctx.rewind(checkpoint);
      if (replay_tail_failure_sequence_attempt<Context, I>(
              ctx, terminalRecoveryFacts, bestPlan)) {
        return true;
      }
      if (currentCommittedProgress) {
        ctx.rewind(checkpointAfterCurrent);
      }
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

  template <std::size_t I>
  bool try_insert_missing_element(RecoveryContext &ctx) const {
    if constexpr (std::remove_cvref_t<
                      decltype(std::get<I>(elements))>::nullable) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      if (element_is_terminal_like_for_missing_insert(current)) {
        return false;
      }
      if constexpr (requires { current.getElement(); }) {
        if (const auto *inner = current.getElement();
            inner != nullptr &&
            element_is_terminal_like_for_missing_insert(*inner)) {
          return false;
        }
      }
      if (detail::attempt_parse_without_side_effects(ctx, current)) {
        return false;
      }
      const auto checkpoint = ctx.mark();
      const auto baseEditCost = ctx.currentEditCost();
      const auto baseRecoveryEditCount = ctx.recoveryEditCount();
      const auto replayInsertAttempt =
          [this, &ctx, &current](bool &matchedCleanTail) {
            matchedCleanTail = false;
            if (!detail::apply_insert_synthetic_recovery_edit(
                    ctx, std::addressof(current))) {
              return false;
            }
            if (this->template parse_clean_tail_without_edits<I + 1>(ctx)) {
              matchedCleanTail = true;
              return true;
            }
            if (!this->template tail_supports_single_terminal_recovery<I + 1>()) {
              return false;
            }
            const auto tailCheckpoint = ctx.mark();
            const char *const savedFurthestExploredCursor =
                ctx.furthestExploredCursor();
            const bool tailRecoverable =
                this->template probe_recoverable_entry_elements<I + 1>(ctx);
            ctx.rewind(tailCheckpoint);
            ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
            return tailRecoverable &&
                   this->template parse_elements<RecoveryContext, I + 1>(ctx);
          };
      bool insertMatchedCleanTail = false;
      const auto insertCandidate = detail::evaluate_editable_recovery_candidate(
          ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
          [&replayInsertAttempt, &insertMatchedCleanTail]() {
            return replayInsertAttempt(insertMatchedCleanTail);
          });
      if (!insertCandidate.matched ||
          insertCandidate.postSkipCursorOffset <=
              insertCandidate.firstEditOffset) {
        return false;
      }
      const bool singleInsertedElementWithCleanTail =
          insertMatchedCleanTail && insertCandidate.editCount == 1u &&
          insertCandidate.editSpan == 0;
      const bool leadingMissingElementSupportedOnlyBySingleTerminalTail =
          I == 0u && insertMatchedCleanTail &&
          tail_supports_single_terminal_recovery<I + 1>();
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
                parse(current, ctx) &&
                this->template parse_elements<RecoveryContext, I + 1>(ctx);
            ctx.allowDeleteRetry = previousAllowDeleteRetry;
            return matched;
          });
      if (parseCandidate.matched && parseCandidate.editCost == 0 &&
          parseCandidate.editCount == 0) {
        return false;
      }
      if (leadingMissingElementSupportedOnlyBySingleTerminalTail) {
        return false;
      }
      if (parseCandidate.matched &&
          !detail::is_better_choice_recovery_candidate(
              insertCandidate, parseCandidate,
              {.preferredBoundaryElement = std::addressof(current)})) {
        return false;
      }
      ctx.rewind(checkpoint);
      bool replayMatchedCleanTail = false;
      return replayInsertAttempt(replayMatchedCleanTail);
    }
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
    if (!allows_synthetic_terminal_insert(ctx, current)) {
      return false;
    }
    const auto checkpoint = ctx.mark();
    const auto parseStartOffset = ctx.cursorOffset();
    auto allowLocalInsertGuard =
        ctx.withEditPermissions(true, ctx.allowDelete);
    (void)allowLocalInsertGuard;
    const bool inserted = detail::apply_insert_synthetic_recovery_edit(
        ctx, std::addressof(current));
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
    if constexpr (requires {
                    { current.isWordLike() } -> std::convertible_to<bool>;
                  }) {
      if (current.isWordLike() && postSkipCursorOffset <= parseStartOffset) {
        ctx.rewind(checkpoint);
        return false;
      }
    }
    if (!allowNullableTailStop) {
      if (postSkipCursorOffset <= parseStartOffset) {
        ctx.rewind(checkpoint);
        return false;
      }
    }
    return true;
  }

  template <std::size_t I>
  bool run_delete_then_insert_current_attempt(RecoveryContext &ctx) const {
    const auto deleteCheckpoint = ctx.mark();
    const bool previousSkipAfterDelete = ctx.skipAfterDelete;
    ctx.skipAfterDelete = false;
    bool deletedAny = false;
    while (ctx.deleteOneCodepoint()) {
      deletedAny = true;
      if (ctx.skip_without_builder(ctx.cursor()) > ctx.cursor()) {
        if (ctx.extendLastDeleteThroughHiddenTrivia()) {
          continue;
        }
        break;
      }
    }
    ctx.skipAfterDelete = previousSkipAfterDelete;
    if (!deletedAny) {
      ctx.rewind(deleteCheckpoint);
      return false;
    }
    if (this->template try_insert_current_terminal_with_clean_tail<I>(ctx,
                                                                      true)) {
      return true;
    }
    ctx.rewind(deleteCheckpoint);
    return false;
  }

  template <std::size_t I>
  void consider_terminal_current_failure_attempts(
      SequenceRecoveryReplayPlan &bestPlan,
      detail::EditableRecoveryCandidate &bestCandidate,
      RecoveryContext &ctx, bool allowInsertCurrentWithCleanTail,
      bool allowDeleteThenInsertCurrent,
      bool allowNullableTailStop) const {
    if (!allowInsertCurrentWithCleanTail && !allowDeleteThenInsertCurrent) {
      return;
    }
    const auto &current = std::get<I>(elements);
    const auto checkpoint = ctx.mark();
    const auto baseEditCost = ctx.currentEditCost();
    const auto baseRecoveryEditCount = ctx.recoveryEditCount();
    if (allowInsertCurrentWithCleanTail) {
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .insertCurrentWithCleanTail = true,
           .allowNullableTailStop = allowNullableTailStop},
          detail::evaluate_editable_recovery_candidate(
              ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
              [this, &ctx, allowNullableTailStop]() {
                return this->template
                    try_insert_current_terminal_with_clean_tail<I>(
                        ctx, allowNullableTailStop);
              }),
          std::addressof(current));
    }
    if (allowDeleteThenInsertCurrent) {
      this->consider_sequence_recovery_candidate(
          bestPlan, bestCandidate,
          {.valid = true,
           .deleteThenInsertCurrent = true,
           .allowNullableTailStop = allowNullableTailStop},
          detail::evaluate_editable_recovery_candidate(
              ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
              [this, &ctx]() {
                return this->template run_delete_then_insert_current_attempt<I>(
                    ctx);
              }),
          std::addressof(current));
    }
  }

  template <std::size_t I>
  bool replay_terminal_current_failure_attempt(
      RecoveryContext &ctx,
      const SequenceRecoveryReplayPlan &plan) const {
    if (!plan.valid) {
      return false;
    }
    if (plan.insertCurrentWithCleanTail) {
      return this->template try_insert_current_terminal_with_clean_tail<I>(
          ctx, plan.allowNullableTailStop);
    }
    if (plan.deleteThenInsertCurrent) {
      return this->template run_delete_then_insert_current_attempt<I>(ctx);
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
  [[nodiscard]] constexpr bool
  is_word_like_impl(std::index_sequence<Is...>) const noexcept {
    return (... &&
            detail::element_is_word_like_terminal(std::get<Is>(elements)));
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
struct GroupWithSkipper final : Group<Elements...>, CompletionSkipperProvider {
  using Base = Group<Elements...>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit GroupWithSkipper(std::tuple<Elements...> &&elements,
                            Skipper localSkipper)
      : Base{std::move(elements)}, _localSkipper{std::move(localSkipper)} {}

  explicit GroupWithSkipper(const std::tuple<Elements...> &elements,
                            Skipper localSkipper)
      : Base{elements}, _localSkipper{std::move(localSkipper)} {}

  GroupWithSkipper(GroupWithSkipper &&) noexcept = default;
  GroupWithSkipper(const GroupWithSkipper &) = default;
  GroupWithSkipper &operator=(GroupWithSkipper &&) noexcept = default;
  GroupWithSkipper &operator=(const GroupWithSkipper &) = default;
  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return std::addressof(_localSkipper);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    auto localSkipperGuard = ctx.with_skipper(_localSkipper);
    (void)localSkipperGuard;
    return parse(static_cast<const Base &>(*this), ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    static_cast<const Base &>(*this).init_impl(ctx);
  }

  Skipper _localSkipper;
};

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
