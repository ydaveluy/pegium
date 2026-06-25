#pragma once

/// Parser expression representing prioritized alternatives.

#include <array>
#include <concepts>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/parser/ChoiceAttempt.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/CstSearch.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/SkipperWrapped.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pegium::parser::detail {

[[nodiscard]] inline RecoveryPolicyFingerprint
make_recovery_policy_fingerprint(const RecoveryContext &ctx) noexcept {
  RecoveryPolicyFingerprint fp;
  fp.followProbeFn = reinterpret_cast<const void *>(ctx._followProbeFn);
  fp.followProbeData = ctx._followProbeData;
  fp.recoverableFollowProbeFn =
      reinterpret_cast<const void *>(ctx._recoverableFollowProbeFn);
  fp.recoverableFollowProbeData = ctx._recoverableFollowProbeData;
  fp.recoverableFollowConsumesVisibleProbeFn = reinterpret_cast<const void *>(
      ctx._recoverableFollowConsumesVisibleProbeFn);
  fp.recoverableFollowConsumesVisibleProbeData =
      ctx._recoverableFollowConsumesVisibleProbeData;
  fp.remainingEditBudget = ctx.maxEditCost > ctx.currentEditCost()
                               ? ctx.maxEditCost - ctx.currentEditCost()
                               : 0U;
  fp.consecutiveDeletes = ctx.recoveryState.editBudget.consecutiveDeletes;
  fp.editFloorOffset = ctx.editFloorOffset;
  fp.allowInsert = ctx.allowInsert;
  fp.allowDelete = ctx.allowDelete;
  fp.skipAfterDelete = ctx.skipAfterDelete;
  fp.allowDestructiveWindowContinuation =
      ctx.allowDestructiveWindowContinuation;
  fp.allowLeadingTerminalInsertScope = ctx.allowLeadingTerminalInsertScope;
  fp.inRecoveryPhase = ctx.isInRecoveryPhase();
  fp.hadEdits = ctx.recoveryState.editBudget.hadEdits;
  fp.insideEditWindow = ctx.editWindow.has_value();
  fp.completedWindowContinuation =
      ctx.allowsCompletedWindowContinuationRecovery();
  fp.frontierBlocked = ctx.frontierBlocked();
  fp.trackEditState = ctx.trackEditState;
  fp.committedRecoveryEditIndex = ctx.committedRecoveryEditIndex;
  fp.remainingEditCount =
      ctx.maxEditsPerAttempt > ctx.recoveryState.editBudget.editCount
          ? ctx.maxEditsPerAttempt - ctx.recoveryState.editBudget.editCount
          : 0U;
  fp.remainingConsecutiveDeletes =
      ctx.maxConsecutiveCodepointDeletes >
              ctx.recoveryState.editBudget.consecutiveDeletes
          ? ctx.maxConsecutiveCodepointDeletes -
                ctx.recoveryState.editBudget.consecutiveDeletes
          : 0U;
  return fp;
}

[[nodiscard]] inline ChoiceRecoverCacheKey
make_choice_recover_cache_key(const RecoveryContext &ctx,
                              const void *choice) noexcept {
  ChoiceRecoverCacheKey key;
  key.choice = choice;
  key.cursorOffset = ctx.cursorOffset();
  key.maxCursorOffset = ctx.maxCursorOffset();
  key.furthestVisibleLeafCount =
      static_cast<std::uint32_t>(ctx.furthestFailureHistorySize());
  key.currentVisibleLeafCount =
      static_cast<std::uint32_t>(ctx.failureHistorySize());
  key.policy = make_recovery_policy_fingerprint(ctx);
  return key;
}

} // namespace pegium::parser::detail

namespace pegium::parser {


template <Expression... Elements>
struct OrderedChoice : grammar::OrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  static constexpr bool nullable =
      (... || std::remove_cvref_t<Elements>::nullable);
  static constexpr bool isFailureSafe = true;
  static consteval bool nullable_only_last() {
    constexpr bool flags[] = {std::remove_cvref_t<Elements>::nullable...};
    for (std::size_t i = 0; i + 1 < sizeof...(Elements); ++i)
      if (flags[i])
        return false;
    return true;
  }

