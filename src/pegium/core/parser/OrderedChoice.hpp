#pragma once

/// Parser expression representing prioritized alternatives.

#include <algorithm>
#include <array>
#include <concepts>
#include <limits>
#include <pegium/core/grammar/OrderedChoice.hpp>
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
#include <pegium/core/parser/StepTrace.hpp>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <Expression... Elements> struct OrderedChoiceWithSkipper;

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
    return probe_recoverable_choices(
        ctx, std::make_index_sequence<sizeof...(Elements)>{});
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    return probe_recoverable_entry_choices(
        ctx, std::make_index_sequence<sizeof...(Elements)>{});
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    init_choices<0>(ctx);
  }

private:

  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return probe_choices(ctx, std::make_index_sequence<sizeof...(Elements)>{});
  }

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    return fast_probe_choices(ctx,
                              std::make_index_sequence<sizeof...(Elements)>{});
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceStrictPasses);
      return match_choice(ctx);
    } else if constexpr (RecoveryParseModeContext<Context>) {
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceRecoverCalls);

      const auto entryCheckpoint = ctx.mark();
      const char *const entryFurthestExploredCursor =
          ctx.furthestExploredCursor();
      PEGIUM_RECOVERY_TRACE("[choice rule] enter offset=", ctx.cursorOffset(),
                            " allowI=", ctx.allowInsert,
                            " allowD=", ctx.allowDelete);

      const auto bestAttempt = evaluate_choice_attempts(ctx, entryCheckpoint);
      if (replay_choice_attempt(ctx, entryCheckpoint, bestAttempt,
                                entryFurthestExploredCursor)) {
        return true;
      }

      PEGIUM_RECOVERY_TRACE("[choice rule] fail offset=", ctx.cursorOffset());
      return false;
    } else {
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
    return OrderedChoiceWithSkipper<Elements...>{
        *this, static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return OrderedChoiceWithSkipper<Elements...>{
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
  enum class ChoiceAttemptKind : std::uint8_t {
    None,
    NoEditReplay,
    Editable,
    RestartReplay,
  };

  enum class RestartReplayMode : std::uint8_t {
    Editable,
    NoEdit,
  };

  struct ChoiceAttempt {
    std::size_t branchIndex = 0;
    ChoiceAttemptKind kind = ChoiceAttemptKind::None;
    detail::EditableRecoveryCandidate recovery{};
    TextOffset restartRetryCursorOffset = 0;
    RestartReplayMode restartReplayMode = RestartReplayMode::Editable;
    bool startedWithoutEdits = false;
  };

  [[nodiscard]] detail::EditableRecoveryCandidate capture_choice_candidate(
      RecoveryContext &ctx, std::uint32_t baseEditCost,
      std::size_t baseRecoveryEditCount) const {
    detail::EditableRecoveryCandidate candidate{.matched = true,
                                                .cursorOffset = ctx.cursorOffset()};
    candidate.postSkipCursorOffset = detail::post_skip_cursor_offset(ctx);
    candidate.editCost = ctx.currentEditCost() - baseEditCost;
    candidate.editCount = static_cast<std::uint32_t>(ctx.recoveryEditCount() -
                                                     baseRecoveryEditCount);
    if (ctx.recoveryEditCount() > baseRecoveryEditCount) {
      const auto edits = ctx.recoveryEditsView();
      const auto &firstEdit = edits[baseRecoveryEditCount];
      candidate.firstEditOffset = firstEdit.beginOffset;
      candidate.firstEditElement = firstEdit.element;
      candidate.hasDeleteEdit =
          firstEdit.kind == ParseDiagnosticKind::Deleted;
      TextOffset maxEndOffset = firstEdit.endOffset;
      for (std::size_t i = baseRecoveryEditCount + 1u; i < edits.size(); ++i) {
        candidate.hasDeleteEdit =
            candidate.hasDeleteEdit ||
            edits[i].kind == ParseDiagnosticKind::Deleted;
        maxEndOffset = std::max(maxEndOffset, edits[i].endOffset);
      }
      candidate.editSpan = maxEndOffset > firstEdit.beginOffset
                               ? maxEndOffset - firstEdit.beginOffset
                               : 0;
    }
    return candidate;
  }

  static void consider_choice_attempt(ChoiceAttempt &bestAttempt,
                                      const ChoiceAttempt &candidate,
                                      TextOffset parseStartOffset) noexcept {
    if (!candidate.recovery.matched) {
      return;
    }
    if (!bestAttempt.recovery.matched ||
        detail::is_better_choice_recovery_candidate(
            candidate.recovery, bestAttempt.recovery,
            {.parseStartOffset = parseStartOffset})) {
      bestAttempt = candidate;
    }
  }

  template <std::size_t I>
  [[nodiscard]] bool
  branch_starts_without_edits(RecoveryContext &ctx) const {
    const auto checkpoint = ctx.mark();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    ctx.restoreFurthestExploredCursor(ctx.cursor());
    const auto startOffset = ctx.cursorOffset();
    const auto advanced_without_edits = [&]() noexcept {
      return ctx.cursorOffset() > startOffset ||
             ctx.furthestExploredOffset() > startOffset;
    };
    bool started = false;
    if (attempt_fast_probe(ctx, std::get<I>(choices))) {
      started = advanced_without_edits();
    }
    if (!started) {
      (void)attempt_parse_no_edits(ctx, std::get<I>(choices));
      started = advanced_without_edits();
    }
    ctx.rewind(checkpoint);
    ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    return started;
  }

  template <typename Checkpoint>
  bool replay_no_edit_choice(RecoveryContext &ctx,
                             const Checkpoint &entryCheckpoint,
                             std::size_t branchIndex) const {
    if (replay_no_edit_choice_by_index(ctx, branchIndex)) {
      PEGIUM_RECOVERY_TRACE("[choice rule] deferred strict success offset=",
                            ctx.cursorOffset());
      return true;
    }
    ctx.rewind(entryCheckpoint);
    return false;
  }

  template <typename Checkpoint>
  ChoiceAttempt evaluate_choice_attempts(RecoveryContext &ctx,
                                         const Checkpoint &entryCheckpoint) const {
    const auto parseStartOffset = ctx.cursorOffset();
    const auto baseEditCost = ctx.currentEditCost();
    const auto baseRecoveryEditCount = ctx.recoveryEditCount();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    ctx.restoreFurthestExploredCursor(ctx.cursor());
    const auto bestAttempt = collect_choice_attempts(
        ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
        parseStartOffset, std::make_index_sequence<sizeof...(Elements)>{});

    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceStrictPasses);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ChoiceEditablePasses);
    if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    }
    return bestAttempt;
  }

