#pragma once

#include <array>
#include <concepts>
#include <limits>
#include <optional>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <string>

namespace pegium::parser {

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct RepetitionWithSkipper;

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct Repetition : grammar::Repetition {
  static constexpr bool nullable = min == 0;
  static constexpr bool isFailureSafe =
      min == 0 ||
      (min == 1 &&
       (max != 1 || std::remove_cvref_t<Element>::isFailureSafe));
  static_assert(!(min == 0 && max == 0),
                "A Repetition cannot have both min and max set to 0.");

  explicit constexpr Repetition(Element &&element)
    requires(!std::is_lvalue_reference_v<Element>)
      : _element(std::move(element)) {}

  explicit constexpr Repetition(Element element)
    requires(std::is_lvalue_reference_v<Element>)
      : _element(element) {}

  constexpr Repetition(Repetition &&) noexcept = default;
  constexpr Repetition(const Repetition &) = default;
  constexpr Repetition &operator=(Repetition &&) noexcept = default;
  constexpr Repetition &operator=(const Repetition &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  friend struct detail::InitAccess;

  static constexpr bool is_optional = (min == 0 && max == 1);
  static constexpr bool is_star =
      (min == 0 && max == std::numeric_limits<std::size_t>::max());
  static constexpr bool is_plus =
      (min == 1 && max == std::numeric_limits<std::size_t>::max());
  static constexpr bool is_fixed = (min == max && min > 0);

  struct ExpectIterationResult {
    bool matched = false;
    bool blocked = false;
    bool progressed = false;
  };

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      if constexpr (is_optional) {
        (void)attempt_parse_strict(ctx, _element);
        return true;
      } else if constexpr (is_star) {
        if (!attempt_parse_strict(ctx, _element)) {
          return true;
        }
        auto checkpointAfterItem = ctx.mark();
        while (true) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
      } else if constexpr (is_plus) {
        if (!attempt_parse_strict(ctx, _element)) {
          return false;
        }
        auto checkpointAfterItem = ctx.mark();
        while (true) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
      } else if constexpr (is_fixed) {
        if (!parse(_element, ctx)) {
          return false;
        }
        for (std::size_t repetitionIndex = 1; repetitionIndex < min;
             ++repetitionIndex) {
          ctx.skip();
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        return true;
      } else if constexpr (min == 0) {
        if (!attempt_parse_strict(ctx, _element)) {
          return true;
        }
        auto checkpointAfterItem = ctx.mark();
        std::size_t repetitionCount = 1;
        for (; repetitionCount < max; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
        return true;
      } else {
        if (!attempt_parse_strict(ctx, _element)) {
          return false;
        }
        auto checkpointAfterItem = ctx.mark();
        std::size_t repetitionCount = 1;
        for (; repetitionCount < min; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return false;
          }
          checkpointAfterItem = ctx.mark();
        }

        for (; repetitionCount < max; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
        return true;
      }
    } else if constexpr (RecoveryParseModeContext<Context>) {
      if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows()) {
        TrackedParseContext &strictCtx = ctx;
        return parse_impl(strictCtx);
      }
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::RepetitionRecoverCalls);
      if constexpr (is_optional) {
        const auto checkpoint = ctx.mark();
        const char *const savedFurthestExploredCursor =
            ctx.furthestExploredCursor();
        const auto noEditObservation = observe_no_edit_parse(ctx, _element);
        if (noEditObservation.matched) {
          return finalize_optional_strict_success(ctx, checkpoint,
                                                  savedFurthestExploredCursor);
        }
        ctx.rewind(checkpoint);
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
        PEGIUM_STEP_TRACE_INC(detail::StepCounter::RepetitionFastAttempts);
        const bool startedWithoutEdits =
            noEditObservation.startedWithoutEdits;
        PEGIUM_STEP_TRACE_INC(
            startedWithoutEdits ? detail::StepCounter::RepetitionFastSuccess
                                : detail::StepCounter::RepetitionFastFailures);
        const bool entryRecoverable = has_entry_recovery_signal(ctx);
        if (!(startedWithoutEdits || entryRecoverable)) {
          return true;
        }
        if (try_recovery_iteration(ctx, /*skipBetweenIterations=*/false)) {
          return true;
        }
        ctx.rewind(checkpoint);
        if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
        }
        // A speculative entry-recovery signal alone must not force an optional
        // branch to fail. If recovery cannot materialize a real started branch,
        // the optional remains skippable.
        return !startedWithoutEdits;
      } else if constexpr (is_star) {
        PEGIUM_RECOVERY_TRACE("[repeat * rule] enter offset=",
                              ctx.cursorOffset());
        const bool matched = parse_zero_min_repetition_recovery(
            ctx, std::numeric_limits<std::size_t>::max());
        PEGIUM_RECOVERY_TRACE(
            matched ? "[repeat * rule] stop offset="
                    : "[repeat * rule] first element failed offset=",
            ctx.cursorOffset());
        return matched;
      } else if constexpr (is_plus) {
        if (!try_recovery_iteration(ctx, /*skipBetweenIterations=*/false)) {
          PEGIUM_RECOVERY_TRACE("[repeat + rule] first element failed offset=",
                                ctx.cursorOffset());
          return false;
        }
        PEGIUM_RECOVERY_TRACE("[repeat + rule] first element ok offset=",
                              ctx.cursorOffset());
        while (true) {
          if (!try_recovery_iteration(ctx, /*skipBetweenIterations=*/true)) {
            if (ctx.frontierBlocked()) {
              if (blocked_frontier_stops_cleanly_after_window_progress(ctx)) {
                ctx.clearFrontierBlock();
                break;
              }
              return false;
            }
            break;
          }
          PEGIUM_RECOVERY_TRACE("[repeat + rule] element matched offset=",
                                ctx.cursorOffset());
        }
        PEGIUM_RECOVERY_TRACE("[repeat + rule] stop offset=", ctx.cursorOffset());
        return true;
      } else if constexpr (is_fixed) {
        for (std::size_t repetitionIndex = 0; repetitionIndex < min;
             ++repetitionIndex) {
          if (repetitionIndex > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        return true;
      } else {
        std::size_t repetitionCount = 0;
        for (; repetitionCount < min; ++repetitionCount) {
          if (repetitionCount > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        if constexpr (min == 0) {
          return parse_zero_min_repetition_recovery(ctx, max);
        }
        for (; repetitionCount < max; ++repetitionCount) {
          if (!try_recovery_iteration(ctx, repetitionCount > 0)) {
            if (ctx.frontierBlocked()) {
              if (blocked_frontier_stops_cleanly_after_window_progress(ctx)) {
                ctx.clearFrontierBlock();
                break;
              }
              return false;
            }
            break;
          }
        }
        return true;
      }
    } else {
      if constexpr (is_optional) {
        if (const auto result = try_expect_iteration(ctx, /*skipBefore=*/false);
            result.matched && result.blocked) {
          ctx.clearFrontierBlock();
        }
        return true;
      } else if constexpr (is_star) {
        while (true) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/false);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
          ctx.skip();
        }
      } else if constexpr (is_plus) {
        const auto first = try_expect_iteration(ctx, /*skipBefore=*/false);
        if (!first.matched) {
          return false;
        }
        if (first.blocked) {
          return true;
        }
        while (true) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/true);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
        }
      } else {
        std::size_t repetitionCount = 0;
        for (; repetitionCount < min; ++repetitionCount) {
          if (repetitionCount > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
          if (ctx.frontierBlocked()) {
            return true;
          }
        }
        for (; repetitionCount < max; ++repetitionCount) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/true);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
        }
        return true;
      }
    }
  }