  static_assert(nullable_only_last(),
                "OrderedChoice: a nullable alternative must be the last one.");

  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elements)
      : choices{std::move(elements)} {}
  constexpr OrderedChoice(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice(const OrderedChoice &) = default;
  constexpr OrderedChoice &operator=(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice &operator=(const OrderedChoice &) = default;

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

public:
  bool probeRecoverable(RecoveryContext &ctx) const {
    return any_choice([&](const auto &c) {
      return parser::attempt_fast_probe(ctx, c) ||
             probe_locally_recoverable(c, ctx);
    });
  }

  template <typename Context> bool probeMatchHere(Context &ctx) const {
    return any_choice(
        [&](const auto &c) { return parser::probe_match_here(c, ctx); });
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    return any_choice(
        [&](const auto &c) { return probe_recoverable_at_entry(c, ctx); });
  }

  bool probeRecoverableAtEntryConsumesVisible(RecoveryContext &ctx) const {
    return any_choice([&](const auto &c) {
      return parser::attempt_fast_probe(ctx, c) ||
             probe_recoverable_at_entry_consumes_visible(c, ctx);
    });
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (parser::init(std::get<Is>(choices), ctx), ...);
    }(std::make_index_sequence<sizeof...(Elements)>{});
  }

private:

  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return any_choice([&](const auto &c) { return parser::probe(c, ctx); });
  }

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    return any_choice(
        [&](const auto &c) { return parser::attempt_fast_probe(ctx, c); });
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      return parse_strict_impl(ctx);
    } else if constexpr (RecoveryParseModeContext<Context>) {
      return parse_recovery_impl(ctx);
    } else {
      return parse_expect_impl(ctx);
    }
  }

  template <StrictParseModeContext Context>
  bool parse_strict_impl(Context &ctx) const {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceStrictPasses);
    return match_choice(ctx);
  }

  bool parse_recovery_impl(RecoveryContext &ctx) const {
    if (ctx.recoveryDescentInactive()) {
      TrackedParseContext &strictCtx = ctx;
      return parse_strict_impl(strictCtx);
    }
    if (!ctx.isInRecoveryPhase() && ctx.hasPendingRecoveryWindows() &&
        ctx.cursorOffset() < ctx.pendingRecoveryWindowBeginOffset()) {
      return match_choice(ctx);
    }
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceRecoverCalls);

    const auto entryCheckpoint = ctx.mark();
    PEGIUM_RECOVERY_TRACE("[choice rule] enter offset=", ctx.cursorOffset(),
                          " allowI=", ctx.allowInsert,
                          " allowD=", ctx.allowDelete);

    const auto cacheKey = detail::make_choice_recover_cache_key(ctx, this);
    ChoiceAttempt bestAttempt;
    if (const auto *cached = ctx.choiceRecoverCache().tryGet(cacheKey)) {
      bestAttempt = *cached;
      ctx.bumpMaxCursor(ctx.begin + bestAttempt.postEvalMaxCursorOffset);
      ctx.bumpFurthestFailureHistorySize(
          bestAttempt.postEvalFurthestVisibleLeafCount);
      ctx.bumpFurthestFailureOffset(bestAttempt.postEvalFurthestFailureOffset);
    } else {
      const auto baseEditCost = ctx.currentEditCost();
      const auto baseRecoveryEditCount = ctx.recoveryEditCount();
      bestAttempt = collect_choice_attempts(
          ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
          std::make_index_sequence<sizeof...(Elements)>{});
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceStrictPasses);
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceEditablePasses);
      bestAttempt.postEvalMaxCursorOffset = ctx.maxCursorOffset();
      bestAttempt.postEvalFurthestVisibleLeafCount =
          static_cast<std::uint32_t>(ctx.furthestFailureHistorySize());
      bestAttempt.postEvalFurthestFailureOffset = ctx.furthestFailureOffset();
      ctx.choiceRecoverCache().store(cacheKey, bestAttempt);
    }
    if (replay_choice_attempt(ctx, entryCheckpoint, bestAttempt)) {
      return true;
    }

    PEGIUM_RECOVERY_TRACE("[choice rule] fail offset=", ctx.cursorOffset());
    return false;
  }

  bool parse_expect_impl(ExpectContext &ctx) const {
    const auto base = ctx.mark();
    std::array<BranchResult, sizeof...(Elements)> branches{};
    collect_expect_results(ctx, base, branches,
                           std::make_index_sequence<sizeof...(Elements)>{});

    std::optional<std::size_t> bestIndex;
    for (std::size_t index = 0; index < branches.size(); ++index) {
      const auto &candidate = branches[index];
      if (!candidate.matched) {
        continue;
      }
      if (!bestIndex.has_value()) {
        bestIndex = index;
        continue;
      }
      const auto &best = branches[*bestIndex];
      if (candidate.cursor > best.cursor ||
          (candidate.cursor == best.cursor &&
           candidate.blocked != best.blocked && !candidate.blocked) ||
          (candidate.cursor == best.cursor &&
           candidate.blocked == best.blocked &&
           candidate.editCost < best.editCost)) {
        bestIndex = index;
      }
    }

    if (!bestIndex.has_value()) {
      ctx.rewind(base);
      return false;
    }

    ctx.rewind(base);
    if (!replay_expect_branch_by_index(ctx, *bestIndex)) {
      return false;
    }

    const auto &best = branches[*bestIndex];
    for (std::size_t index = 0; index < branches.size(); ++index) {
      if (index == *bestIndex) {
        continue;
      }
      const auto &candidate = branches[index];
      if (!candidate.matched || candidate.cursor != best.cursor ||
          candidate.editCost != best.editCost) {
        continue;
      }
      ctx.mergeFrontier(candidate.frontier);
    }
    if (!best.blocked) {
      ctx.clearFrontierBlock();
    }
    return true;
  }