public:
  std::tuple<Elements...> choices;

private:
  template <std::size_t I>
  void init_choices(AstReflectionInitContext &ctx) const {
    if constexpr (I < sizeof...(Elements)) {
      parser::init(std::get<I>(choices), ctx);
      init_choices<I + 1>(ctx);
    }
  }


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

  template <StrictParseModeContext Context, std::size_t... Is>
  bool probe_choices(Context &ctx, std::index_sequence<Is...>) const {
    return (... || parser::probe(std::get<Is>(choices), ctx));
  }

  template <StrictParseModeContext Context, std::size_t... Is>
  bool fast_probe_choices(Context &ctx,
                          std::index_sequence<Is...>) const {
    return (... || parser::attempt_fast_probe(ctx, std::get<Is>(choices)));
  }

  template <std::size_t... Is>
  bool probe_recoverable_choices(RecoveryContext &ctx,
                                 std::index_sequence<Is...>) const {
    return (... ||
            (parser::attempt_fast_probe(ctx, std::get<Is>(choices)) ||
             probe_locally_recoverable(std::get<Is>(choices), ctx)));
  }

  template <std::size_t... Is>
  bool probe_recoverable_entry_choices(RecoveryContext &ctx,
                                       std::index_sequence<Is...>) const {
    return (... || [&ctx](const auto &choice) {
             return probe_recoverable_at_entry(choice, ctx);
           }(std::get<Is>(choices)));
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

  template <std::size_t I = 0, StrictParseModeContext Context>
  bool match_choice(Context &ctx) const {
    if (attempt_parse_strict(ctx, std::get<I>(choices))) {
      return true;
    }
    if constexpr (I + 1 == sizeof...(Elements)) {
      return false;
    } else {
      return match_choice<I + 1>(ctx);
    }
  }

  template <std::size_t I = 0>
  bool replay_no_edit_choice_by_index(RecoveryContext &ctx,
                                      std::size_t bestIndex) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return attempt_parse_no_edits(ctx, std::get<I>(choices));
      }
      return replay_no_edit_choice_by_index<I + 1>(ctx, bestIndex);
    }
  }

  template <std::size_t I>
  bool run_restart_choice(RecoveryContext &ctx, TextOffset retryCursorOffset,
                          RestartReplayMode replayMode) const {
    const auto recoveryCheckpoint = ctx.mark();
    detail::DeleteRetryReplayScope restartScanScope{ctx};
    while (ctx.cursorOffset() < retryCursorOffset) {
      if (!ctx.deleteOneCodepoint()) {
        if (restartScanScope.tryEnableExtendedDeleteScan()) {
          continue;
        }
        ctx.rewind(recoveryCheckpoint);
        return false;
      }
    }
    const bool usedExtendedDeleteScan = restartScanScope.overflowBudgetScope.enabled;
    restartScanScope.restoreExtendedDeleteScan();
    ctx.skip();
    const bool matchedAfterRestart =
        replayMode == RestartReplayMode::NoEdit
            ? attempt_parse_no_edits(ctx, std::get<I>(choices))
            : attempt_parse_editable(ctx, std::get<I>(choices));
    if (!matchedAfterRestart) {
      ctx.rewind(recoveryCheckpoint);
      return false;
    }
    if (usedExtendedDeleteScan) {
      detail::enable_budget_overflow_edits_for_attempt(ctx);
    }
    PEGIUM_RECOVERY_TRACE("[choice rule] restart success offset=",
                          ctx.cursorOffset());
    return true;
  }

  template <std::size_t I = 0>
  bool replay_editable_choice_by_index(RecoveryContext &ctx,
                                       std::size_t bestIndex) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return attempt_parse_editable(ctx, std::get<I>(choices));
      }
      return replay_editable_choice_by_index<I + 1>(ctx, bestIndex);
    }
  }

  template <std::size_t I = 0>
  bool replay_restart_choice_by_index(RecoveryContext &ctx,
                                      std::size_t bestIndex,
                                      TextOffset retryCursorOffset,
                                      RestartReplayMode replayMode) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return run_restart_choice<I>(ctx, retryCursorOffset, replayMode);
      }
      return replay_restart_choice_by_index<I + 1>(ctx, bestIndex,
                                                   retryCursorOffset,
                                                   replayMode);
    }
  }

  template <std::size_t I>
  ChoiceAttempt evaluate_restart_choice_attempt(
      RecoveryContext &ctx, std::uint32_t baseEditCost,
      std::size_t baseRecoveryEditCount, TextOffset parseStartOffset) const {
    ChoiceAttempt bestAttempt;
    if (!ctx.allowDeleteRetry || !ctx.canDelete()) {
      return bestAttempt;
    }

    const auto recoveryCheckpoint = ctx.mark();
    detail::DeleteRetryReplayScope restartScanScope{ctx};
    while (true) {
      while (ctx.deleteOneCodepoint()) {
        const auto candidateRetryCursorOffset = ctx.cursorOffset();
        const auto retryCheckpoint = ctx.mark();
        ctx.skip();
        const bool matchedAfterRetry =
            restartScanScope.overflowBudgetScope.enabled
                ? attempt_parse_no_edits(ctx, std::get<I>(choices))
                : attempt_parse_editable(ctx, std::get<I>(choices));
        if (matchedAfterRetry) {
          ChoiceAttempt candidateAttempt{
              .branchIndex = I,
              .kind = ChoiceAttemptKind::RestartReplay,
              .recovery = capture_choice_candidate(ctx, baseEditCost,
                                                   baseRecoveryEditCount),
          };
          candidateAttempt.restartRetryCursorOffset = candidateRetryCursorOffset;
          candidateAttempt.restartReplayMode =
              restartScanScope.overflowBudgetScope.enabled
                  ? RestartReplayMode::NoEdit
                  : RestartReplayMode::Editable;
          if (!bestAttempt.recovery.matched ||
              detail::is_better_choice_recovery_candidate(
                  candidateAttempt.recovery, bestAttempt.recovery,
                  {.parseStartOffset = parseStartOffset})) {
            bestAttempt = candidateAttempt;
          }
        }
        ctx.rewind(retryCheckpoint);
        if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
          if (ctx.skip_without_builder(ctx.cursor()) > ctx.cursor()) {
            if (matchedAfterRetry) {
              break;
            }
            if constexpr (requires { ctx.extendLastDeleteThroughHiddenTrivia(); }) {
              if (ctx.extendLastDeleteThroughHiddenTrivia()) {
                continue;
              }
            }
            break;
          }
        }
      }
      if (!restartScanScope.tryEnableExtendedDeleteScan()) {
        break;
      }
    }
    ctx.rewind(recoveryCheckpoint);
    return bestAttempt;
  }

  template <typename Checkpoint>
  bool replay_choice_attempt(RecoveryContext &ctx,
                             const Checkpoint &entryCheckpoint,
                             const ChoiceAttempt &attempt,
                             const char *entryFurthestExploredCursor) const {
    switch (attempt.kind) {
    case ChoiceAttemptKind::NoEditReplay:
      return replay_no_edit_choice(ctx, entryCheckpoint, attempt.branchIndex);
    case ChoiceAttemptKind::RestartReplay:
      return replay_restart_choice_by_index(ctx, attempt.branchIndex,
                                            attempt.restartRetryCursorOffset,
                                            attempt.restartReplayMode);
    case ChoiceAttemptKind::Editable:
      ctx.restoreFurthestExploredCursor(entryFurthestExploredCursor);
      if (replay_editable_choice_by_index(ctx, attempt.branchIndex)) {
        PEGIUM_RECOVERY_TRACE("[choice rule] editable success offset=",
                              ctx.cursorOffset());
        return true;
      }
      ctx.rewind(entryCheckpoint);
      return false;
    case ChoiceAttemptKind::None:
      return false;
    }
    return false;
  }

  template <std::size_t... Is, typename Checkpoint>
  std::array<ChoiceAttempt, sizeof...(Elements)> observe_no_edit_choice_attempts(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset, std::index_sequence<Is...>) const {
    std::array<ChoiceAttempt, sizeof...(Elements)> noEditAttempts{};
    ((noEditAttempts[Is] = evaluate_branch_no_edit_choice_attempt<Is>(
          ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
          parseStartOffset)),
     ...);
    return noEditAttempts;
  }

  [[nodiscard]] static bool any_choice_branch_has_strict_start_signal(
      const std::array<ChoiceAttempt, sizeof...(Elements)> &noEditAttempts) noexcept {
    return std::ranges::any_of(
        noEditAttempts, [](const ChoiceAttempt &attempt) noexcept {
          return attempt.recovery.matched || attempt.startedWithoutEdits;
        });
  }

  [[nodiscard]] static ChoiceAttempt select_best_no_edit_choice_attempt(
      const std::array<ChoiceAttempt, sizeof...(Elements)> &noEditAttempts,
      TextOffset parseStartOffset) noexcept {
    ChoiceAttempt bestAttempt;
    for (const auto &noEditAttempt : noEditAttempts) {
      consider_choice_attempt(bestAttempt, noEditAttempt, parseStartOffset);
    }
    return bestAttempt;
  }

  template <std::size_t... Is, typename Checkpoint>
  ChoiceAttempt select_choice_attempt(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset,
      const std::array<ChoiceAttempt, sizeof...(Elements)> &noEditAttempts,
      bool hasStrictStartSignal,
      ChoiceAttempt bestAttempt, std::index_sequence<Is...>) const {
    const bool bestAttemptPreservesCleanBoundary =
        hasStrictStartSignal &&
        detail::preserves_clean_no_edit_choice_boundary(bestAttempt.recovery,
                                                        parseStartOffset);
    (([&]() {
       const bool branchHasStrictStartSignal =
           noEditAttempts[Is].recovery.matched ||
           noEditAttempts[Is].startedWithoutEdits;
       if (bestAttemptPreservesCleanBoundary && !branchHasStrictStartSignal) {
         return;
       }
       const auto attempt = evaluate_branch_choice_attempt<Is>(
           ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
           parseStartOffset, noEditAttempts[Is],
           /*allowRestartRetry=*/true, hasStrictStartSignal,
           branchHasStrictStartSignal);
       consider_choice_attempt(bestAttempt, attempt, parseStartOffset);
     }()),
     ...);
    return bestAttempt;
  }

  template <std::size_t... Is, typename Checkpoint>
  ChoiceAttempt collect_choice_attempts(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset, std::index_sequence<Is...>) const {
    const auto noEditAttempts = observe_no_edit_choice_attempts(
        ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
        parseStartOffset, std::index_sequence<Is...>{});
    const bool hasStrictStartSignal =
        any_choice_branch_has_strict_start_signal(noEditAttempts);
    const auto bestNoEditAttempt = select_best_no_edit_choice_attempt(
        noEditAttempts, parseStartOffset);
    return select_choice_attempt(
        ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
        parseStartOffset, noEditAttempts, hasStrictStartSignal,
        bestNoEditAttempt,
        std::index_sequence<Is...>{});
  }

  template <std::size_t I, typename Checkpoint>
  ChoiceAttempt evaluate_branch_no_edit_choice_attempt(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset) const {
    const auto visibleLeafCountBefore = ctx.failureHistorySize();
    if (!attempt_parse_no_edits(ctx, std::get<I>(choices))) {
      ctx.rewind(entryCheckpoint);
      return {.branchIndex = I,
              .startedWithoutEdits = branch_starts_without_edits<I>(ctx)};
    }
    const auto candidate =
        capture_choice_candidate(ctx, baseEditCost, baseRecoveryEditCount);
    const auto probe = detail::capture_recovery_probe_progress(
        ctx, visibleLeafCountBefore);
    if (probe.deferred()) {
      const bool preserveCleanSuffix =
          baseRecoveryEditCount != 0u &&
          detail::preserves_clean_no_edit_choice_boundary(
              candidate, parseStartOffset);
      if (probe.committedOffset >= ctx.pendingRecoveryWindowBeginOffset() &&
          probe.committedOffset < ctx.pendingRecoveryWindowMaxCursorOffset() &&
          !preserveCleanSuffix) {
        PEGIUM_RECOVERY_TRACE("[choice rule] strict deferred blocked offset=",
                              probe.committedOffset,
                              " furthest=", probe.furthestExploredOffset);
        ctx.rewind(entryCheckpoint);
        return {};
      }
      PEGIUM_RECOVERY_TRACE("[choice rule] strict success deferred offset=",
                            probe.committedOffset,
                            " furthest=", probe.furthestExploredOffset);
    } else {
      PEGIUM_RECOVERY_TRACE("[choice rule] strict success offset=",
                            ctx.cursorOffset());
    }
    ctx.rewind(entryCheckpoint);
    return {
        .branchIndex = I,
        .kind = ChoiceAttemptKind::NoEditReplay,
        .recovery = candidate,
    };
  }

  template <std::size_t I, typename Checkpoint>
  ChoiceAttempt evaluate_branch_choice_attempt(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset,
      const ChoiceAttempt &noEditAttempt,
      bool allowRestartRetry = true,
      bool hasStrictStartSignal = false,
      bool branchHasStrictStartSignal = false) const {
    bool branchHasEntryStartSignal = false;
    if (hasStrictStartSignal && !branchHasStrictStartSignal) {
      const auto checkpoint = ctx.mark();
      const char *const savedFurthestExploredCursor =
          ctx.furthestExploredCursor();
      branchHasEntryStartSignal =
          probe_recoverable_at_entry(std::get<I>(choices), ctx);
      ctx.rewind(checkpoint);
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      if (!branchHasEntryStartSignal) {
        return {};
      }
    }
    auto bestAttempt = noEditAttempt;
    const auto editableAttempt = evaluate_branch_local_editable_choice_attempt<I>(
        ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
        parseStartOffset, allowRestartRetry);
    if (editableAttempt.recovery.matched &&
        (!bestAttempt.recovery.matched ||
         detail::is_better_choice_recovery_candidate(
             editableAttempt.recovery, bestAttempt.recovery,
             {.parseStartOffset = parseStartOffset}))) {
      bestAttempt = editableAttempt;
    }
    if (!hasStrictStartSignal) {
      const bool matched = bestAttempt.recovery.matched;
      const bool noEditAttempt = matched && bestAttempt.recovery.editCount == 0u;
      const bool continuesAfterFirstEdit =
          matched && detail::continues_after_first_edit(bestAttempt.recovery);
      const bool forbiddenWithoutStrictStartSignal =
          matched && bestAttempt.recovery.hasDeleteEdit &&
          !continuesAfterFirstEdit;
      const bool requiresEntryStartSignal =
          matched && !noEditAttempt &&
          bestAttempt.recovery.firstEditOffset <= parseStartOffset &&
          !(bestAttempt.recovery.firstEditOffset == parseStartOffset &&
            !bestAttempt.recovery.hasDeleteEdit &&
            continuesAfterFirstEdit);
      if (requiresEntryStartSignal) {
        const auto checkpoint = ctx.mark();
        const char *const savedFurthestExploredCursor =
            ctx.furthestExploredCursor();
        branchHasEntryStartSignal =
            probe_recoverable_at_entry(std::get<I>(choices), ctx);
        ctx.rewind(checkpoint);
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      }
      const bool legalWithoutStrictStartSignal =
          matched && (noEditAttempt ||
                      (!forbiddenWithoutStrictStartSignal &&
                       (branchHasEntryStartSignal ||
                        bestAttempt.recovery.firstEditOffset >
                            parseStartOffset ||
                        (bestAttempt.recovery.firstEditOffset ==
                             parseStartOffset &&
                         !bestAttempt.recovery.hasDeleteEdit &&
                         continuesAfterFirstEdit))));
      if (!legalWithoutStrictStartSignal) {
        return {};
      }
    }
    return bestAttempt;
  }

  template <std::size_t I, typename Checkpoint>
  ChoiceAttempt evaluate_branch_local_editable_choice_attempt(
      RecoveryContext &ctx, const Checkpoint &entryCheckpoint,
      std::uint32_t baseEditCost, std::size_t baseRecoveryEditCount,
      TextOffset parseStartOffset, bool allowRestartRetry = true) const {
    const auto editableCandidate = detail::evaluate_editable_recovery_candidate(
        ctx, entryCheckpoint, baseEditCost, baseRecoveryEditCount,
        [this, &ctx]() { return attempt_parse_editable(ctx, std::get<I>(choices)); });
    const ChoiceAttempt editableAttempt{
        .branchIndex = I,
        .kind = ChoiceAttemptKind::Editable,
        .recovery = editableCandidate,
    };
    const ChoiceAttempt restartAttempt =
        (allowRestartRetry && ctx.isInRecoveryPhase() && ctx.allowDeleteRetry)
            ? evaluate_restart_choice_attempt<I>(
                  ctx, baseEditCost, baseRecoveryEditCount, parseStartOffset)
            : ChoiceAttempt{};
    if (restartAttempt.recovery.matched &&
        (!editableAttempt.recovery.matched ||
         detail::is_better_choice_recovery_candidate(
             restartAttempt.recovery, editableAttempt.recovery,
             {.parseStartOffset = parseStartOffset}))) {
      return restartAttempt;
    }
    return editableAttempt;
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

  template <std::size_t I = 0>
  bool replay_expect_branch_by_index(ExpectContext &ctx,
                                     std::size_t bestIndex) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return parser::replay_expect_branch(ctx, std::get<I>(choices));
      }
      return replay_expect_branch_by_index<I + 1>(ctx, bestIndex);
    }
  }

};

template <Expression... Elements>
struct OrderedChoiceWithSkipper final : OrderedChoice<Elements...>,
                                        CompletionSkipperProvider {
  using Base = OrderedChoice<Elements...>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit OrderedChoiceWithSkipper(const Base &base, Skipper localSkipper)
      : Base(base), _localSkipper(std::move(localSkipper)) {}
  explicit OrderedChoiceWithSkipper(Base &&base, Skipper localSkipper)
      : Base(std::move(base)), _localSkipper(std::move(localSkipper)) {}

  OrderedChoiceWithSkipper(OrderedChoiceWithSkipper &&) noexcept = default;
  OrderedChoiceWithSkipper(const OrderedChoiceWithSkipper &) = default;
  OrderedChoiceWithSkipper &
  operator=(OrderedChoiceWithSkipper &&) noexcept = default;
  OrderedChoiceWithSkipper &
  operator=(const OrderedChoiceWithSkipper &) = default;
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