public:
  bool probeRecoverable(RecoveryContext &ctx) const {
    return attempt_fast_probe(ctx, _element) ||
           probe_locally_recoverable(_element, ctx);
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    if (attempt_fast_probe(ctx, _element)) {
      return true;
    }
    return probe_recoverable_at_entry(_element, ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const { parser::init(_element, ctx); }

private:
  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return attempt_fast_probe(ctx, _element);
  }

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    return attempt_fast_probe(ctx, _element);
  }

  enum class IterationReplayKind : std::uint8_t {
    NoInsert,
    InsertRetry,
    DeleteRetry,
  };

  enum class IterationRecoveryStrength : std::uint8_t {
    Strong,
    Weak,
  };

  struct IterationAttempt {
    IterationReplayKind kind = IterationReplayKind::NoInsert;
    detail::StructuralProgressRecoveryCandidate candidate{};
    bool hasDestructiveEdits = false;
    bool mutatesDestructiveSuffixBeyondStrictExploration = false;
  };

  struct IterationObservation {
    IterationAttempt noInsertAttempt{};
    detail::RecoveryProbeProgress noInsertProbe{};
    bool noInsertProgressed = false;
    bool startedWithoutEdits = false;
    bool iterationStarted = false;
    bool failedInActiveWindow = false;
  };

  struct IterationReplayPlan {
    IterationReplayKind kind = IterationReplayKind::NoInsert;
    bool legal = false;
    bool allowInsert = false;
    bool allowDelete = false;
    bool allowDestructiveWindowContinuation = false;
    bool scopeLeadingTerminalInsert = false;
    bool allowExtendedDeleteScan = false;
    bool protectParseStartBoundary = false;
    bool protectLaterVisibleBoundary = false;
  };

  using IterationPlanList = std::array<IterationReplayPlan, 4>;

  struct IterationSelectionResult {
    IterationAttempt bestAttempt{};
    IterationReplayPlan winningPlan{
        .kind = IterationReplayKind::NoInsert,
    };
    bool blockedByBoundaryUnsafeRetry = false;
  };

  [[nodiscard]] static RecoveryContext::RepetitionIterationMemoValue
  capture_iteration_memo_value(const IterationReplayPlan &plan, bool matched,
                               bool blockFrontier,
                               TextOffset observedFurthestExploredOffset) noexcept {
    return {
        .observedFurthestExploredOffset = observedFurthestExploredOffset,
        .kind = static_cast<std::uint8_t>(plan.kind),
        .matched = matched,
        .blockFrontier = blockFrontier,
        .allowInsert = plan.allowInsert,
        .allowDelete = plan.allowDelete,
        .allowDestructiveWindowContinuation =
            plan.allowDestructiveWindowContinuation,
        .scopeLeadingTerminalInsert = plan.scopeLeadingTerminalInsert,
        .allowExtendedDeleteScan = plan.allowExtendedDeleteScan,
        .protectParseStartBoundary = plan.protectParseStartBoundary,
        .protectLaterVisibleBoundary = plan.protectLaterVisibleBoundary,
    };
  }

  [[nodiscard]] static IterationReplayPlan replay_cached_iteration_plan(
      const RecoveryContext::RepetitionIterationMemoValue &cached) noexcept {
    return {
        .kind = static_cast<IterationReplayKind>(cached.kind),
        .legal = cached.matched,
        .allowInsert = cached.allowInsert,
        .allowDelete = cached.allowDelete,
        .allowDestructiveWindowContinuation =
            cached.allowDestructiveWindowContinuation,
        .scopeLeadingTerminalInsert = cached.scopeLeadingTerminalInsert,
        .allowExtendedDeleteScan = cached.allowExtendedDeleteScan,
        .protectParseStartBoundary = cached.protectParseStartBoundary,
        .protectLaterVisibleBoundary = cached.protectLaterVisibleBoundary,
    };
  }

  static void consider_iteration_attempt(IterationSelectionResult &selection,
                                         const IterationAttempt &candidate,
                                         const IterationReplayPlan &plan) {
    if (!candidate.candidate.matched) {
      return;
    }
    if (!selection.bestAttempt.candidate.matched ||
        detail::is_better_structural_progress_recovery_candidate(
            candidate.candidate, selection.bestAttempt.candidate)) {
      selection.bestAttempt = candidate;
      selection.winningPlan = plan;
    }
  }

  [[nodiscard]] static bool
  has_legal_iteration_retry_plan(const IterationPlanList &plans) noexcept {
    for (const auto &plan : plans) {
      if (plan.legal && plan.kind != IterationReplayKind::NoInsert) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] static constexpr std::uint8_t
  iteration_attempt_priority(IterationReplayKind kind) noexcept {
    switch (kind) {
    case IterationReplayKind::NoInsert:
      return 0u;
    case IterationReplayKind::InsertRetry:
      return 1u;
    case IterationReplayKind::DeleteRetry:
      return 2u;
    }
    return 2u;
  }

  [[nodiscard]] detail::StructuralProgressRecoveryCandidate
  capture_iteration_candidate(
      RecoveryContext &ctx,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      IterationReplayKind kind,
      TextOffset parseStartOffset,
      std::size_t recoveryEditCountBefore) const {
    bool continuesAfterFirstEdit = true;
    ParseDiagnosticKind firstEditKind = ParseDiagnosticKind::Incomplete;
    TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
    const grammar::AbstractElement *firstEditElement = nullptr;
    const bool hadEdits = ctx.recoveryEditCount() > recoveryEditCountBefore;
    if (hadEdits) {
      const auto edits = ctx.recoveryEditsView();
      const auto &firstEdit = edits[recoveryEditCountBefore];
      firstEditKind = firstEdit.kind;
      firstEditOffset = firstEdit.beginOffset;
      firstEditElement = firstEdit.element;
      TextOffset maxEndOffset = firstEdit.endOffset;
      for (std::size_t i = recoveryEditCountBefore + 1u; i < edits.size();
           ++i) {
        maxEndOffset = std::max(maxEndOffset, edits[i].endOffset);
      }
      continuesAfterFirstEdit =
          detail::post_skip_cursor_offset(ctx) > maxEndOffset;
    }
    return detail::make_structural_progress_recovery_candidate(
        detail::capture_progress_recovery_candidate(ctx, iterationCheckpoint),
        iteration_attempt_priority(kind), hadEdits, continuesAfterFirstEdit,
        hadEdits && firstEditOffset <= parseStartOffset, firstEditKind,
        firstEditOffset, firstEditElement);
  }

  [[nodiscard]] static constexpr IterationRecoveryStrength
  classify_iteration_attempt_strength(const IterationAttempt &attempt) noexcept {
    if (!attempt.candidate.matched || !attempt.candidate.hadEdits ||
        attempt.candidate.continuesAfterFirstEdit) {
      return IterationRecoveryStrength::Strong;
    }
    return IterationRecoveryStrength::Weak;
  }

  [[nodiscard]] static bool
  has_visible_remainder_after_current_cursor(const RecoveryContext &ctx) {
    if constexpr (requires { ctx.skip_without_builder(ctx.cursor()); }) {
      return ctx.skip_without_builder(ctx.cursor()) < ctx.end;
    }
    return ctx.cursor() < ctx.end;
  }

  [[nodiscard]] bool has_entry_recovery_signal(RecoveryContext &ctx) const {
    if (!has_visible_remainder_after_current_cursor(ctx)) {
      return false;
    }
    return probe_recoverable_at_entry(_element, ctx);
  }

  [[nodiscard]] bool
  is_locally_recoverable_after_iteration_skip(RecoveryContext &ctx) const {
    const auto checkpoint = ctx.mark();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    ctx.skip();
    const bool recoverable = probe_locally_recoverable(_element, ctx);
    ctx.rewind(checkpoint);
    ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    return recoverable;
  }

  bool finalize_optional_strict_success(RecoveryContext &ctx,
                                        const RecoveryContext::Checkpoint &checkpoint,
    const char *savedFurthestExploredCursor) const {
    if (!has_visible_remainder_after_current_cursor(ctx)) {
      if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      }
      return true;
    }

    const auto strictCursorOffset = ctx.cursorOffset();
    const auto strictExploredOffset =
        std::max(strictCursorOffset, ctx.furthestExploredOffset());
    const auto strictPostSkipOffset = detail::post_skip_cursor_offset(ctx);
    const bool strictFrontierIsRecoveryRelevant =
        ctx.canEditAtOffset(strictPostSkipOffset) ||
        (ctx.hasPendingRecoveryWindows() &&
         strictPostSkipOffset >= ctx.pendingRecoveryWindowBeginOffset() &&
         strictPostSkipOffset <= ctx.pendingRecoveryWindowMaxCursorOffset());
    if (!strictFrontierIsRecoveryRelevant) {
      if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      }
      return true;
    }

    ctx.rewind(checkpoint);
    ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    const auto baseEditCost = ctx.currentEditCost();
    const auto baseRecoveryEditCount = ctx.recoveryEditCount();
    if (baseRecoveryEditCount != 0u &&
        strictExploredOffset == strictCursorOffset) {
      const bool matchedStrictly = attempt_parse_no_edits(ctx, _element);
      if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      }
      return matchedStrictly;
    }
    // A strictly matched optional already established a local boundary. Only
    // let the editable replay compete when it repairs the continuation inside
    // the local frontier already explored by the strict parse, instead of
    // reopening the already matched prefix or speculating deeper into the
    // remainder.
    const auto editableCandidate = detail::evaluate_editable_recovery_candidate(
        ctx, checkpoint, baseEditCost, baseRecoveryEditCount,
        [this, &ctx]() { return parse(_element, ctx); });
    const bool editableRepairsWithinStrictFrontier =
        editableCandidate.firstEditOffset >= strictCursorOffset &&
        editableCandidate.firstEditOffset <= strictExploredOffset;
    const bool preferEditableMatch =
        editableCandidate.matched &&
        editableCandidate.editCount > 0u &&
        editableRepairsWithinStrictFrontier &&
        detail::continues_after_first_edit(editableCandidate) &&
        editableCandidate.postSkipCursorOffset > strictPostSkipOffset;
    if (preferEditableMatch) {
      return parse(_element, ctx);
    }

    const bool matchedStrictly = attempt_parse_no_edits(ctx, _element);
    if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    }
    return matchedStrictly;
  }

  [[nodiscard]] static bool failed_in_active_window(
      const RecoveryContext &ctx, TextOffset parseStartOffset,
      bool progressed) noexcept {
    return !progressed && ctx.hasPendingRecoveryWindows() &&
           parseStartOffset >= ctx.pendingRecoveryWindowBeginOffset() &&
           parseStartOffset <= ctx.pendingRecoveryWindowMaxCursorOffset();
  }

  [[nodiscard]] static bool iteration_starts_in_active_window(
      const RecoveryContext &ctx, TextOffset parseStartOffset) noexcept {
    return ctx.hasPendingRecoveryWindows() &&
           parseStartOffset >= ctx.pendingRecoveryWindowBeginOffset() &&
           parseStartOffset <= ctx.pendingRecoveryWindowMaxCursorOffset();
  }

  [[nodiscard]] static bool
  blocked_frontier_stops_cleanly_after_window_progress(
      const RecoveryContext &ctx) noexcept {
    return ctx.frontierBlocked() && ctx.hasHadEdits() &&
           ctx.hasPendingRecoveryWindows() &&
           ctx.furthestExploredOffset() >=
               ctx.pendingRecoveryWindowMaxCursorOffset();
  }

  bool replay_iteration(RecoveryContext &ctx, const IterationReplayPlan &plan,
                        const char *parseStart) const {
    if (!plan.legal) {
      return false;
    }
    switch (plan.kind) {
    case IterationReplayKind::NoInsert:
      return attempt_parse_no_edits(ctx, _element) && ctx.cursor() != parseStart;
    case IterationReplayKind::InsertRetry: {
      auto editGuard =
          ctx.withEditPermissions(plan.allowInsert, plan.allowDelete);
      (void)editGuard;
      std::optional<RecoveryContext::DestructiveWindowContinuationGuard>
          destructiveWindowContinuationGuard;
      if (plan.allowDestructiveWindowContinuation) {
        destructiveWindowContinuationGuard.emplace(
            ctx.withDestructiveWindowContinuation());
      }
      const auto parseWithCurrentPermissions = [this, &ctx, parseStart]() {
        return parse(_element, ctx) && ctx.cursor() != parseStart;
      };
      if (plan.scopeLeadingTerminalInsert) {
        auto leadingInsertGuard = ctx.withLeadingTerminalInsertScope();
        (void)leadingInsertGuard;
        return parseWithCurrentPermissions();
      }
      return parseWithCurrentPermissions();
    }
    case IterationReplayKind::DeleteRetry: {
      auto editGuard =
          ctx.withEditPermissions(plan.allowInsert, plan.allowDelete);
      (void)editGuard;
      std::optional<RecoveryContext::DestructiveWindowContinuationGuard>
          destructiveWindowContinuationGuard;
      if (plan.allowDestructiveWindowContinuation) {
        destructiveWindowContinuationGuard.emplace(
            ctx.withDestructiveWindowContinuation());
      }
      const auto recoveryCheckpoint = ctx.mark();
      const char *const savedRecoveryFurthestExploredCursor =
          ctx.furthestExploredCursor();
      const auto parseWithRetryPermissions = [this, &ctx, parseStart]() {
        return parse(_element, ctx) && ctx.cursor() != parseStart;
      };
      if (parseWithRetryPermissions()) {
        return true;
      }
      ctx.rewind(recoveryCheckpoint);
      ctx.restoreFurthestExploredCursor(savedRecoveryFurthestExploredCursor);
      return detail::recover_by_guarded_delete_retry(
          ctx,
          [this, &ctx]() {
            const auto retryCheckpoint = ctx.mark();
            const char *const savedFurthestExploredCursor =
                ctx.furthestExploredCursor();
            const bool plausibleRetryStart =
                attempt_fast_probe(ctx, _element) ||
                probe_recoverable_at_entry(_element, ctx) ||
                probe_locally_recoverable(_element, ctx);
            ctx.rewind(retryCheckpoint);
            ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
            return plausibleRetryStart;
          },
          [&ctx, &parseWithRetryPermissions]() {
            const auto retryCheckpoint = ctx.mark();
            const char *const savedFurthestExploredCursor =
                ctx.furthestExploredCursor();
            const bool matched = parseWithRetryPermissions();
            if (!matched) {
              ctx.rewind(retryCheckpoint);
              ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
            }
            return matched;
          },
          {.scan = {.allowOverflow = plan.allowExtendedDeleteScan}});
    }
    }
    return false;
  }

  [[nodiscard]] bool
  insert_retry_candidate_is_valid(RecoveryContext &ctx,
                                  const IterationReplayPlan &plan,
                                  std::size_t recoveryEditCountBefore) const {
    if (plan.scopeLeadingTerminalInsert) {
      const auto insertEditCount = static_cast<std::uint32_t>(
          ctx.recoveryEditCount() - recoveryEditCountBefore);
      bool continuesAfterFirstEdit = true;
      if (insertEditCount > 0u) {
        const auto edits = ctx.recoveryEditsView();
        continuesAfterFirstEdit =
            detail::post_skip_cursor_offset(ctx) >
            edits[recoveryEditCountBefore].endOffset;
      }
      if (insertEditCount != 1u || !continuesAfterFirstEdit) {
        return false;
      }
    }
    return true;
  }

  IterationAttempt evaluate_no_insert_iteration_attempt(
      RecoveryContext &ctx,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      const char *parseStart, TextOffset parseStartOffset) const {
    IterationAttempt attempt{.kind = IterationReplayKind::NoInsert};
    const auto recoveryEditCountBefore = ctx.recoveryEditCount();
    if (attempt_parse_no_edits(ctx, _element) && ctx.cursor() != parseStart) {
      attempt.candidate = capture_iteration_candidate(
          ctx, iterationCheckpoint, attempt.kind, parseStartOffset,
          recoveryEditCountBefore);
    }
    return attempt;
  }

  IterationObservation observe_iteration(
      RecoveryContext &ctx,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      const char *parseStart, TextOffset parseStartOffset) const {
    const auto visibleLeafCountBefore = ctx.failureHistorySize();
    IterationObservation observation{
        .noInsertAttempt = evaluate_no_insert_iteration_attempt(
            ctx, iterationCheckpoint, parseStart, parseStartOffset),
        .noInsertProbe = detail::capture_recovery_probe_progress(
            ctx, visibleLeafCountBefore),
    };
    observation.noInsertProgressed =
        observation.noInsertProbe.committedProgressed(parseStartOffset);
    observation.startedWithoutEdits =
        !observation.noInsertProgressed &&
        probe_started_without_edits(ctx, _element);
    observation.iterationStarted =
        observation.noInsertProbe.exploredBeyond(parseStartOffset) ||
        observation.startedWithoutEdits;
    observation.failedInActiveWindow =
        failed_in_active_window(ctx, parseStartOffset,
                                observation.noInsertProgressed);
    return observation;
  }

  [[nodiscard]] IterationPlanList enumerate_iteration_plans(
      RecoveryContext &ctx, bool skipBetweenIterations,
      const IterationObservation &observation,
      TextOffset parseStartOffset,
      std::size_t recoveryEditCountBefore,
      bool parseStartAtIterationEntryBoundary,
      bool deepStartedOptionalAfterPriorRecovery) const {
    const bool noInsertCandidateLegal =
        observation.noInsertAttempt.candidate.matched &&
        observation.noInsertProgressed;
    const bool firstZeroMinIteration = min == 0 && !skipBetweenIterations;
    const bool currentBoundaryLooksLikeStartedIteration =
        observation.startedWithoutEdits || observation.noInsertProgressed;
    const bool retryContinuesStartedIteration =
        observation.noInsertProgressed || observation.iterationStarted;
    const bool iterationTouchesActiveWindow =
        observation.failedInActiveWindow ||
        iteration_starts_in_active_window(ctx, parseStartOffset);
    const bool speculativeContinuedRetryAfterRecoveredIteration =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        ctx.hasHadEdits() &&
        observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !iterationTouchesActiveWindow;
    const bool speculativeSyntheticSingleLeafRetryAfterRecoveredIteration =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        ctx.hasHadEdits() &&
        !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess();
    const bool protectRecoveredIterationBoundary =
        skipBetweenIterations &&
        recoveryEditCountBefore > 0u &&
        parseStartAtIterationEntryBoundary;
    bool entryRecoverySignalKnown = false;
    bool entryRecoverySignal = false;
    const auto hasEntryRecoverySignal = [&]() {
      if (!entryRecoverySignalKnown) {
        entryRecoverySignal = has_entry_recovery_signal(ctx);
        entryRecoverySignalKnown = true;
      }
      return entryRecoverySignal;
    };
    const bool speculativeBoundaryRetryWithoutEntrySignal =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !iterationTouchesActiveWindow &&
        !hasEntryRecoverySignal();
    bool localRetrySignalKnown = false;
    bool localRetrySignal = false;
    const auto hasLocalRetrySignal = [&]() {
      if (!localRetrySignalKnown) {
        localRetrySignal = probe_locally_recoverable(_element, ctx);
        localRetrySignalKnown = true;
      }
      return localRetrySignal;
    };

    bool deleteRetryLegal = false;
    if (!noInsertCandidateLegal &&
        !speculativeBoundaryRetryWithoutEntrySignal &&
        !speculativeContinuedRetryAfterRecoveredIteration &&
        !speculativeSyntheticSingleLeafRetryAfterRecoveredIteration) {
      const bool entryRetryWithoutPriorEdits =
          skipBetweenIterations && !ctx.hasHadEdits() &&
          (observation.startedWithoutEdits || hasLocalRetrySignal());
      deleteRetryLegal =
          observation.iterationStarted || entryRetryWithoutPriorEdits;
      if (!deleteRetryLegal && firstZeroMinIteration && !ctx.hasHadEdits()) {
        deleteRetryLegal = hasLocalRetrySignal();
      }
      if (!deleteRetryLegal && !is_optional &&
          observation.failedInActiveWindow) {
        deleteRetryLegal =
            hasLocalRetrySignal() ||
            (observation.iterationStarted && hasEntryRecoverySignal());
      }
    }
    if (deepStartedOptionalAfterPriorRecovery) {
      // Once an optional iteration already made real strict progress after an
      // earlier recovery, deleting from the repetition boundary mostly
      // replays a wide outer search over content that should now be repaired
      // by the engaged inner structure instead.
      deleteRetryLegal = false;
    }
    bool insertRetryLegal =
        noInsertCandidateLegal && ctx.allowInsert;
    if (!insertRetryLegal) {
      insertRetryLegal = deleteRetryLegal;
    }
    if (speculativeSyntheticSingleLeafRetryAfterRecoveredIteration) {
      insertRetryLegal = false;
    }
    if (!insertRetryLegal && firstZeroMinIteration && !noInsertCandidateLegal &&
        !ctx.hasHadEdits()) {
      insertRetryLegal = hasEntryRecoverySignal();
    }
    if (!insertRetryLegal && !noInsertCandidateLegal && ctx.allowInsert &&
        !speculativeBoundaryRetryWithoutEntrySignal &&
        !speculativeContinuedRetryAfterRecoveredIteration &&
        !ctx.hasHadEdits()) {
      insertRetryLegal = is_locally_recoverable_after_iteration_skip(ctx);
    }
    IterationPlanList plans{
        IterationReplayPlan{
            .kind = IterationReplayKind::NoInsert,
            .legal = noInsertCandidateLegal,
        },
        IterationReplayPlan{
        .kind = IterationReplayKind::DeleteRetry,
        .legal = deleteRetryLegal,
        .allowDelete = deleteRetryLegal && ctx.allowDelete,
        .allowDestructiveWindowContinuation =
            deleteRetryLegal && retryContinuesStartedIteration &&
            ctx.hasHadEdits() && parseStartAtIterationEntryBoundary,
        .allowExtendedDeleteScan =
            deleteRetryLegal && detail::allows_extended_delete_scan(ctx),
        .protectParseStartBoundary =
            protectRecoveredIterationBoundary &&
            (observation.iterationStarted ||
             currentBoundaryLooksLikeStartedIteration),
        .protectLaterVisibleBoundary =
            protectRecoveredIterationBoundary &&
            observation.iterationStarted},
        IterationReplayPlan{
        .kind = IterationReplayKind::InsertRetry,
        .legal = insertRetryLegal,
        .allowInsert = insertRetryLegal,
        .allowDelete =
            insertRetryLegal && retryContinuesStartedIteration &&
            ctx.allowDelete,
        .allowDestructiveWindowContinuation =
            insertRetryLegal && retryContinuesStartedIteration &&
            ctx.hasHadEdits() && parseStartAtIterationEntryBoundary,
        .scopeLeadingTerminalInsert =
            insertRetryLegal && !retryContinuesStartedIteration,
        .protectParseStartBoundary =
            protectRecoveredIterationBoundary ||
            (!observation.noInsertProgressed &&
             parseStartAtIterationEntryBoundary &&
             currentBoundaryLooksLikeStartedIteration),
        .protectLaterVisibleBoundary =
            protectRecoveredIterationBoundary &&
            currentBoundaryLooksLikeStartedIteration},
        IterationReplayPlan{
            .kind = IterationReplayKind::InsertRetry,
        }};
    auto &insertRetryPlan = plans[2];
    auto &boundaryPreservingInsertRetryPlan = plans[3];
    if (insertRetryPlan.legal && insertRetryPlan.allowDelete &&
        parseStartAtIterationEntryBoundary &&
        currentBoundaryLooksLikeStartedIteration &&
        !deepStartedOptionalAfterPriorRecovery) {
      boundaryPreservingInsertRetryPlan = insertRetryPlan;
      boundaryPreservingInsertRetryPlan.allowDelete = false;
    }
    return plans;
  }

  IterationAttempt evaluate_retry_iteration_attempt(
      RecoveryContext &ctx, const IterationReplayPlan &plan,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      const RecoveryContext::Checkpoint &parseCheckpoint,
      const char *parseStart, const char *parseStartFurthestExploredCursor,
      TextOffset parseStartOffset, TextOffset strictExploredOffset,
      const char *&furthestExploredCursor,
      std::size_t recoveryEditCountBefore,
      bool &blockedByBoundaryUnsafeRetry) const {
    IterationAttempt attempt{.kind = plan.kind};
    const bool matched = replay_iteration(ctx, plan, parseStart);
    const bool valid =
        matched &&
        (plan.kind != IterationReplayKind::InsertRetry ||
         insert_retry_candidate_is_valid(ctx, plan, recoveryEditCountBefore));
    if (ctx.furthestExploredCursor() > furthestExploredCursor) {
      furthestExploredCursor = ctx.furthestExploredCursor();
    }
    if (valid) {
      attempt.candidate = capture_iteration_candidate(
          ctx, iterationCheckpoint, plan.kind, parseStartOffset,
          recoveryEditCountBefore);
      attempt.hasDestructiveEdits =
          has_destructive_retry_edits(ctx, recoveryEditCountBefore);
      attempt.mutatesDestructiveSuffixBeyondStrictExploration =
          mutates_destructive_suffix_beyond_strict_exploration(
              ctx, strictExploredOffset, recoveryEditCountBefore);
      const bool deletesProtectedSuffixWithoutVisibleContinuation =
          plan.protectLaterVisibleBoundary &&
          attempt.mutatesDestructiveSuffixBeyondStrictExploration &&
          !attempt.candidate.continuesAfterFirstEdit;
      const bool insertRetryDeletesProtectedSuffix =
          plan.protectLaterVisibleBoundary &&
          plan.kind == IterationReplayKind::InsertRetry &&
          has_destructive_retry_edits(ctx, recoveryEditCountBefore);
      const bool boundaryUnsafe =
          (plan.protectParseStartBoundary &&
           attempt.candidate.rewritesParseStartBoundary) ||
          deletesProtectedSuffixWithoutVisibleContinuation ||
          insertRetryDeletesProtectedSuffix;
      if (boundaryUnsafe) {
        blockedByBoundaryUnsafeRetry =
            blockedByBoundaryUnsafeRetry || attempt.candidate.matched;
        attempt = {};
        attempt.kind = plan.kind;
      }
    }
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    return attempt;
  }

  IterationSelectionResult select_iteration_attempt(
      RecoveryContext &ctx, const IterationPlanList &plans,
      const IterationObservation &observation,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      const RecoveryContext::Checkpoint &parseCheckpoint,
      const char *parseStart, const char *parseStartFurthestExploredCursor,
      TextOffset parseStartOffset,
      const char *&furthestExploredCursor,
      std::size_t recoveryEditCountBefore) const {
    IterationSelectionResult selection;
    for (const auto &plan : plans) {
      if (!plan.legal) {
        continue;
      }
      if (plan.kind == IterationReplayKind::NoInsert) {
        selection.bestAttempt = observation.noInsertAttempt;
        selection.winningPlan = plan;
        continue;
      }
      auto retryAttempt = evaluate_retry_iteration_attempt(
          ctx, plan, iterationCheckpoint, parseCheckpoint, parseStart,
          parseStartFurthestExploredCursor, parseStartOffset,
          observation.noInsertProbe.furthestExploredOffset,
          furthestExploredCursor, recoveryEditCountBefore,
          selection.blockedByBoundaryUnsafeRetry);
      consider_iteration_attempt(selection, retryAttempt, plan);
    }

    return selection;
  }

  [[nodiscard]] static bool
  mutates_destructive_suffix_beyond_strict_exploration(
      RecoveryContext &ctx, TextOffset strictExploredOffset,
      std::size_t recoveryEditCountBefore) {
    const auto edits = ctx.recoveryEditsView();
    return std::ranges::any_of(
        edits.begin() + static_cast<std::ptrdiff_t>(recoveryEditCountBefore),
        edits.end(), [strictExploredOffset](const auto &edit) noexcept {
          return (edit.kind == ParseDiagnosticKind::Deleted ||
                  edit.kind == ParseDiagnosticKind::Replaced) &&
                 edit.endOffset > strictExploredOffset;
        });
  }

  [[nodiscard]] static bool
  has_destructive_retry_edits(RecoveryContext &ctx,
                              std::size_t recoveryEditCountBefore) {
    const auto edits = ctx.recoveryEditsView();
    return std::ranges::any_of(
        edits.begin() + static_cast<std::ptrdiff_t>(recoveryEditCountBefore),
        edits.end(), [](const auto &edit) noexcept {
          return edit.kind == ParseDiagnosticKind::Deleted ||
                 edit.kind == ParseDiagnosticKind::Replaced;
        });
  }

  std::size_t continue_zero_min_recovery_run(
      RecoveryContext &ctx, std::size_t matchedCount,
      std::size_t maxIterationCount) const {
    while (matchedCount < maxIterationCount) {
      if (!try_recovery_iteration(ctx, /*skipBetweenIterations=*/true)) {
        break;
      }
      ++matchedCount;
    }
    return matchedCount;
  }

  bool parse_zero_min_repetition_recovery(RecoveryContext &ctx,
                                          std::size_t maxIterationCount) const {
    const auto checkpoint = ctx.mark();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::RepetitionFastAttempts);
    const bool startedWithoutEdits = probe_started_without_edits(ctx, _element);
    PEGIUM_STEP_TRACE_INC(
        startedWithoutEdits ? detail::StepCounter::RepetitionFastSuccess
                            : detail::StepCounter::RepetitionFastFailures);
    if (!try_recovery_iteration(ctx, /*skipBetweenIterations=*/false)) {
      if (ctx.frontierBlocked()) {
        ctx.rewind(checkpoint);
        if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
        }
        return false;
      }
      ctx.rewind(checkpoint);
      if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      }
      // Zero-min repetitions may stop cleanly when a speculative entry repair
      // does not turn into a real first iteration.
      return !startedWithoutEdits;
    }
    if (maxIterationCount > 1u) {
      (void)continue_zero_min_recovery_run(ctx, 1u, maxIterationCount);
    }
    if (ctx.frontierBlocked()) {
      if (blocked_frontier_stops_cleanly_after_window_progress(ctx)) {
        ctx.clearFrontierBlock();
        return true;
      }
      return false;
    }
    return true;
  }

  bool try_recovery_iteration(RecoveryContext &ctx,
                              bool skipBetweenIterations) const {
    const auto iterationCheckpoint = ctx.mark();
    const auto recoveryEditCountBefore = ctx.recoveryEditCount();
    const auto iterationEntryOffset = ctx.cursorOffset();
    const auto entryFurthestExploredOffset = ctx.furthestExploredOffset();
    const auto policySignature = detail::mix_recovery_memo_signature(
        ctx.recoveryPolicySignature(), ctx.recoveryObservationSignature());
    const auto activeRecoverySignature = ctx.activeRecoverySignature();
    const auto memoKey = RecoveryContext::RecoveryMemoKey{
        .queryKind =
            RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration,
        .purposeBits = static_cast<std::uint8_t>(skipBetweenIterations),
        .ownerIdentity = static_cast<const void *>(this),
        .cursorOffset = iterationEntryOffset,
        .furthestExploredOffset = entryFurthestExploredOffset,
        .policySignature = policySignature,
        .activeRecoverySignature = activeRecoverySignature,
    };
    RecoveryContext::RepetitionIterationMemoValue cachedIteration;
    if (ctx.memoTable().template tryGet<
            RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
            memoKey, cachedIteration)) {
      const char *const cachedFurthestExploredCursor =
          ctx.begin + cachedIteration.observedFurthestExploredOffset;
      if (cachedFurthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(cachedFurthestExploredCursor);
      }
      if (!cachedIteration.matched) {
        if (cachedIteration.blockFrontier) {
          ctx.blockFrontier();
        }
        return false;
      }
      if (skipBetweenIterations) {
        ctx.skip();
      }
      const auto parseCheckpoint = ctx.mark();
      const char *const parseStart = ctx.cursor();
      const auto cachedPlan = replay_cached_iteration_plan(cachedIteration);
      if (replay_iteration(ctx, cachedPlan, parseStart)) {
        if (cachedFurthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(cachedFurthestExploredCursor);
        }
        return true;
      }
      ctx.rewind(parseCheckpoint);
      ctx.rewind(iterationCheckpoint);
    }
    if (skipBetweenIterations) {
      ctx.skip();
    }
    const auto parseCheckpoint = ctx.mark();
    const char *const parseStart = ctx.cursor();
    const char *const parseStartFurthestExploredCursor =
        ctx.furthestExploredCursor();
    const TextOffset parseStartOffset = ctx.cursorOffset();
    const bool parseStartAtIterationEntryBoundary =
        detail::post_skip_cursor_offset(ctx) == parseStartOffset;
    const auto observation =
        observe_iteration(ctx, iterationCheckpoint, parseStart, parseStartOffset);
    const bool deepStartedOptionalAfterPriorRecovery =
        is_optional &&
        !skipBetweenIterations &&
        recoveryEditCountBefore > 0u &&
        parseStartAtIterationEntryBoundary &&
        observation.noInsertProgressed &&
        !observation.failedInActiveWindow &&
        !observation.noInsertProbe.exploredSingleVisibleLeafOrLess();
    const auto iterationPlans = enumerate_iteration_plans(
        ctx, skipBetweenIterations, observation, parseStartOffset,
        recoveryEditCountBefore,
        parseStartAtIterationEntryBoundary,
        deepStartedOptionalAfterPriorRecovery);
    const bool hasLegalRetryPlan =
        has_legal_iteration_retry_plan(iterationPlans);
    const char *furthestExploredCursor = ctx.furthestExploredCursor();
    if (iterationPlans.front().legal && !hasLegalRetryPlan) {
      ctx.memoTable().template store<
          RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
          memoKey,
          capture_iteration_memo_value(iterationPlans.front(), true,
                                       /*blockFrontier=*/false,
                                       ctx.furthestExploredOffset()));
      return true;
    }
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    if (!observation.noInsertProgressed &&
        !hasLegalRetryPlan) {
      ctx.rewind(iterationCheckpoint);
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      ctx.memoTable().template store<
          RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
          memoKey, capture_iteration_memo_value(
                       {.kind = IterationReplayKind::NoInsert},
                       /*matched=*/false, /*blockFrontier=*/false,
                       static_cast<TextOffset>(furthestExploredCursor -
                                               ctx.begin)));
      return false;
    }
    const auto selection = select_iteration_attempt(
        ctx, iterationPlans, observation, iterationCheckpoint, parseCheckpoint,
        parseStart, parseStartFurthestExploredCursor, parseStartOffset,
        furthestExploredCursor, recoveryEditCountBefore);
    const bool iterationTouchedActiveWindow =
        observation.failedInActiveWindow ||
        iteration_starts_in_active_window(ctx, parseStartOffset);
    if (!selection.bestAttempt.candidate.matched) {
      ctx.rewind(iterationCheckpoint);
      bool blockedFrontier = false;
      if (selection.blockedByBoundaryUnsafeRetry) {
        const bool mustBlockFrontier =
            !skipBetweenIterations || recoveryEditCountBefore == 0u ||
            iterationTouchedActiveWindow;
        if (mustBlockFrontier) {
          ctx.blockFrontier();
          blockedFrontier = true;
        }
      }
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      ctx.memoTable().template store<
          RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
          memoKey, capture_iteration_memo_value(
                       {.kind = IterationReplayKind::NoInsert},
                       /*matched=*/false, blockedFrontier,
                       static_cast<TextOffset>(furthestExploredCursor -
                                               ctx.begin)));
      return false;
    }
    const auto bestStrength =
        classify_iteration_attempt_strength(selection.bestAttempt);
    const bool weakRetryEscapesActiveWindow =
        skipBetweenIterations &&
        recoveryEditCountBefore > 0u &&
        selection.bestAttempt.kind != IterationReplayKind::NoInsert &&
        bestStrength == IterationRecoveryStrength::Weak &&
        !iterationTouchedActiveWindow;
    if (weakRetryEscapesActiveWindow) {
      ctx.rewind(iterationCheckpoint);
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      ctx.memoTable().template store<
          RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
          memoKey, capture_iteration_memo_value(
                       {.kind = IterationReplayKind::NoInsert},
                       /*matched=*/false, /*blockFrontier=*/false,
                       static_cast<TextOffset>(furthestExploredCursor -
                                               ctx.begin)));
      return false;
    }
    // Rebuild only the winning branch from the shared parse checkpoint instead
    // of keeping success checkpoints alive across competing recovery parses.
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    const bool replayed =
        replay_iteration(ctx, selection.winningPlan, parseStart);
    if (!replayed) {
      ctx.rewind(iterationCheckpoint);
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      ctx.memoTable().template store<
          RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
          memoKey, capture_iteration_memo_value(
                       selection.winningPlan, /*matched=*/false,
                       /*blockFrontier=*/false,
                       static_cast<TextOffset>(furthestExploredCursor -
                                               ctx.begin)));
      return false;
    }
    const auto observedFurthestExploredOffset = static_cast<TextOffset>(
        std::max(furthestExploredCursor, ctx.furthestExploredCursor()) -
        ctx.begin);
    ctx.memoTable().template store<
        RecoveryContext::RecoveryMemoQueryKind::RepetitionIteration>(
        memoKey, capture_iteration_memo_value(selection.winningPlan, true,
                                              /*blockFrontier=*/false,
                                              observedFurthestExploredOffset));
    return replayed;
  }