public:
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
    return SkipperWrapped<OrderedChoice<Elements...>>{
        *this, static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return SkipperWrapped<OrderedChoice<Elements...>>{
        std::move(*this),
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

private:
  using BranchResult = ExpectBranchResult;
  using EditableRecoveryCandidate = detail::EditableRecoveryCandidate;
  using ChoiceAttemptKind = detail::ChoiceAttemptKind;
  using ChoiceAttempt = detail::ChoiceAttempt;

  /// Captures the no-edit branch candidate. Only called after
  /// `attempt_parse_no_edits` matched, so no recovery edits were
  /// committed: editCost / editCount / firstEditOffset are zero by
  /// construction.
  [[nodiscard]] detail::EditableRecoveryCandidate
  capture_choice_candidate(RecoveryContext &ctx) const {
    // No recovery edits here (no-edit branch), so replayPrefix stays at its
    // default ReplayPrefixClass::Empty — same as classify_replay_prefix(false,...).
    // OrderedChoice ranks on the post-skip cursor, so keyProgressOffset mirrors
    // postSkipCursorOffset.
    const TextOffset postSkipCursor = detail::post_skip_cursor_offset(ctx);
    return detail::EditableRecoveryCandidate{
        .matched = true,
        .postSkipCursorOffset = postSkipCursor,
        .keyProgressOffset = postSkipCursor,
    };
  }

  /// Selects the better of two choice attempts. Three-stage shape:
  ///
  ///   1. Admission: if `candidate.envelope.key.matched` is false, the
  ///      candidate is rejected.
  ///   2. Family redundancy: `extension_dominates` removes a base
  ///      candidate when an extending candidate (same anchor,
  ///      `ReplayPrefixClass::ExtendedCommittedPrefix`) progresses
  ///      strictly further at no-worse cost, gated by a
  ///      `progressGap >= costGap` ratio test. NOT a
  ///      replay-equivalence dominance — the two candidates carry
  ///      different scripts.
  ///   3. Ranking: `is_better_recovery_key` on `envelope.key` decides
  ///      everything the family-redundancy filter did not settle.
  static void consider_choice_attempt(ChoiceAttempt &bestAttempt,
                                      const ChoiceAttempt &candidate) noexcept {
    if (!candidate.envelope.key.matched) {
      return;
    }
    if (bestAttempt.envelope.key.matched &&
        detail::extension_dominates(
            candidate.envelope, bestAttempt.envelope,
            detail::ExtensionDominanceGuard::WhenCurrentProgressedPastAnchor)) {
      bestAttempt = candidate;
      return;
    }
    if (!bestAttempt.envelope.key.matched ||
        detail::is_better_recovery_key(candidate.envelope.key,
                                        bestAttempt.envelope.key)) {
      bestAttempt = candidate;
    }
  }

public:
  std::tuple<Elements...> choices;

private:
  template <std::size_t... Is>
  const AbstractElement *get_impl(std::size_t elementIndex,
                                  std::index_sequence<Is...>) const noexcept {
    using AccessorFn =
        const AbstractElement *(*)(const OrderedChoice *) noexcept;

    static constexpr std::array<AccessorFn, sizeof...(Elements)> accessors = {
        +[](const OrderedChoice *self) noexcept -> const AbstractElement * {
          return std::addressof(std::get<Is>(self->choices));
        }...};

    return accessors[elementIndex](this);
  }

  /// Folds an OR over every branch with a caller-supplied predicate. The
  /// predicate is invoked with each `choice` element in order; the fold
  /// short-circuits as soon as one branch returns `true`.
  template <typename Predicate, std::size_t... Is>
  [[gnu::always_inline]] bool
  any_choice_impl(Predicate &&pred, std::index_sequence<Is...>) const {
    return (... || pred(std::get<Is>(choices)));
  }

  template <typename Predicate>
  [[gnu::always_inline]] bool any_choice(Predicate &&pred) const {
    return any_choice_impl(std::forward<Predicate>(pred),
                           std::make_index_sequence<sizeof...(Elements)>{});
  }

  template <std::size_t I = 0>
  constexpr const char *terminal_impl(const char *inputBegin) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    if constexpr (I == sizeof...(Elements)) {
      return nullptr;
    } else {
      const char *matchEnd = std::get<I>(choices).terminal(inputBegin);
      return matchEnd != nullptr ? matchEnd
                                 : terminal_impl<I + 1>(inputBegin);
    }
  }

  template <StrictParseModeContext Context>
  bool match_choice(Context &ctx) const {
    return any_choice(
        [&](const auto &c) { return attempt_parse_strict(ctx, c); });
  }

  template <RecoveryParseModeContext Context>
  bool match_choice(Context &ctx) const {
    return any_choice([&](const auto &c) {
      const auto checkpoint = ctx.mark();
      if (parse(c, ctx)) {
        return true;
      }
      ctx.rewind(checkpoint);
      return false;
    });
  }

  /// Dispatches to the branch at `bestIndex` and invokes `action` on it.
  /// Returns false if `bestIndex` is out of range. The lambda gets the
  /// concrete branch reference, so `std::get<I>` is resolved at compile time
  /// for each unfolded position.
  template <typename Action, std::size_t I = 0>
  [[gnu::always_inline]] bool
  dispatch_choice_by_index(std::size_t bestIndex, Action &&action) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return action(std::get<I>(choices));
      }
      return dispatch_choice_by_index<Action, I + 1>(
          bestIndex, std::forward<Action>(action));
    }
  }

  bool replay_no_edit_choice_by_index(RecoveryContext &ctx,
                                      std::size_t bestIndex) const {
    return dispatch_choice_by_index(bestIndex, [&](const auto &c) {
      return attempt_parse_no_edits(ctx, c);
    });
  }

  bool replay_editable_choice_by_index(RecoveryContext &ctx,
                                       std::size_t bestIndex) const {
    return dispatch_choice_by_index(bestIndex, [&](const auto &c) {
      return attempt_parse_editable(ctx, c);
    });
  }

  template <typename Checkpoint>
  bool replay_choice_attempt(RecoveryContext &ctx,
                             const Checkpoint &entryCheckpoint,
                             const ChoiceAttempt &attempt) const {
    switch (attempt.kind) {
    case ChoiceAttemptKind::NoEditReplay:
      if (replay_no_edit_choice_by_index(ctx, attempt.branchIndex)) {
        PEGIUM_RECOVERY_TRACE("[choice rule] deferred strict success offset=",
                              ctx.cursorOffset());
        return true;
      }
      ctx.rewind(entryCheckpoint);
      return false;
    case ChoiceAttemptKind::Editable: {
      if (replay_editable_choice_by_index(ctx, attempt.branchIndex)) {
        PEGIUM_RECOVERY_TRACE("[choice rule] editable success offset=",
                              ctx.cursorOffset());
        return true;
      }
      ctx.rewind(entryCheckpoint);
      return false;
    }
    case ChoiceAttemptKind::None:
      return false;
    }
    return false;
  }

  template <std::size_t... Is, typename Checkpoint>
  ChoiceAttempt collect_choice_attempts(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      std::index_sequence<Is...>) const {
    ChoiceAttempt bestAttempt;
    (([&]() {
       const auto noEditAttempt =
           evaluate_branch_no_edit_choice_attempt<Is>(ctx, entryCheckpoint);
       consider_choice_attempt(bestAttempt, noEditAttempt);
       const auto editableCandidate = detail::evaluate_editable_recovery_candidate(
           ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
           [this, &ctx]() {
             return attempt_parse_editable(ctx, std::get<Is>(choices));
           });
       consider_choice_attempt(
           bestAttempt,
           ChoiceAttempt{
               .branchIndex = Is,
               .kind = ChoiceAttemptKind::Editable,
               .envelope = detail::to_candidate_envelope(editableCandidate),
           });
     }()),
     ...);
    return bestAttempt;
  }

  template <std::size_t I, typename Checkpoint>
  ChoiceAttempt evaluate_branch_no_edit_choice_attempt(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint) const {
    if (!attempt_parse_no_edits(ctx, std::get<I>(choices))) {
      ctx.rewind(entryCheckpoint);
      return {.branchIndex = I};
    }
    const auto candidate = capture_choice_candidate(ctx);
    PEGIUM_RECOVERY_TRACE("[choice rule] strict success offset=",
                          ctx.cursorOffset());
    ctx.rewind(entryCheckpoint);
    return {
        .branchIndex = I,
        .kind = ChoiceAttemptKind::NoEditReplay,
        .envelope = detail::to_candidate_envelope(candidate),
    };
  }

  template <std::size_t... Is>
  void collect_expect_results(ExpectContext &ctx,
                              const ExpectContext::Checkpoint &base,
                              std::array<BranchResult, sizeof...(Elements)> &branches,
                              std::index_sequence<Is...>) const {
    (collect_expect_result<Is>(ctx, base, branches[Is]), ...);
  }

  template <std::size_t I>
  void collect_expect_result(ExpectContext &ctx,
                             const ExpectContext::Checkpoint &base,
                             BranchResult &result) const {
    collect_expect_branch(ctx, base, std::get<I>(choices), result);
  }

  bool replay_expect_branch_by_index(ExpectContext &ctx,
                                     std::size_t bestIndex) const {
    return dispatch_choice_by_index(bestIndex, [&](const auto &c) {
      return parser::replay_expect_branch(ctx, c);
    });
  }

};

template <Expression... Elements>
using OrderedChoiceWithSkipper = SkipperWrapped<OrderedChoice<Elements...>>;

namespace detail {

template <typename T> struct IsOrderedChoiceRaw : std::false_type {};

template <typename... E>
struct IsOrderedChoiceRaw<OrderedChoice<E...>> : std::true_type {};

template <typename... E>
struct IsOrderedChoiceRaw<OrderedChoiceWithSkipper<E...>> : std::true_type {};

template <typename T> struct IsOrderedChoiceFlattenRaw : std::false_type {};

template <typename... E>
struct IsOrderedChoiceFlattenRaw<OrderedChoice<E...>> : std::true_type {};

template <typename C>
  requires IsOrderedChoiceFlattenRaw<std::remove_cvref_t<C>>::value
constexpr decltype(auto) as_choice_tuple(C &&choice) {
  return std::forward<C>(choice).choices;
}

template <Expression Expr>
  requires(!IsOrderedChoiceFlattenRaw<std::remove_cvref_t<Expr>>::value)
constexpr auto as_choice_tuple(Expr &&expr) {
  return std::tuple<ExpressionHolder<Expr>>{std::forward<Expr>(expr)};
}

template <typename... Ts>
constexpr auto make_ordered_choice(std::tuple<Ts...> &&elements) {
  return OrderedChoice<Ts...>{std::move(elements)};
}

} // namespace detail

template <Expression Lhs, Expression Rhs>
constexpr auto operator|(Lhs &&lhs, Rhs &&rhs) {
  return detail::make_ordered_choice(std::tuple_cat(
      detail::as_choice_tuple(std::forward<Lhs>(lhs)),
      detail::as_choice_tuple(std::forward<Rhs>(rhs))));
}

template <typename T>
struct IsOrderedChoice : detail::IsOrderedChoiceRaw<std::remove_cvref_t<T>> {};

} // namespace pegium::parser