public:

  constexpr const char *terminal(const char *begin) const noexcept
    requires TerminalCapableExpression<Element>
  {
    // optional
    if constexpr (is_optional) {
      const char *matchEnd = _element.terminal(begin);
      return matchEnd != nullptr ? matchEnd : begin;
    }
    // zero or more
    else if constexpr (is_star) {
      const char *cursor = begin;
      while (true) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }
      return cursor;
    }
    // one or more
    else if constexpr (is_plus) {

      const char *cursor = _element.terminal(begin);
      if (cursor == nullptr)
        return cursor;

      while (true) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }
      return cursor;
    }
    // only min/max times
    else if constexpr (is_fixed) {
      const char *cursor = begin;
      for (std::size_t repetitionIndex = 0; repetitionIndex < min;
           ++repetitionIndex) {
        cursor = _element.terminal(cursor);
        if (cursor == nullptr) {
          return cursor;
        }
      }
      return cursor;
    }
    // other cases
    else {

      const char *cursor = begin;
      std::size_t repetitionCount = 0;
      for (; repetitionCount < min; ++repetitionCount) {
        cursor = _element.terminal(cursor);
        if (cursor == nullptr) {
          return cursor;
        }
      }

      for (; repetitionCount < max; ++repetitionCount) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }

      return cursor;
    }
  }
  constexpr const char *terminal(const std::string &text) const noexcept
    requires TerminalCapableExpression<Element>
  {
    return terminal(text.c_str());
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<ExpressionHolder<Element>>
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
            std::copy_constructible<ExpressionHolder<Element>>
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
    requires std::copy_constructible<ExpressionHolder<Element>>
  auto with_skipper(LocalSkipper &&localSkipper) const & {
    return RepetitionWithSkipper<min, max, Element>{
        *this, static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return RepetitionWithSkipper<min, max, Element>{
        std::move(*this),
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

private:
  ExpressionHolder<Element> _element;

  ExpectIterationResult try_expect_iteration(ExpectContext &ctx,
                                             bool skipBefore) const {
    const auto checkpoint = ctx.mark();
    if (skipBefore) {
      ctx.skip();
    }
    const auto parseCheckpoint = ctx.mark();
    if (!parse(_element, ctx)) {
      ctx.rewind(checkpoint);
      return {};
    }
    const auto frontier = capture_frontier_since(ctx, parseCheckpoint);
    return {
        .matched = true,
        .blocked = frontier.blocked,
        .progressed = ctx.cursor() != parseCheckpoint.cursor,
    };
  }

};

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct RepetitionWithSkipper final : Repetition<min, max, Element>,
                                     CompletionSkipperProvider {
  using Base = Repetition<min, max, Element>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit RepetitionWithSkipper(const Base &base, Skipper localSkipper)
      : Base(base), _localSkipper(std::move(localSkipper)) {}
  explicit RepetitionWithSkipper(Base &&base, Skipper localSkipper)
      : Base(std::move(base)), _localSkipper(std::move(localSkipper)) {}

  RepetitionWithSkipper(RepetitionWithSkipper &&) noexcept = default;
  RepetitionWithSkipper(const RepetitionWithSkipper &) = default;
  RepetitionWithSkipper &operator=(RepetitionWithSkipper &&) noexcept = default;
  RepetitionWithSkipper &operator=(const RepetitionWithSkipper &) = default;
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

/// Make the `element` optional (repeated zero or one)
/// @tparam Element the expression to repeat
/// @param element the element to be optional
/// @return a repetition of zero or one `element`.
template <NonNullableExpression Element>
constexpr auto option(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` zero or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of zero or more `element`.
template <NonNullableExpression Element>
constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of one or more `element`.
template <NonNullableExpression Element>
constexpr auto some(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more using a `separator`:
/// `element (separator element)*`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of one or more `element` with a `separator`.
template <NonNullableExpression Element, Expression Sep>
constexpr auto some(Element &&element, Sep &&separator) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(separator) + std::forward<Element>(element));
}

/// Repeat the `element` zero or more using a `separator`
/// `(element (separator element)*)?`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of zero or more `element` with a `separator`.
template <NonNullableExpression Element, Expression Sep>
constexpr auto many(Element &&element, Sep &&separator) {
  return option(
      some(std::forward<Element>(element), std::forward<Sep>(separator)));
}

/// Repeat the `element` `count` times.
/// @tparam count the count of repetitions
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `count` `element`.
template <std::size_t count, NonNullableExpression Element>
constexpr auto repeat(Element &&element) {
  return Repetition<count, count, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` between `min` and `max` times.
/// @tparam min the min number of occurence (inclusive)
/// @tparam max the max number of occurence (inclusive)
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `min` to `max` `element`.
template <std::size_t min, std::size_t max, NonNullableExpression Element>
constexpr auto repeat(Element &&element) {
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser
