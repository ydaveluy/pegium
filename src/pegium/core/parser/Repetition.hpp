#pragma once

#include <array>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/DataTypeRule.hpp>
#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/grammar/TerminalRule.hpp>
#include <pegium/core/parser/CandidateEnvelope.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/IterationBoundaryFactsBuilder.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <ranges>
#include <string>
#include <string_view>

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

  /// Upper bound on recovery candidates evaluated per iteration cycle.
  /// Mirrors the static `IterationPlanList` size; bumping one without
  /// the other will silently drop attempts.
  static constexpr std::size_t kMaxRecoveryCandidatesPerCall = 4U;

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
      if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows() &&
          !ctx.allowsCompletedWindowContinuationRecovery()) {
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
        const bool suffixRecoverableWithoutOptional =
            is_locally_recoverable_after_iteration_skip(ctx);
        // A speculative entry-recovery signal alone must not force an optional
        // branch to fail. Even when the optional started strictly, keep it
        // skippable if abandoning it preserves a recoverable suffix.
        return !startedWithoutEdits || suffixRecoverableWithoutOptional;
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

  bool probeRecoverableAtEntryConsumesVisible(RecoveryContext &ctx) const {
    if (ctx.probeFollowAcceptsHere()) {
      return false;
    }
    if (attempt_fast_probe(ctx, _element)) {
      return true;
    }
    if (probe_recoverable_at_entry_consumes_visible(_element, ctx)) {
      return true;
    }
    return probe_resync_entry_consumes_visible(ctx);
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
    /// Legacy candidate, retained because replay reads its mutable
    /// state (`cursorOffset`, `continuesAfterFirstEdit`,
    /// `rewritesParseStartBoundary`) and the dispatch's
    /// non-decision sites still consult it. The decision-relevant
    /// projection is mirrored on `envelope` and is the single
    /// channel the family-redundancy filters and the central
    /// `RecoveryKey` ranking read.
    detail::StructuralProgressRecoveryCandidate candidate{};
    /// Closed-vocabulary view consumed by admission,
    /// family-redundancy and ranking. Built from `candidate` via
    /// `to_candidate_envelope` at every construction site so the
    /// two stay in sync; the dispatch reads the envelope for
    /// decisions and never the legacy candidate for those.
    detail::CandidateEnvelope envelope{};
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

  /// Per-anchor family-redundancy filter (NOT replay-equivalence
  /// dominance): a no-edit iteration attempt is outranked by a
  /// candidate whose edits start exactly at the no-edit attempt's
  /// boundary, progress strictly further, and continue cleanly after
  /// the edit.
  ///
  /// The two attempts carry DIFFERENT scripts (one has edits, the
  /// other does not), so this is NOT replay-equivalence. The
  /// predicate names a structural preference inside the same-anchor
  /// family — an edited candidate that anchors at the no-edit
  /// boundary and overtakes it is preferred — applied as a
  /// redundancy filter before the central
  /// `is_better_recovery_key` ranking on `envelope.key`.
  ///
  /// Reads the closed `ReplayPrefixClass` set on every candidate at
  /// construction (`classify_structural_replay_prefix`): `candidate`
  /// must be non-`Empty` (it carries the extension), `committed`
  /// must be `Empty` (it is the no-edit reference). The central
  /// `RecoveryKey`-based ranking remains the only ordering authority
  /// for candidates that survive this filter.
  [[nodiscard]] static bool boundary_repair_outranks_no_edit_iteration(
      const IterationAttempt &candidate,
      const IterationAttempt &committed) noexcept {
    // Delegates to the free function in `CandidateEnvelope.hpp`. The
    // free function carries the `candidate.origin == committed.origin`
    // parent/child guard: dominance must not cross parent/child.
    return detail::boundary_repair_outranks_no_edit_iteration(
        candidate.envelope, committed.envelope);
  }

  /// Per-anchor family-redundancy filter (NOT replay-equivalence
  /// dominance, NOT a ranking comparator): `next` outranks `current`
  /// when both attempt destructive edits anchored at the same
  /// first-edit offset, `next` is classified
  /// `ReplayPrefixClass::ExtendedCommittedPrefix` (its replay prefix
  /// carries destructive edits — Deleted or Replaced — that
  /// strictly extend the committed-prefix family), `current` shares
  /// a non-`Empty` prefix at the same anchor, and `next` progresses
  /// strictly further at strictly higher cost while continuing
  /// after the edit.
  ///
  /// The predicate's two attempts carry DIFFERENT scripts (next has
  /// additional destructive edits over current), so this is NOT
  /// replay-equivalence. The predicate names a structural preference
  /// inside the same-anchor extension family, applied as a
  /// redundancy filter before the central
  /// `is_better_structural_progress_recovery_candidate` ranking,
  /// mirroring `OrderedChoice::extension_outranks_anchor_base`.
  ///
  /// Reads the closed `ReplayPrefixClass` set on every candidate at
  /// construction (`classify_structural_replay_prefix`); the legacy
  /// `next.hasDestructiveEdits` (on `IterationAttempt`) is captured
  /// as `next.candidate.replayPrefix == ExtendedCommittedPrefix`.
  [[nodiscard]] static bool
  destructive_extension_outranks_anchor_base(
      const IterationAttempt &next,
      const IterationAttempt &current) noexcept {
    // Delegates to the free function in `CandidateEnvelope.hpp`. The
    // free function carries the `next.origin == current.origin`
    // parent/child guard: dominance must not cross parent/child.
    return detail::destructive_extension_outranks_anchor_base(
        next.envelope, current.envelope);
  }

  /// Selects the better iteration attempt. Shape:
  /// `admission -> family-redundancy -> ranking`, mirroring
  /// `OrderedChoice::consider_choice_attempt`. The two
  /// family-redundancy filters
  /// (`destructive_extension_outranks_anchor_base` and
  /// `boundary_repair_outranks_no_edit_iteration`) are checked
  /// before the central ranking; they are NOT
  /// replay-equivalence dominance — they remove redundancy inside
  /// the same-anchor extension family.
  static void consider_iteration_attempt(IterationSelectionResult &selection,
                                         const IterationAttempt &candidate,
                                         const IterationReplayPlan &plan) {
    if (!candidate.envelope.key.matched) {
      return;
    }
    if (selection.bestAttempt.envelope.key.matched) {
      if (destructive_extension_outranks_anchor_base(candidate,
                                                    selection.bestAttempt)) {
        selection.bestAttempt = candidate;
        selection.winningPlan = plan;
        return;
      }
      if (destructive_extension_outranks_anchor_base(selection.bestAttempt,
                                                    candidate)) {
        return;
      }
      if (boundary_repair_outranks_no_edit_iteration(
              candidate, selection.bestAttempt)) {
        selection.bestAttempt = candidate;
        selection.winningPlan = plan;
        return;
      }
      if (boundary_repair_outranks_no_edit_iteration(
              selection.bestAttempt, candidate)) {
        return;
      }
    }
    if (!selection.bestAttempt.envelope.key.matched ||
        detail::is_better_recovery_key(candidate.envelope.key,
                                        selection.bestAttempt.envelope.key)) {
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

  [[nodiscard]] static std::optional<std::size_t>
  single_legal_iteration_retry_plan_index(
      const IterationPlanList &plans) noexcept {
    std::optional<std::size_t> singleIndex;
    for (std::size_t i = 0; i < plans.size(); ++i) {
      const auto &plan = plans[i];
      if (!plan.legal || plan.kind == IterationReplayKind::NoInsert) {
        continue;
      }
      if (singleIndex.has_value()) {
        return std::nullopt;
      }
      singleIndex = i;
    }
    return singleIndex;
  }

  [[nodiscard]] static constexpr bool
  boundary_insert_dominates_delete_retry(const IterationAttempt &attempt,
                                         TextOffset parseStartOffset) noexcept {
    return attempt.kind == IterationReplayKind::InsertRetry &&
           attempt.candidate.matched && attempt.candidate.hadEdits &&
           !attempt.candidate.hasDestructiveEdit &&
           attempt.candidate.firstEditOffset == parseStartOffset &&
           attempt.candidate.continuesAfterFirstEdit &&
           attempt.candidate.editCost == 1u;
  }

  [[nodiscard]] detail::StructuralProgressRecoveryCandidate
  capture_iteration_candidate(
      RecoveryContext &ctx,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      TextOffset parseStartOffset,
      std::size_t recoveryEditCountBefore) const {
    bool continuesAfterFirstEdit = true;
    TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
    const bool hadEdits = ctx.recoveryEditCount() > recoveryEditCountBefore;
    bool hasDestructiveEdit = false;
    bool rewritesParseStartBoundary = false;
    if (hadEdits) {
      const auto edits = ctx.recoveryEditsView();
      const auto &firstEdit = edits[recoveryEditCountBefore];
      firstEditOffset = firstEdit.beginOffset;
      hasDestructiveEdit =
          firstEdit.kind == ParseDiagnosticKind::Deleted ||
          firstEdit.kind == ParseDiagnosticKind::Replaced;
      rewritesParseStartBoundary =
          hasDestructiveEdit && firstEdit.beginOffset <= parseStartOffset;
      TextOffset maxEndOffset = firstEdit.endOffset;
      for (std::size_t i = recoveryEditCountBefore + 1u; i < edits.size();
           ++i) {
        const bool editIsDestructive =
            edits[i].kind == ParseDiagnosticKind::Deleted ||
            edits[i].kind == ParseDiagnosticKind::Replaced;
        hasDestructiveEdit = hasDestructiveEdit || editIsDestructive;
        rewritesParseStartBoundary =
            rewritesParseStartBoundary ||
            (editIsDestructive && edits[i].beginOffset <= parseStartOffset);
        maxEndOffset = std::max(maxEndOffset, edits[i].endOffset);
      }
      continuesAfterFirstEdit =
          detail::post_skip_cursor_offset(ctx) > maxEndOffset;
    }
    return detail::StructuralProgressRecoveryCandidate{
        .matched = true,
        .cursorOffset = ctx.cursorOffset(),
        .editCost = ctx.editCostDelta(iterationCheckpoint),
        .hadEdits = hadEdits,
        .hasDestructiveEdit = hasDestructiveEdit,
        .continuesAfterFirstEdit = continuesAfterFirstEdit,
        .rewritesParseStartBoundary = rewritesParseStartBoundary,
        .firstEditOffset = firstEditOffset,
        .replayPrefix = detail::classify_structural_replay_prefix(
            hadEdits, hasDestructiveEdit),
    };
  }

  [[nodiscard]] static constexpr IterationRecoveryStrength
  classify_iteration_attempt_strength(const IterationAttempt &attempt,
                                      TextOffset parseStartOffset) noexcept {
    // Weak retries are boundary fabrications: the first edit happens at the
    // iteration entry and no visible continuation proves ownership. If the
    // first edit is later than the iteration entry, the element already
    // consumed source before repairing its tail (for example a missing close
    // delimiter at EOF), so it is a real started iteration and may outlive the
    // active window.
    if (!attempt.candidate.matched || !attempt.candidate.hadEdits ||
        attempt.candidate.continuesAfterFirstEdit ||
        attempt.candidate.firstEditOffset > parseStartOffset) {
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

  [[nodiscard]] bool
  probe_resync_entry_consumes_visible(RecoveryContext &ctx) const {
    const auto maxDeletes = is_optional ? ctx.maxConsecutiveCodepointDeletes
                                        : ctx.maxResyncSkipBytes;
    if (!ctx.allowDelete || maxDeletes == 0u || ctx.cursor() >= ctx.end) {
      return false;
    }
    detail::ScopedBoolOverride destructiveContinuationGuard{
        ctx.allowDestructiveWindowContinuation,
        ctx.allowDestructiveWindowContinuation || ctx.hasHadEdits()};
    (void)destructiveContinuationGuard;
    detail::ProbeRestoreScope guard{ctx};
    bool consumesVisible = false;
    (void)detail::visit_guarded_delete_scan_positions(
        ctx, [&ctx]() { return !ctx.probeFollowAcceptsHere(); },
        [this, &ctx, &consumesVisible](const detail::DeleteScanVisitState &) {
          if (ctx.probeFollowAcceptsHere()) {
            return detail::DeleteScanVisitResult::Stop;
          }
          if (attempt_fast_probe(ctx, _element) ||
              probe_recoverable_at_entry_consumes_visible(_element, ctx)) {
            consumesVisible = true;
            return detail::DeleteScanVisitResult::Accept;
          }
          return detail::DeleteScanVisitResult::Continue;
        },
        {.scan = {.maxDeletes = maxDeletes,
                  .allowOverflow = !is_optional &&
                                   detail::allows_extended_delete_scan(ctx)}});
    return consumesVisible;
  }

  [[nodiscard]] static TextOffset protected_start_rewrite_span(
      const RecoveryContext &ctx, std::size_t recoveryEditCountBefore,
      TextOffset parseStartOffset) noexcept {
    TextOffset firstRewriteOffset = std::numeric_limits<TextOffset>::max();
    TextOffset lastRewriteOffset = parseStartOffset;
    const auto edits = ctx.recoveryEditsView();
    for (std::size_t i = recoveryEditCountBefore; i < edits.size(); ++i) {
      const auto &edit = edits[i];
      const bool editIsDestructive =
          edit.kind == ParseDiagnosticKind::Deleted ||
          edit.kind == ParseDiagnosticKind::Replaced;
      if (!editIsDestructive || edit.beginOffset > parseStartOffset) {
        continue;
      }
      firstRewriteOffset = std::min(firstRewriteOffset, edit.beginOffset);
      lastRewriteOffset = std::max(lastRewriteOffset, edit.endOffset);
    }
    if (firstRewriteOffset == std::numeric_limits<TextOffset>::max() ||
        lastRewriteOffset <= firstRewriteOffset) {
      return 0;
    }
    return lastRewriteOffset - firstRewriteOffset;
  }

  /// Aggregated facts derived from a single pass over the recovery
  /// edit suffix `[recoveryEditCountBefore .. end)`. Replaces four
  /// separate scans (`protected_started_iteration_rewrite_span`,
  /// `has_local_insert_delete_compensation`,
  /// `has_scoped_leading_insert_then_destructive_suffix_edit`,
  /// `protected_suffix_rewrite_span_after_insert`) previously called
  /// from `retry_attempt_is_boundary_safe`.
  struct RetryEditScanFacts {
    TextOffset startedIterationRewriteSpan = 0;
    bool hasLocalInsertDeleteCompensation = false;
    bool hasScopedLeadingInsertThenDestructiveSuffixEdit = false;
    TextOffset suffixRewriteSpanAfterInsert = 0;
  };

  [[nodiscard]] static RetryEditScanFacts scan_retry_edits(
      const RecoveryContext &ctx, std::size_t recoveryEditCountBefore,
      TextOffset parseStartOffset, TextOffset strictExploredOffset) noexcept {
    RetryEditScanFacts facts;
    const auto edits = ctx.recoveryEditsView();
    if (recoveryEditCountBefore >= edits.size()) {
      return facts;
    }
    TextOffset startedIterationFirst =
        std::numeric_limits<TextOffset>::max();
    TextOffset startedIterationLast = parseStartOffset;
    bool sawScopedLeadingInsertAtStart = false;
    bool sawInsertBeforeSuffix = false;
    TextOffset suffixRewriteFirst = std::numeric_limits<TextOffset>::max();
    TextOffset suffixRewriteLast = strictExploredOffset;
    for (std::size_t i = recoveryEditCountBefore; i < edits.size(); ++i) {
      const auto &edit = edits[i];
      const bool editIsDestructive =
          edit.kind == ParseDiagnosticKind::Deleted ||
          edit.kind == ParseDiagnosticKind::Replaced;
      const bool editIsInsert =
          edit.kind == ParseDiagnosticKind::Inserted;

      // startedIterationRewriteSpan: destructive edits whose
      // beginOffset lies in [parseStart, strictExplored).
      if (strictExploredOffset > parseStartOffset && editIsDestructive &&
          edit.beginOffset >= parseStartOffset &&
          edit.beginOffset < strictExploredOffset) {
        startedIterationFirst =
            std::min(startedIterationFirst, edit.beginOffset);
        startedIterationLast =
            std::max(startedIterationLast, edit.endOffset);
      }

      // hasScopedLeadingInsertThenDestructiveSuffixEdit
      if (editIsInsert && edit.beginOffset == parseStartOffset &&
          edit.endOffset == parseStartOffset) {
        sawScopedLeadingInsertAtStart = true;
      } else if (sawScopedLeadingInsertAtStart && editIsDestructive &&
                 edit.beginOffset > parseStartOffset) {
        facts.hasScopedLeadingInsertThenDestructiveSuffixEdit = true;
      }

      // suffixRewriteSpanAfterInsert: insert before strict suffix,
      // then destructive edit ending past strictExplored.
      if (editIsInsert && edit.beginOffset <= strictExploredOffset) {
        sawInsertBeforeSuffix = true;
      } else if (sawInsertBeforeSuffix && editIsDestructive &&
                 edit.endOffset > strictExploredOffset) {
        suffixRewriteFirst =
            std::min(suffixRewriteFirst, edit.beginOffset);
        suffixRewriteLast =
            std::max(suffixRewriteLast, edit.endOffset);
      }
    }
    if (startedIterationFirst !=
            std::numeric_limits<TextOffset>::max() &&
        startedIterationLast > startedIterationFirst) {
      facts.startedIterationRewriteSpan =
          startedIterationLast - startedIterationFirst;
    }
    if (suffixRewriteFirst != std::numeric_limits<TextOffset>::max() &&
        suffixRewriteLast > suffixRewriteFirst) {
      facts.suffixRewriteSpanAfterInsert =
          suffixRewriteLast - suffixRewriteFirst;
    }

    // hasLocalInsertDeleteCompensation: keep the legacy O(K²) check
    // because typical K is tiny (≤ 4 — see kMaxPostHocPruningEditCount).
    // Short-circuit: if there is no insert+later-delete pair, skip.
    bool hasInsert = false;
    bool hasLaterDelete = false;
    for (std::size_t i = recoveryEditCountBefore; i < edits.size(); ++i) {
      if (edits[i].kind == ParseDiagnosticKind::Inserted) {
        hasInsert = true;
      } else if (hasInsert &&
                 edits[i].kind == ParseDiagnosticKind::Deleted) {
        hasLaterDelete = true;
        break;
      }
    }
    if (hasInsert && hasLaterDelete) {
      for (std::size_t i = recoveryEditCountBefore;
           i < edits.size() && !facts.hasLocalInsertDeleteCompensation;
           ++i) {
        if (edits[i].kind != ParseDiagnosticKind::Inserted) {
          continue;
        }
        for (std::size_t j = i + 1u; j < edits.size(); ++j) {
          if (edits[j].kind != ParseDiagnosticKind::Deleted ||
              edits[i].beginOffset > edits[j].beginOffset) {
            continue;
          }
          if (elements_equivalent_for_replay(edits[i].element,
                                              edits[j].element) ||
              deleted_source_starts_with_inserted_literal(ctx, edits[i],
                                                          edits[j])) {
            facts.hasLocalInsertDeleteCompensation = true;
            break;
          }
        }
      }
    }
    return facts;
  }

  [[nodiscard]] static bool deleted_source_starts_with_inserted_literal(
      const RecoveryContext &ctx, const detail::SyntaxScriptEntry &inserted,
      const detail::SyntaxScriptEntry &deleted) noexcept {
    const auto inputSize = static_cast<TextOffset>(ctx.end - ctx.begin);
    if (inserted.element == nullptr ||
        inserted.element->getKind() != ElementKind::Literal ||
        deleted.beginOffset > inputSize || deleted.endOffset > inputSize) {
      return false;
    }
    const auto &literal =
        static_cast<const grammar::Literal &>(*inserted.element);
    const auto value = literal.getValue();
    if (value.empty() ||
        deleted.endOffset - deleted.beginOffset < value.size()) {
      return false;
    }
    return std::string_view(ctx.begin + deleted.beginOffset, value.size()) ==
           value;
  }

  [[nodiscard]] bool has_entry_recovery_signal(RecoveryContext &ctx) const {
    if (!has_visible_remainder_after_current_cursor(ctx)) {
      return false;
    }
    return probe_recoverable_at_entry(_element, ctx);
  }

  [[nodiscard]] bool
  is_locally_recoverable_after_iteration_skip(RecoveryContext &ctx) const {
    detail::ProbeRestoreScope guard{ctx};
    ctx.skip();
    return probe_locally_recoverable(_element, ctx);
  }

  [[nodiscard]] bool
  is_non_strictly_recoverable_after_iteration_skip(RecoveryContext &ctx) const {
    detail::ProbeRestoreScope guard{ctx};
    ctx.skip();
    const bool strictMatch = attempt_fast_probe(ctx, _element);
    return !strictMatch && probe_locally_recoverable(_element, ctx);
  }

  /// Walks the grammar subtree starting at `element` and returns whether
  /// the *first non-nullable, non-wrapping leaf* satisfies `LeafPred`.
  /// Mirrors the recursion the legacy walkers used in lockstep:
  ///
  ///   - `Assignment` / `TerminalRule` / `DataTypeRule` / `ParserRule`
  ///     unwrap to their inner element;
  ///   - `Group`: skip nullable children, recurse into the first
  ///     non-nullable;
  ///   - `OrderedChoice`: every branch must satisfy `LeafPred`;
  ///   - `Repetition`: only consider it if `min > 0`, then recurse
  ///     into its element;
  ///   - everything else delegates to `LeafPred(element)`.
  ///
  /// The depth cap matches the legacy cap of 24 — grammars deeper than
  /// that are pathological and the legacy walkers all returned `false`.
  template <typename LeafPred>
  [[nodiscard]] static bool walk_grammar_first_non_nullable_satisfies(
      const grammar::AbstractElement &element, LeafPred leafPred,
      std::uint8_t depth = 0) noexcept {
    if (depth > 24U) {
      return false;
    }
    const auto next = static_cast<std::uint8_t>(depth + 1U);
    switch (element.getKind()) {
    case ElementKind::Assignment:
      return walk_grammar_first_non_nullable_satisfies(
          *static_cast<const grammar::Assignment &>(element).getElement(),
          leafPred, next);
    case ElementKind::TerminalRule:
      return walk_grammar_first_non_nullable_satisfies(
          *static_cast<const grammar::TerminalRule &>(element).getElement(),
          leafPred, next);
    case ElementKind::DataTypeRule:
      return walk_grammar_first_non_nullable_satisfies(
          *static_cast<const grammar::DataTypeRule &>(element).getElement(),
          leafPred, next);
    case ElementKind::ParserRule:
      return walk_grammar_first_non_nullable_satisfies(
          *static_cast<const grammar::ParserRule &>(element).getElement(),
          leafPred, next);
    case ElementKind::Group: {
      const auto &group = static_cast<const grammar::Group &>(element);
      for (std::size_t i = 0; i < group.size(); ++i) {
        const auto &child = *group.get(i);
        if (child.isNullable()) {
          continue;
        }
        return walk_grammar_first_non_nullable_satisfies(child, leafPred,
                                                          next);
      }
      return false;
    }
    case ElementKind::OrderedChoice: {
      const auto &choice =
          static_cast<const grammar::OrderedChoice &>(element);
      if (choice.size() == 0U) {
        return false;
      }
      for (std::size_t i = 0; i < choice.size(); ++i) {
        if (!walk_grammar_first_non_nullable_satisfies(*choice.get(i),
                                                        leafPred, next)) {
          return false;
        }
      }
      return true;
    }
    case ElementKind::Repetition: {
      const auto &repetition =
          static_cast<const grammar::Repetition &>(element);
      return repetition.getMin() > 0U &&
             walk_grammar_first_non_nullable_satisfies(
                 *repetition.getElement(), leafPred, next);
    }
    default:
      return leafPred(element);
    }
  }

  [[nodiscard]] static bool starts_with_insertable_terminal(
      const grammar::AbstractElement &element) noexcept {
    return walk_grammar_first_non_nullable_satisfies(
        element, [](const grammar::AbstractElement &leaf) noexcept {
          if (leaf.getKind() != ElementKind::Literal) {
            return false;
          }
          const auto value =
              static_cast<const grammar::Literal &>(leaf).getValue();
          return detail::classify_literal_recovery_profile(value).allowsInsert();
        });
  }

  [[nodiscard]] static bool
  is_unassigned_lexical_tail(const grammar::AbstractElement &element,
                             std::uint8_t depth = 0) noexcept {
    return walk_grammar_first_non_nullable_satisfies(
        element,
        [](const grammar::AbstractElement &leaf) noexcept {
          switch (leaf.getKind()) {
          case ElementKind::TerminalRule:
          case ElementKind::AnyCharacter:
          case ElementKind::CharacterRange:
            return true;
          default:
            return false;
          }
        },
        depth);
  }

  [[nodiscard]] static bool
  starts_with_synthetic_terminal_then_unassigned_lexical_tail(
      const grammar::AbstractElement &element,
      std::uint8_t depth = 0) noexcept {
    if (depth > 24u) {
      return false;
    }
    switch (element.getKind()) {
    case ElementKind::DataTypeRule:
      return starts_with_synthetic_terminal_then_unassigned_lexical_tail(
          *static_cast<const grammar::DataTypeRule &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::TerminalRule:
      return starts_with_synthetic_terminal_then_unassigned_lexical_tail(
          *static_cast<const grammar::TerminalRule &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::Group: {
      const auto &group = static_cast<const grammar::Group &>(element);
      std::size_t i = 0;
      for (; i < group.size(); ++i) {
        const auto &child = *group.get(i);
        if (child.isNullable()) {
          continue;
        }
        if (!starts_with_insertable_terminal(child)) {
          return false;
        }
        ++i;
        break;
      }
      for (; i < group.size(); ++i) {
        const auto &child = *group.get(i);
        if (child.isNullable()) {
          continue;
        }
        return is_unassigned_lexical_tail(
            child, static_cast<std::uint8_t>(depth + 1u));
      }
      return false;
    }
    case ElementKind::Assignment:
    case ElementKind::ParserRule:
    case ElementKind::OrderedChoice:
    case ElementKind::Repetition:
    case ElementKind::Literal:
    case ElementKind::AndPredicate:
    case ElementKind::AnyCharacter:
    case ElementKind::CharacterRange:
    case ElementKind::Create:
    case ElementKind::InfixOperator:
    case ElementKind::InfixRule:
    case ElementKind::Nest:
    case ElementKind::NotPredicate:
    case ElementKind::UnorderedGroup:
      return false;
    }
    return false;
  }

  [[nodiscard]] static bool
  starts_with_required_literal_anchor(const grammar::AbstractElement &element,
                                      std::uint8_t depth = 0) noexcept {
    if (depth > 24u) {
      return false;
    }
    switch (element.getKind()) {
    case ElementKind::Literal:
      return !static_cast<const grammar::Literal &>(element).getValue().empty();
    case ElementKind::Assignment:
      return starts_with_required_literal_anchor(
          *static_cast<const grammar::Assignment &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::TerminalRule:
      return starts_with_required_literal_anchor(
          *static_cast<const grammar::TerminalRule &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::ParserRule:
      return starts_with_required_literal_anchor(
          *static_cast<const grammar::ParserRule &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::DataTypeRule:
      return starts_with_required_literal_anchor(
          *static_cast<const grammar::DataTypeRule &>(element).getElement(),
          static_cast<std::uint8_t>(depth + 1u));
    case ElementKind::Group: {
      const auto &group = static_cast<const grammar::Group &>(element);
      for (std::size_t i = 0; i < group.size(); ++i) {
        const auto &child = *group.get(i);
        if (starts_with_required_literal_anchor(
                child, static_cast<std::uint8_t>(depth + 1u))) {
          return true;
        }
        if (!child.isNullable()) {
          return false;
        }
      }
      return false;
    }
    case ElementKind::OrderedChoice: {
      const auto &choice = static_cast<const grammar::OrderedChoice &>(element);
      if (choice.size() == 0u) {
        return false;
      }
      for (std::size_t i = 0; i < choice.size(); ++i) {
        if (!starts_with_required_literal_anchor(
                *choice.get(i), static_cast<std::uint8_t>(depth + 1u))) {
          return false;
        }
      }
      return true;
    }
    case ElementKind::Repetition: {
      const auto &repetition =
          static_cast<const grammar::Repetition &>(element);
      return repetition.getMin() > 0u &&
             starts_with_required_literal_anchor(
                 *repetition.getElement(),
                 static_cast<std::uint8_t>(depth + 1u));
    }
    case ElementKind::AndPredicate:
    case ElementKind::AnyCharacter:
    case ElementKind::CharacterRange:
    case ElementKind::Create:
    case ElementKind::InfixOperator:
    case ElementKind::InfixRule:
    case ElementKind::Nest:
    case ElementKind::NotPredicate:
    case ElementKind::UnorderedGroup:
      return false;
    }
    return false;
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
    const bool committedReplayFrontierIsRelevant =
        ctx.hasPendingCommittedRecoveryEditWithin(strictCursorOffset,
                                                  strictPostSkipOffset);
    const bool strictFrontierIsRecoveryRelevant =
        committedReplayFrontierIsRelevant ||
        ctx.canEditAtOffset(strictPostSkipOffset) ||
        (ctx.hasPendingRecoveryWindows() &&
         strictPostSkipOffset >= ctx.pendingRecoveryWindowActivationOffset() &&
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
           parseStartOffset >= ctx.pendingRecoveryWindowActivationOffset() &&
           parseStartOffset <= ctx.pendingRecoveryWindowMaxCursorOffset();
  }

  [[nodiscard]] static bool iteration_starts_in_active_window(
      const RecoveryContext &ctx, TextOffset parseStartOffset) noexcept {
    return ctx.hasPendingRecoveryWindows() &&
           parseStartOffset >= ctx.pendingRecoveryWindowActivationOffset() &&
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

  [[nodiscard]] static bool
  no_insert_iteration_preserves_clean_suffix(
      const IterationObservation &observation,
      TextOffset parseStartOffset,
      std::size_t recoveryEditCountBefore) noexcept {
    return recoveryEditCountBefore != 0u &&
           observation.noInsertAttempt.candidate.matched &&
           !observation.noInsertAttempt.candidate.hadEdits &&
           observation.noInsertAttempt.candidate.cursorOffset > parseStartOffset;
  }

  [[nodiscard]] static bool
  no_insert_iteration_is_deferred_inside_active_window(
      const RecoveryContext &ctx, const IterationObservation &observation,
      TextOffset parseStartOffset,
      std::size_t recoveryEditCountBefore) noexcept {
    if (!observation.noInsertAttempt.candidate.matched ||
        observation.noInsertAttempt.candidate.hadEdits ||
        !observation.noInsertProbe.deferred() ||
        !ctx.hasPendingRecoveryWindows()) {
      return false;
    }
    if (observation.noInsertProbe.committedOffset <
            ctx.pendingRecoveryWindowBeginOffset() ||
        observation.noInsertProbe.committedOffset >=
            ctx.pendingRecoveryWindowMaxCursorOffset()) {
      return false;
    }
    return !no_insert_iteration_preserves_clean_suffix(
        observation, parseStartOffset, recoveryEditCountBefore);
  }

  [[nodiscard]] static bool
  parent_recoverable_follow_may_own_weak_started_iteration(
      const IterationObservation &observation, bool atLocalIterationBoundary,
      bool localEntryHasRequiredLiteralAnchor, bool parentFollowStrict,
      bool parentFollowRecoverableConsumesVisible,
      bool committedPrefixImposes) noexcept {
    return atLocalIterationBoundary && !localEntryHasRequiredLiteralAnchor &&
           observation.startedWithoutEdits && !observation.noInsertProgressed &&
           !parentFollowStrict && parentFollowRecoverableConsumesVisible &&
           !committedPrefixImposes;
  }

  bool replay_iteration(RecoveryContext &ctx, const IterationReplayPlan &plan,
                        const char *parseStart,
                        bool allowTrailingDeleteStop = false) const {
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
      detail::ScopedBoolOverride destructiveContinuationGuard{
          ctx.allowDestructiveWindowContinuation,
          plan.allowDestructiveWindowContinuation ||
              ctx.allowDestructiveWindowContinuation};
      (void)destructiveContinuationGuard;
      const auto parseWithCurrentPermissions = [this, &ctx, parseStart]() {
        return with_iteration_element_follow(ctx, [&]() {
          return parse(_element, ctx) && ctx.cursor() != parseStart;
        });
      };
      if (plan.scopeLeadingTerminalInsert) {
        detail::ScopedBoolOverride leadingInsertGuard{
            ctx.allowLeadingTerminalInsertScope, true};
        (void)leadingInsertGuard;
        return parseWithCurrentPermissions();
      }
      return parseWithCurrentPermissions();
    }
    case IterationReplayKind::DeleteRetry: {
      auto editGuard =
          ctx.withEditPermissions(plan.allowInsert, plan.allowDelete);
      (void)editGuard;
      detail::ScopedBoolOverride destructiveContinuationGuard{
          ctx.allowDestructiveWindowContinuation,
          plan.allowDestructiveWindowContinuation ||
              ctx.allowDestructiveWindowContinuation};
      (void)destructiveContinuationGuard;
      const auto parseWithRetryPermissions = [this, &ctx, parseStart]() {
        return with_iteration_element_follow(ctx, [&]() {
          return parse(_element, ctx) && ctx.cursor() != parseStart;
        });
      };
      {
        detail::ProbeRestoreScope recoveryGuard{ctx};
        if (parseWithRetryPermissions()) {
          recoveryGuard.commit();
          return true;
        }
      }
      const bool canDeleteToEofAsTrailingGarbage =
          allowTrailingDeleteStop && ctx._followProbeFn == nullptr &&
          ctx._recoverableFollowProbeFn == nullptr;
      return detail::recover_by_guarded_delete_retry(
          ctx,
          [this, &ctx, canDeleteToEofAsTrailingGarbage]() {
            detail::ProbeRestoreScope guard{ctx};
            return (canDeleteToEofAsTrailingGarbage &&
                    ctx.cursor() == ctx.end) ||
                   attempt_fast_probe(ctx, _element) ||
                   probe_recoverable_at_entry(_element, ctx) ||
                   probe_locally_recoverable(_element, ctx);
          },
          [&ctx, &parseWithRetryPermissions,
           canDeleteToEofAsTrailingGarbage]() {
            detail::ProbeRestoreScope guard{ctx};
            const bool matched = parseWithRetryPermissions();
            const bool cleanedTrailingVisibleGarbage =
                canDeleteToEofAsTrailingGarbage && ctx.cursor() == ctx.end;
            if (matched) {
              guard.commit();
            }
            return matched || cleanedTrailingVisibleGarbage;
          },
          {.scan = {.allowOverflow = plan.allowExtendedDeleteScan},
           .stopOverflowAtStructuredVisibleSource = true});
    }
    }
    return false;
  }

  struct IterationElementFollowProbe {
    const Repetition *self = nullptr;
    RecoveryContext::FollowProbeFn outerStrict = nullptr;
    const void *outerStrictData = nullptr;
    RecoveryContext::FollowProbeFn outerRecoverable = nullptr;
    const void *outerRecoverableData = nullptr;
    RecoveryContext::FollowProbeFn outerRecoverableConsumesVisible = nullptr;
    const void *outerRecoverableConsumesVisibleData = nullptr;

    static bool strict(RecoveryContext &ctx, const void *data) {
      const auto &probe =
          *static_cast<const IterationElementFollowProbe *>(data);
      if (attempt_fast_probe(ctx, probe.self->_element)) {
        return true;
      }
      return probe.outerStrict != nullptr &&
             probe.outerStrict(ctx, probe.outerStrictData);
    }

    static bool recoverable(RecoveryContext &ctx, const void *data) {
      const auto &probe =
          *static_cast<const IterationElementFollowProbe *>(data);
      if (attempt_fast_probe(ctx, probe.self->_element)) {
        return true;
      }
      return probe.outerRecoverable != nullptr &&
             probe.outerRecoverable(ctx, probe.outerRecoverableData);
    }

    static bool recoverableConsumesVisible(RecoveryContext &ctx,
                                           const void *data) {
      const auto &probe =
          *static_cast<const IterationElementFollowProbe *>(data);
      if (attempt_fast_probe(ctx, probe.self->_element)) {
        return true;
      }
      return probe.outerRecoverableConsumesVisible != nullptr &&
             probe.outerRecoverableConsumesVisible(
                 ctx, probe.outerRecoverableConsumesVisibleData);
    }
  };

  template <typename Fn>
  bool with_iteration_element_follow(RecoveryContext &ctx, Fn &&fn) const {
    IterationElementFollowProbe probe{
        .self = this,
        .outerStrict = ctx._followProbeFn,
        .outerStrictData = ctx._followProbeData,
        .outerRecoverable = ctx._recoverableFollowProbeFn,
        .outerRecoverableData = ctx._recoverableFollowProbeData,
        .outerRecoverableConsumesVisible =
            ctx._recoverableFollowConsumesVisibleProbeFn,
        .outerRecoverableConsumesVisibleData =
            ctx._recoverableFollowConsumesVisibleProbeData,
    };
    auto followGuard = ctx.withFollowProbe(
        &IterationElementFollowProbe::strict, &probe,
        &IterationElementFollowProbe::recoverable, &probe,
        &IterationElementFollowProbe::recoverableConsumesVisible, &probe);
    (void)followGuard;
    return std::forward<Fn>(fn)();
  }

  [[nodiscard]] bool insert_retry_candidate_is_valid(
      RecoveryContext &ctx, const IterationReplayPlan &plan,
      TextOffset parseStartOffset, std::size_t recoveryEditCountBefore) const {
    if (plan.scopeLeadingTerminalInsert) {
      const auto insertEditCount = static_cast<std::uint32_t>(
          ctx.recoveryEditCount() - recoveryEditCountBefore);
      if (insertEditCount == 0u) {
        return true;
      }
      const auto edits = ctx.recoveryEditsView();
      const auto &firstEdit = edits[recoveryEditCountBefore];
      const bool usedScopedLeadingInsert =
          firstEdit.kind == ParseDiagnosticKind::Inserted &&
          firstEdit.beginOffset == parseStartOffset &&
          firstEdit.endOffset == parseStartOffset;
      if (!usedScopedLeadingInsert) {
        return true;
      }
      bool continuesAfterFirstEdit = true;
      continuesAfterFirstEdit =
          detail::post_skip_cursor_offset(ctx) > firstEdit.endOffset;
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
          ctx, iterationCheckpoint, parseStartOffset, recoveryEditCountBefore);
      attempt.envelope = detail::to_candidate_envelope(
          attempt.candidate, detail::CandidateOrigin::RepetitionIteration);
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
      bool deepStartedOptionalAfterPriorRecovery,
      const detail::IterationElementProbeResult &elementProbes) const {
    // Compute the closed `IterationBoundaryDecision` from facts that
    // reflect FULL strict acceptance, not just the fast probe at the
    // entry. `firstStrict` means the iteration body accepts strictly
    // at the cursor without any edit; the right signal is
    // `noInsertAttempt.candidate.matched && !hadEdits`, which is
    // computed by `observe_iteration`. Using `attempt_fast_probe`
    // alone over-fires `firstStrict` (a fast probe of a multi-token
    // body returns true on first-token match, which does not imply
    // a full strict parse).
    //
    const auto &noEditCandidate = observation.noInsertAttempt.candidate;
    bool entryRecoverySignalKnown = false;
    bool entryRecoverySignal = false;
    const auto hasEntryRecoverySignal = [&]() {
      if (!entryRecoverySignalKnown) {
        entryRecoverySignal = has_entry_recovery_signal(ctx);
        entryRecoverySignalKnown = true;
      }
      return entryRecoverySignal;
    };
    detail::IterationBoundaryFacts boundaryFacts{
        .startedStrictly = observation.iterationStarted,
        .firstStrict = noEditCandidate.matched && !noEditCandidate.hadEdits,
        .firstRecoverable = false,
        .followStrict = ctx.probeFollowAcceptsHere(),
        .followRecoverable = ctx.probeRecoverableFollowHere(),
        .committedPrefixImposes =
            detail::committed_prefix_imposes_continuation(ctx),
    };
    if (!boundaryFacts.firstStrict && !boundaryFacts.followStrict &&
        !boundaryFacts.startedStrictly) {
      boundaryFacts.firstRecoverable = hasEntryRecoverySignal();
    }
    const auto boundaryDecision =
        detail::decide_iteration_boundary(boundaryFacts);

    const bool noInsertDeferredInsideActiveWindow =
        no_insert_iteration_is_deferred_inside_active_window(
            ctx, observation, parseStartOffset, recoveryEditCountBefore);
    bool noInsertCandidateLegal =
        observation.noInsertAttempt.candidate.matched &&
        observation.noInsertProgressed &&
        !noInsertDeferredInsideActiveWindow;
    const bool firstZeroMinIteration = min == 0 && !skipBetweenIterations;
    const bool firstRequiredIterationInActiveWindow =
        !skipBetweenIterations && min != 0 && !is_optional &&
        !ctx.hasHadEdits() && ctx.hasPendingRecoveryWindows() &&
        parseStartOffset >= ctx.pendingRecoveryWindowActivationOffset() &&
        parseStartOffset <= ctx.pendingRecoveryWindowMaxCursorOffset();
    const bool currentBoundaryLooksLikeStartedIteration =
        observation.startedWithoutEdits || observation.noInsertProgressed;
    const bool retryContinuesStartedIteration =
        observation.noInsertProgressed || observation.iterationStarted;
    const bool iterationTouchesActiveWindow =
        observation.failedInActiveWindow ||
        iteration_starts_in_active_window(ctx, parseStartOffset);
    const bool protectRecoveredIterationBoundary =
        skipBetweenIterations &&
        recoveryEditCountBefore > 0u &&
        parseStartAtIterationEntryBoundary;
    const bool pendingCommittedReplaySignal =
        ctx.hasPendingCommittedRecoveryEditWithin(
            parseStartOffset, observation.noInsertProbe.furthestExploredOffset);
    const bool replayingCommittedPrefixBeforeLocalWindow =
        ctx.hasPendingCommittedRecoveryEdits() &&
        parseStartOffset < ctx.pendingRecoveryWindowBeginOffset();
    bool entryRecoveryConsumesVisibleSignalKnown = false;
    bool entryRecoveryConsumesVisibleSignal = false;
    const auto hasEntryRecoveryConsumesVisibleSignal = [&]() {
      if (!entryRecoveryConsumesVisibleSignalKnown) {
        detail::ScopedBoolOverride leadingInsertProbeGuard{
            ctx.allowLeadingTerminalInsertScope,
            ctx.allowLeadingTerminalInsertScope || !is_optional};
        (void)leadingInsertProbeGuard;
        entryRecoveryConsumesVisibleSignal =
            probe_recoverable_at_entry_consumes_visible(_element, ctx);
        entryRecoveryConsumesVisibleSignalKnown = true;
      }
      return entryRecoveryConsumesVisibleSignal;
    };
    bool entryRecoveryRepairsBoundaryKnown = false;
    bool entryRecoveryRepairsBoundary = false;
    const auto hasEntryRecoveryRepairsBoundarySignal = [&]() {
      if (!entryRecoveryRepairsBoundaryKnown) {
        const auto probeCheckpoint = ctx.mark();
        detail::ScopedBoolOverride leadingInsertProbeGuard{
            ctx.allowLeadingTerminalInsertScope,
            ctx.allowLeadingTerminalInsertScope || !is_optional};
        (void)leadingInsertProbeGuard;
        const auto candidate = detail::evaluate_editable_recovery_candidate(
            ctx, probeCheckpoint, ctx.currentEditCost(),
            ctx.recoveryEditCount(), [this, &ctx, parseStartOffset]() {
              return with_iteration_element_follow(ctx, [&]() {
                return parse(_element, ctx) &&
                       ctx.cursorOffset() > parseStartOffset;
              });
            });
        entryRecoveryRepairsBoundary =
            candidate.matched && candidate.editCount != 0u &&
            candidate.firstEditOffset == parseStartOffset &&
            candidate.postSkipCursorOffset > parseStartOffset;
        entryRecoveryRepairsBoundaryKnown = true;
      }
      return entryRecoveryRepairsBoundary;
    };
    const bool speculativeContinuedRetryAfterRecoveredIteration =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        ctx.hasHadEdits() &&
        observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !iterationTouchesActiveWindow &&
        !hasEntryRecoveryConsumesVisibleSignal();
    const bool speculativeSyntheticSingleLeafRetryAfterRecoveredIteration =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        ctx.hasHadEdits() &&
        !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !iterationTouchesActiveWindow &&
        !hasEntryRecoveryConsumesVisibleSignal();
    const bool speculativeBoundaryRetryWithoutEntrySignal =
        skipBetweenIterations &&
        parseStartAtIterationEntryBoundary &&
        observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !iterationTouchesActiveWindow &&
        !hasEntryRecoverySignal();
    // If the iteration started without prior edits, the enclosing Group's next
    // element already accepts the current cursor, and the NoInsert probe did
    // not progress, let iteration terminate cleanly instead of speculating
    // retry edits that would steal characters from the parent's follow. Edits
    // from outer recovery attempts must remain free to explore alternate
    // candidates, so gate only the ambient/clean entry point.
    const bool atLocalIterationBoundary =
        (skipBetweenIterations || min == 0u) &&
        parseStartAtIterationEntryBoundary;
    const auto &shape = shapeFacts();
    const bool localEntryHasRequiredLiteralAnchor =
        shape.hasRequiredLiteralAnchor;
    const bool localRecoveryBeginsWithSyntheticTerminalInsert =
        !elementProbes.firstStrictAccepts && shape.hasInsertableTerminal;
    const bool localRecoveryBeginsWithLexicalChainTail =
        localRecoveryBeginsWithSyntheticTerminalInsert &&
        shape.hasSyntheticTerminalThenLexicalTail;
    const bool localProbeDeferredPastBoundedCleanup =
        observation.noInsertProbe.furthestExploredOffset > parseStartOffset &&
        observation.noInsertProbe.furthestExploredOffset - parseStartOffset >
            ctx.maxConsecutiveCodepointDeletes;
    bool parentFollowRecoverableConsumesVisibleKnown = false;
    bool parentFollowRecoverableConsumesVisibleValue = false;
    const auto parentFollowRecoverableConsumesVisible = [&]() {
      if (!parentFollowRecoverableConsumesVisibleKnown) {
        parentFollowRecoverableConsumesVisibleValue =
            ctx.probeRecoverableFollowConsumesVisibleHere();
        parentFollowRecoverableConsumesVisibleKnown = true;
      }
      return parentFollowRecoverableConsumesVisibleValue;
    };
    const bool visibleOptionalEntryRepairAtStrictFollow =
        is_optional && firstZeroMinIteration &&
        !observation.noInsertProgressed && boundaryFacts.followStrict &&
        boundaryFacts.firstRecoverable &&
        hasEntryRecoveryConsumesVisibleSignal();
    if (boundaryDecision ==
        detail::IterationBoundaryDecision::RejectToParentFollow) {
      return {};
    }
    if (boundaryDecision == detail::IterationBoundaryDecision::Resync &&
        !iterationTouchesActiveWindow) {
      return {};
    }
    if (boundaryDecision == detail::IterationBoundaryDecision::StopCleanly &&
        !visibleOptionalEntryRepairAtStrictFollow) {
      if (atLocalIterationBoundary) {
        return {};
      }
      if (noInsertCandidateLegal) {
        IterationPlanList plans{};
        plans[0].kind = IterationReplayKind::NoInsert;
        plans[0].legal = true;
        return plans;
      }
    }
    // For repetitions whose body is a single terminal atom (Literal /
    // TerminalRule), also yield to a recoverable follow that proves it
    // will commit visible source. Without this, `some(ID) + "}"` whose
    // closing `}` is missing keeps eating identifiers (e.g. transition
    // events) past the missing delimiter, and the parent's local Insert
    // at the failure cursor lands too late to repair the structure.
    // Restricted to body=terminal-atom because complex Rule bodies (e.g.
    // `many(Statement)`) commonly have a recoverable follow at every
    // iteration boundary (a synthetic insert or skip would always be
    // possible), so the probe would prematurely terminate legitimate
    // greedy repetitions.
    constexpr bool bodyIsTerminalAtom =
        detail::IsTerminalAtom_v<std::remove_cvref_t<Element>>;
    const bool parentFollowAbsorbsIterationBoundary =
        atLocalIterationBoundary && observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        !visibleOptionalEntryRepairAtStrictFollow &&
        !iterationTouchesActiveWindow &&
        (ctx.probeFollowAcceptsHere() ||
         (bodyIsTerminalAtom &&
          ctx.probeRecoverableFollowConsumesVisibleHere()));
    const bool parentFollowOwnsUnstartedIterationBoundary =
        atLocalIterationBoundary && !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        localRecoveryBeginsWithSyntheticTerminalInsert &&
        !visibleOptionalEntryRepairAtStrictFollow &&
        (boundaryFacts.followStrict || boundaryFacts.followRecoverable) &&
        !hasEntryRecoveryConsumesVisibleSignal();
    const bool parentFollowOwnsSyntheticEntryBoundary =
        atLocalIterationBoundary && !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        localRecoveryBeginsWithSyntheticTerminalInsert &&
        !boundaryFacts.followStrict &&
        !hasEntryRecoveryConsumesVisibleSignal() &&
        !boundaryFacts.committedPrefixImposes &&
        parentFollowRecoverableConsumesVisible();
    const bool parentFollowOwnsUnprogressedOptionalBoundary =
        is_optional && atLocalIterationBoundary &&
        !observation.iterationStarted && !observation.noInsertProgressed &&
        !boundaryFacts.followStrict && !boundaryFacts.committedPrefixImposes &&
        parentFollowRecoverableConsumesVisible();
    bool localRetrySignalKnown = false;
    bool localRetrySignal = false;
    const auto hasLocalRetrySignal = [&]() {
      if (!localRetrySignalKnown) {
        localRetrySignal = probe_locally_recoverable(_element, ctx);
        localRetrySignalKnown = true;
      }
      return localRetrySignal;
    };
    bool recoverableAfterSkippingIterationKnown = false;
    bool recoverableAfterSkippingIteration = false;
    const auto isRecoverableAfterSkippingIteration = [&]() {
      if (!recoverableAfterSkippingIterationKnown) {
        recoverableAfterSkippingIteration =
            is_locally_recoverable_after_iteration_skip(ctx);
        recoverableAfterSkippingIterationKnown = true;
      }
      return recoverableAfterSkippingIteration;
    };
    const bool speculativeOptionalStartHidesRecoverableSuffix =
        is_optional && !ctx.hasHadEdits() && !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        isRecoverableAfterSkippingIteration();
    const bool speculativeSingleLeafBoundaryHidesRecoverableIterationSuffix =
        skipBetweenIterations && !ctx.hasHadEdits() &&
        parseStartAtIterationEntryBoundary && observation.noInsertProgressed &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        is_non_strictly_recoverable_after_iteration_skip(ctx);
    // A broad repeated element can consume one visible leaf and then fabricate
    // the rest of its shape, hiding a recoverable parent boundary. This remains
    // unsafe after earlier edits; a recoverable parent follow may only insert a
    // delimiter here, with visible consumption happening in the parent's next
    // site.
    const bool recoverableParentBoundaryOwnsSingleLeafRetry =
        skipBetweenIterations && parseStartAtIterationEntryBoundary &&
        (observation.noInsertProgressed || observation.startedWithoutEdits) &&
        !localEntryHasRequiredLiteralAnchor &&
        observation.noInsertProbe.exploredSingleVisibleLeafOrLess() &&
        !hasEntryRecoveryRepairsBoundarySignal() &&
        !ctx.probeFollowAcceptsHere() &&
        (ctx.probeRecoverableFollowHere() ||
         parentFollowRecoverableConsumesVisible());
    // A nullable repetition boundary may see an element-shaped prefix that
    // fails before committing progress. If the parent can recover at the same
    // cursor and then consume visible input, the prefix is weaker evidence than
    // the parent boundary: local retry would reinterpret the next sibling as an
    // iteration item.
    const bool recoverableParentBoundaryMayOwnWeakStartedRetry =
        atLocalIterationBoundary && !localEntryHasRequiredLiteralAnchor &&
        observation.startedWithoutEdits && !observation.noInsertProgressed &&
        !boundaryFacts.followStrict && !boundaryFacts.committedPrefixImposes &&
        parentFollowRecoverableConsumesVisible();
    const bool recoverableParentBoundaryOwnsWeakStartedRetry =
        recoverableParentBoundaryMayOwnWeakStartedRetry &&
        !hasEntryRecoveryRepairsBoundarySignal();
    // Same ownership rule for a retry whose only local start is a synthetic
    // leading terminal. If the editable entry probe cannot prove that the
    // insert repairs this iteration boundary, the parent follow's visible
    // continuation is the stronger structural owner. Also apply it, narrowly,
    // to early scalar lexical tails that defer far beyond the bounded cleanup
    // window: those can cheaply reinterpret a parent keyword/identifier as
    // chain continuation, while assigned or AST-producing tails must stay
    // available to the shared ranking.
    const bool recoverableParentBoundaryOwnsSyntheticEntryRetry =
        atLocalIterationBoundary && recoveryEditCountBefore > 0u &&
        !observation.startedWithoutEdits && !observation.noInsertProgressed &&
        observation.iterationStarted &&
        localRecoveryBeginsWithSyntheticTerminalInsert &&
        (!hasEntryRecoveryRepairsBoundarySignal() ||
         (localRecoveryBeginsWithLexicalChainTail &&
          recoveryEditCountBefore <= 2u &&
          localProbeDeferredPastBoundedCleanup)) &&
        !boundaryFacts.followStrict && !iterationTouchesActiveWindow &&
        !boundaryFacts.committedPrefixImposes &&
        parentFollowRecoverableConsumesVisible();
    const bool recoverableParentBoundaryOwnsFreshLexicalChainTail =
        atLocalIterationBoundary && recoveryEditCountBefore == 0u &&
        !ctx.hasHadEdits() && !observation.startedWithoutEdits &&
        !observation.noInsertProgressed &&
        localRecoveryBeginsWithLexicalChainTail &&
        !boundaryFacts.followStrict && !boundaryFacts.committedPrefixImposes &&
        parentFollowRecoverableConsumesVisible();
    if (speculativeSingleLeafBoundaryHidesRecoverableIterationSuffix) {
      noInsertCandidateLegal = false;
    }

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
      if (!deleteRetryLegal && !skipBetweenIterations && min != 0 &&
          !is_optional && !ctx.hasHadEdits() &&
          observation.failedInActiveWindow) {
        deleteRetryLegal = true;
      }
      if (!deleteRetryLegal && firstRequiredIterationInActiveWindow) {
        deleteRetryLegal = true;
      }
    }
    // A zero-min repetition is skippable, but not allowed to discard visible
    // source that clearly belongs to its first item after the parent already
    // opened the surrounding structure by recovery. Keep the item repair
    // admissible only when the entry probe proves visible consumption; pure
    // synthetic first items still stop cleanly.
    const bool recoverableFirstZeroMinAfterPriorRecovery =
        min == 0 && firstZeroMinIteration && ctx.hasHadEdits() &&
        !noInsertCandidateLegal && !observation.noInsertProgressed &&
        hasEntryRecoveryConsumesVisibleSignal();
    const bool freshStructuredSeparatorEntryAfterPriorRecovery =
        recoverableFirstZeroMinAfterPriorRecovery &&
        !observation.startedWithoutEdits &&
        localRecoveryBeginsWithSyntheticTerminalInsert &&
        !localRecoveryBeginsWithLexicalChainTail;
    const bool progressedAnchoredZeroMinAfterPriorRecovery =
        min == 0 && skipBetweenIterations &&
        parseStartAtIterationEntryBoundary && ctx.hasHadEdits() &&
        !noInsertCandidateLegal && observation.noInsertProgressed &&
        localEntryHasRequiredLiteralAnchor;
    if (!deleteRetryLegal && recoverableFirstZeroMinAfterPriorRecovery) {
      deleteRetryLegal = true;
    }
    if (!deleteRetryLegal && progressedAnchoredZeroMinAfterPriorRecovery) {
      deleteRetryLegal = true;
    }
    if (!deleteRetryLegal && skipBetweenIterations &&
        parseStartAtIterationEntryBoundary && ctx.hasHadEdits() &&
        !observation.noInsertProgressed && iterationTouchesActiveWindow) {
      deleteRetryLegal = true;
    }
    if (!deleteRetryLegal && pendingCommittedReplaySignal) {
      deleteRetryLegal = true;
    }
    if (!deleteRetryLegal && replayingCommittedPrefixBeforeLocalWindow) {
      deleteRetryLegal = true;
    }
    if (deepStartedOptionalAfterPriorRecovery) {
      // Once an optional iteration already made real strict progress after an
      // earlier recovery, deleting from the repetition boundary mostly
      // replays a wide outer search over content that should now be repaired
      // by the engaged inner structure instead.
      deleteRetryLegal = false;
    }
    if (recoverableParentBoundaryOwnsSingleLeafRetry) {
      deleteRetryLegal = false;
    }
    if (speculativeOptionalStartHidesRecoverableSuffix) {
      deleteRetryLegal = false;
    }
    bool insertRetryLegal =
        noInsertCandidateLegal && ctx.allowInsert;
    if (!insertRetryLegal) {
      insertRetryLegal = deleteRetryLegal;
    }
    if (!insertRetryLegal && replayingCommittedPrefixBeforeLocalWindow &&
        ctx.allowInsert) {
      insertRetryLegal = true;
    }
    if (speculativeSyntheticSingleLeafRetryAfterRecoveredIteration) {
      insertRetryLegal = false;
    }
    if (speculativeOptionalStartHidesRecoverableSuffix) {
      insertRetryLegal = false;
    }
    if (speculativeSingleLeafBoundaryHidesRecoverableIterationSuffix ||
        recoverableParentBoundaryOwnsSingleLeafRetry) {
      insertRetryLegal = false;
    }
    if (!insertRetryLegal && firstZeroMinIteration && !noInsertCandidateLegal &&
        !ctx.hasHadEdits()) {
      insertRetryLegal = hasEntryRecoverySignal();
    }
    if (!insertRetryLegal && visibleOptionalEntryRepairAtStrictFollow) {
      insertRetryLegal = true;
    }
    if (!insertRetryLegal && recoverableFirstZeroMinAfterPriorRecovery &&
        ctx.allowInsert) {
      insertRetryLegal = true;
    }
    if (!insertRetryLegal && progressedAnchoredZeroMinAfterPriorRecovery &&
        ctx.allowInsert) {
      insertRetryLegal = true;
    }
    if (!insertRetryLegal && !noInsertCandidateLegal && ctx.allowInsert &&
        !speculativeBoundaryRetryWithoutEntrySignal &&
        !speculativeContinuedRetryAfterRecoveredIteration &&
        !ctx.hasHadEdits()) {
      insertRetryLegal = isRecoverableAfterSkippingIteration();
    }
    if (!insertRetryLegal && skipBetweenIterations &&
        parseStartAtIterationEntryBoundary && ctx.hasHadEdits() &&
        !observation.noInsertProgressed && !iterationTouchesActiveWindow &&
        hasEntryRecoveryConsumesVisibleSignal()) {
      insertRetryLegal = true;
    }
    const bool retryContinuesVisibleEntryAfterRecovery =
        insertRetryLegal && skipBetweenIterations &&
        parseStartAtIterationEntryBoundary && ctx.hasHadEdits() &&
        !observation.noInsertProgressed &&
        hasEntryRecoveryConsumesVisibleSignal();
    if (parentFollowAbsorbsIterationBoundary) {
      deleteRetryLegal = false;
      insertRetryLegal = false;
    }
    if (parentFollowOwnsUnstartedIterationBoundary) {
      deleteRetryLegal = false;
      insertRetryLegal = false;
    }
    if (parentFollowOwnsSyntheticEntryBoundary) {
      deleteRetryLegal = false;
      insertRetryLegal = false;
    }
    if (parentFollowOwnsUnprogressedOptionalBoundary) {
      deleteRetryLegal = false;
    }
    if (freshStructuredSeparatorEntryAfterPriorRecovery) {
      // A fresh zero-min structured separator entry whose recovered body
      // consumes visible input is already justified by the inserted separator.
      // Keep lexical-chain tails out of this shortcut: their synthetic
      // terminal may be a local spelling of a parent boundary, so delete/parent
      // ownership must remain able to compete.
      deleteRetryLegal = false;
    }
    if (recoverableParentBoundaryOwnsSingleLeafRetry ||
        recoverableParentBoundaryOwnsWeakStartedRetry ||
        recoverableParentBoundaryOwnsSyntheticEntryRetry ||
        recoverableParentBoundaryOwnsFreshLexicalChainTail) {
      // The repeated element only proved a weak or synthetic local start while
      // the parent's follow can recover at the same boundary. Keep ownership
      // with the parent; later local fallback paths must not reopen retry
      // plans.
      deleteRetryLegal = false;
      insertRetryLegal = false;
    }
    // Phase F/D — lookahead gate for insert-into-option recovery.
    //
    // For a strictly optional repetition (`option(X)` = `Repetition<0,1>`),
    // the first iteration starts with no committed edits. If no clean
    // (no-insert) iteration is legal at this point, the parser has two
    // choices: skip the option (its `min=0` makes that legitimate), or
    // synthesize the iteration by inserting its leading element. The
    // insert opens a speculative path that frequently cascades into a
    // multi-edit fabrication (e.g. inserting `(` so a bare ID becomes a
    // `FunctionCall` with synthetic args, then needing more edits to
    // close the parens and patch the body). When the input has not even
    // started the option's body, the cleanest recovery is almost always
    // to skip — the surrounding rule will resync via its own recovery.
    //
    // Allow InsertRetry on a fresh first-iter of an option only when an
    // entry recovery signal is present (i.e. the option's leading
    // element is locally recoverable from the cursor); otherwise the
    // insert is purely speculative and should not enumerate.
    if constexpr (is_optional) {
      if (insertRetryLegal && firstZeroMinIteration && !ctx.hasHadEdits() &&
          !observation.startedWithoutEdits &&
          !observation.noInsertProgressed && !hasEntryRecoverySignal()) {
        insertRetryLegal = false;
      }
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
            .protectLaterVisibleBoundary = protectRecoveredIterationBoundary &&
                                           observation.iterationStarted},
        IterationReplayPlan{
            .kind = IterationReplayKind::InsertRetry,
            .legal = insertRetryLegal,
            .allowInsert = insertRetryLegal,
            .allowDelete = insertRetryLegal && retryContinuesStartedIteration &&
                           ctx.allowDelete &&
                           !parentFollowOwnsUnprogressedOptionalBoundary,
            .allowDestructiveWindowContinuation =
                insertRetryLegal && ctx.hasHadEdits() &&
                parseStartAtIterationEntryBoundary &&
                (retryContinuesStartedIteration ||
                 retryContinuesVisibleEntryAfterRecovery),
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
    // Boundary-decision gates: a closed `IterationBoundaryDecision`
    // value can disqualify entire plan families before the central
    // ranking sees them. The decision is the explicit authority on
    // which families are reachable, applied uniformly here.
    //
    //   `StopCleanly`: parent strict follow accepts and committed
    //   prefix does not impose; retry plans would reach past that
    //   boundary, which the precedence forbids. The only exception is a
    //   nullable first iteration whose recovery consumes visible input at the
    //   same cursor. In that shape the strict follow and the local optional
    //   repair are both plausible interpretations of the same leaf (for
    //   example a fuzzy keyword before a required identifier), so the repair
    //   must survive to the shared ranking. Pure synthetic optional inserts do
    //   not satisfy `visibleOptionalEntryRepairAtStrictFollow`.
    //
    //   `RejectToParentFollow`: parent strict follow forbids a
    //   local repair AND committed prefix forbids a clean stop;
    //   the iteration produces nothing locally and defers to the
    //   parent.
    //
    // `ContinueStrict` is intentionally NOT used as a gate here.
    // Empirically
    // (`RequirementModelRecoversMissingCommaInsideOptionalApplicableTail`), a
    // no-edit success at this iteration step does not imply that outer recovery
    // has nothing left to repair downstream. The closed
    // `IterationBoundaryFacts` set today does not carry a "tail-needs-repair"
    // signal that would distinguish the safe- to-gate case from the unsafe one.
    // The legacy contextual filter `parentFollowAbsorbsIterationBoundary`
    // (above) handles the narrow subset that IS safe to suppress.
    //
    // `ContinueRecoverable` and `RepairStartedIteration` allow the
    // existing per-plan `legal` flags to drive the choice — both
    // describe states where retries CAN be legitimate.
    //
    // `Resync` is gated only when the iteration is OUTSIDE an
    // active recovery window. A previous unconditional gate broke
    // 9 tests because, inside an active recovery window, the
    // dispatch's entry-recovery-signal fallback heuristics produce
    // legitimate retries that the closed precedence value alone
    // cannot distinguish from speculative ones. Conditioning on
    // `!iterationTouchesActiveWindow` preserves those retries
    // while letting the precedence be the explicit authority for
    // the genuinely "no signal at all" case.
    if (boundaryDecision == detail::IterationBoundaryDecision::StopCleanly &&
        atLocalIterationBoundary && !visibleOptionalEntryRepairAtStrictFollow) {
      for (auto &plan : plans) {
        plan.legal = false;
      }
    } else if (boundaryDecision ==
                   detail::IterationBoundaryDecision::StopCleanly &&
               plans[0].legal && !visibleOptionalEntryRepairAtStrictFollow) {
      for (std::size_t i = 1; i < plans.size(); ++i) {
        plans[i].legal = false;
      }
    }
    if (boundaryDecision ==
        detail::IterationBoundaryDecision::RejectToParentFollow) {
      for (auto &plan : plans) {
        plan.legal = false;
      }
    }
    if (boundaryDecision == detail::IterationBoundaryDecision::Resync &&
        !iterationTouchesActiveWindow) {
      for (auto &plan : plans) {
        plan.legal = false;
      }
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
      bool skipBetweenIterations,
      bool &blockedByBoundaryUnsafeRetry) const {
    IterationAttempt attempt{.kind = plan.kind};
    const bool matched =
        replay_iteration(ctx, plan, parseStart,
                         skipBetweenIterations || min == 0);
    const bool insertRetryValid =
        plan.kind != IterationReplayKind::InsertRetry ||
        insert_retry_candidate_is_valid(ctx, plan, parseStartOffset,
                                        recoveryEditCountBefore);
    const bool valid = matched && insertRetryValid;
    if (ctx.furthestExploredCursor() > furthestExploredCursor) {
      furthestExploredCursor = ctx.furthestExploredCursor();
    }
    if (valid) {
      attempt.candidate = capture_iteration_candidate(
          ctx, iterationCheckpoint, parseStartOffset, recoveryEditCountBefore);
      attempt.envelope = detail::to_candidate_envelope(
          attempt.candidate, detail::CandidateOrigin::RepetitionIteration);
      attempt.hasDestructiveEdits =
          has_destructive_retry_edits(ctx, recoveryEditCountBefore);
      attempt.mutatesDestructiveSuffixBeyondStrictExploration =
          mutates_destructive_suffix_beyond_strict_exploration(
              ctx, strictExploredOffset, recoveryEditCountBefore);
      if (!retry_attempt_is_boundary_safe(
              ctx, plan, attempt, parseStartOffset, strictExploredOffset,
              recoveryEditCountBefore, blockedByBoundaryUnsafeRetry)) {
        attempt = {};
        attempt.kind = plan.kind;
      }
    }
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    return attempt;
  }

  [[nodiscard]] bool retry_attempt_is_boundary_safe(
      RecoveryContext &ctx, const IterationReplayPlan &plan,
      const IterationAttempt &attempt, TextOffset parseStartOffset,
      TextOffset strictExploredOffset, std::size_t recoveryEditCountBefore,
      bool &blockedByBoundaryUnsafeRetry) const {
    const bool deletesProtectedSuffixWithoutVisibleContinuation =
        plan.protectLaterVisibleBoundary &&
        attempt.mutatesDestructiveSuffixBeyondStrictExploration &&
        !attempt.candidate.continuesAfterFirstEdit;
    // An InsertRetry plan that ends up emitting destructive edits is
    // suspect: the plan was admitted to perform local inserts, not
    // restructure later input. A destructive edit that continues cleanly
    // past it remains a legitimate recovery for the shared ranking.
    const bool insertRetryDeletesProtectedSuffix =
        plan.protectLaterVisibleBoundary &&
        plan.kind == IterationReplayKind::InsertRetry &&
        attempt.hasDestructiveEdits &&
        !attempt.candidate.continuesAfterFirstEdit;
    const auto startRewriteSpan = protected_start_rewrite_span(
        ctx, recoveryEditCountBefore, parseStartOffset);
    const bool protectedStartRewriteExceedsBoundedCleanup =
        plan.protectParseStartBoundary &&
        attempt.candidate.rewritesParseStartBoundary &&
        startRewriteSpan > ctx.maxConsecutiveCodepointDeletes;
    const auto retryEdits = scan_retry_edits(
        ctx, recoveryEditCountBefore, parseStartOffset, strictExploredOffset);
    const bool protectedStartedIterationRewriteExceedsBoundedCleanup =
        plan.protectParseStartBoundary &&
        attempt.candidate.rewritesParseStartBoundary &&
        retryEdits.startedIterationRewriteSpan >
            ctx.maxConsecutiveCodepointDeletes;
    const bool movesLocalDelimiterByCompensation =
        retryEdits.hasLocalInsertDeleteCompensation;
    const bool protectedSuffixRewriteAfterInsertExceedsBoundedCleanup =
        plan.protectLaterVisibleBoundary &&
        retryEdits.suffixRewriteSpanAfterInsert >
            ctx.maxConsecutiveCodepointDeletes;
    const bool scopedLeadingInsertRewritesVisibleSuffix =
        plan.scopeLeadingTerminalInsert &&
        plan.kind == IterationReplayKind::InsertRetry &&
        retryEdits.hasScopedLeadingInsertThenDestructiveSuffixEdit;
    const bool boundaryUnsafe =
        protectedStartRewriteExceedsBoundedCleanup ||
        protectedStartedIterationRewriteExceedsBoundedCleanup ||
        deletesProtectedSuffixWithoutVisibleContinuation ||
        insertRetryDeletesProtectedSuffix ||
        protectedSuffixRewriteAfterInsertExceedsBoundedCleanup ||
        scopedLeadingInsertRewritesVisibleSuffix ||
        movesLocalDelimiterByCompensation;
    if (boundaryUnsafe) {
      blockedByBoundaryUnsafeRetry =
          blockedByBoundaryUnsafeRetry || attempt.candidate.matched;
      return false;
    }
    return true;
  }

  IterationSelectionResult select_iteration_attempt(
      RecoveryContext &ctx, const IterationPlanList &plans,
      const IterationObservation &observation,
      const RecoveryContext::Checkpoint &iterationCheckpoint,
      const RecoveryContext::Checkpoint &parseCheckpoint,
      const char *parseStart, const char *parseStartFurthestExploredCursor,
      TextOffset parseStartOffset,
      const char *&furthestExploredCursor,
      std::size_t recoveryEditCountBefore,
      bool skipBetweenIterations) const {
    IterationSelectionResult selection;
    std::optional<std::size_t> preEvaluatedPlanIndex;
    bool hasLegalDeleteRetry = false;
    std::optional<std::size_t> firstLegalInsertRetryIndex;
    if (!plans.front().legal) {
      for (std::size_t i = 0; i < plans.size(); ++i) {
        const auto &plan = plans[i];
        if (!plan.legal) {
          continue;
        }
        if (plan.kind == IterationReplayKind::DeleteRetry) {
          hasLegalDeleteRetry = true;
        } else if (plan.kind == IterationReplayKind::InsertRetry &&
                   !firstLegalInsertRetryIndex.has_value()) {
          firstLegalInsertRetryIndex = i;
        }
      }
    }
    if (hasLegalDeleteRetry && firstLegalInsertRetryIndex.has_value()) {
      const auto insertPlanIndex = *firstLegalInsertRetryIndex;
      const auto &insertPlan = plans[insertPlanIndex];
      auto insertAttempt = evaluate_retry_iteration_attempt(
          ctx, insertPlan, iterationCheckpoint, parseCheckpoint, parseStart,
          parseStartFurthestExploredCursor, parseStartOffset,
          observation.noInsertProbe.furthestExploredOffset,
          furthestExploredCursor, recoveryEditCountBefore,
          skipBetweenIterations, selection.blockedByBoundaryUnsafeRetry);
      consider_iteration_attempt(selection, insertAttempt, insertPlan);
      preEvaluatedPlanIndex = insertPlanIndex;
      if (boundary_insert_dominates_delete_retry(insertAttempt,
                                                 parseStartOffset)) {
        return selection;
      }
    }

    for (std::size_t i = 0; i < plans.size(); ++i) {
      const auto &plan = plans[i];
      if (preEvaluatedPlanIndex.has_value() && *preEvaluatedPlanIndex == i) {
        continue;
      }
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
          skipBetweenIterations,
          selection.blockedByBoundaryUnsafeRetry);
      consider_iteration_attempt(selection, retryAttempt, plan);
    }

    return selection;
  }

  /// Phase G — last-resort panic-mode resync skip.
  ///
  /// Scans forward from the failing cursor up to
  /// `ctx.maxResyncSkipBytes` codepoints looking for the first position
  /// where `_element` strictly starts and consumes at least one codepoint.
  /// The skipped range is emitted as a contiguous `Delete` edit run and
  /// the iteration is treated as matched at the resync point.
  ///
  /// Only invoked when every normal iteration plan failed; the produced
  /// candidate's `editCost` naturally makes a cheaper local candidate
  /// dominate under the shared `RecoveryKey` comparator, so this path
  /// never steals from admissible lower-cost recoveries.
  [[nodiscard]] bool try_resync_skip_iteration(
      RecoveryContext &ctx, const char *parseStart,
      TextOffset parseStartOffset) const {
    if (!ctx.allowDelete || ctx.maxResyncSkipBytes == 0u) {
      return false;
    }
    if (ctx.cursor() != parseStart || ctx.cursor() >= ctx.end) {
      return false;
    }
    // Do not reinterpret bytes inside a committed-prefix replay: the driver
    // already fixed the edit script for that region at an earlier site.
    if (ctx.hasPendingCommittedRecoveryEdits() &&
        parseStartOffset < ctx.committedRecoveryResumeFloor) {
      return false;
    }
    if (ctx.probeFollowAcceptsHere()) {
      return false;
    }
    const auto recoveryCheckpoint = ctx.mark();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    detail::ScopedBoolOverride destructiveContinuationGuard{
        ctx.allowDestructiveWindowContinuation,
        ctx.allowDestructiveWindowContinuation || ctx.hasHadEdits()};
    (void)destructiveContinuationGuard;
    detail::ExtendedDeleteScanBudgetScope<RecoveryContext> budgetScope{ctx};
    if (!budgetScope.tryEnable()) {
      return false;
    }
    const std::uint32_t codepointBudget = ctx.maxResyncSkipBytes;
    std::uint32_t deleted = 0u;
    while (deleted < codepointBudget && ctx.cursor() < ctx.end) {
      if (ctx.probeFollowAcceptsHere()) {
        break;
      }
      if (!ctx.deleteOneCodepoint()) {
        break;
      }
      ++deleted;
      if (!probe_recoverable_at_entry(_element, ctx) &&
          !attempt_fast_probe(ctx, _element)) {
        continue;
      }
      const auto parseCheckpoint = ctx.mark();
      const char *const parseCursor = ctx.cursor();
      const auto targetRecoveryEditCount = ctx.recoveryEditCount();
      const auto targetVisibleLeafCount = ctx.failureHistorySize();
      if ((attempt_parse_no_edits(ctx, _element) ||
           with_iteration_element_follow(
               ctx, [&]() { return parse(_element, ctx); })) &&
          ctx.cursor() != parseCursor) {
        const auto edits = ctx.recoveryEditsView();
        const bool targetIntroducedEdit =
            ctx.recoveryEditCount() > targetRecoveryEditCount;
        const auto visibleLeafCountAfterTarget = ctx.failureHistorySize();
        const auto targetVisibleLeafDelta =
            visibleLeafCountAfterTarget > targetVisibleLeafCount
                ? visibleLeafCountAfterTarget - targetVisibleLeafCount
                : 0u;
        const bool targetIntroducedDestructiveEdit = std::ranges::any_of(
            edits.begin() +
                static_cast<std::ptrdiff_t>(targetRecoveryEditCount),
            edits.end(), [](const auto &edit) noexcept {
              return edit.kind == ParseDiagnosticKind::Deleted ||
                     edit.kind == ParseDiagnosticKind::Replaced;
            });
        if (targetIntroducedDestructiveEdit) {
          ctx.rewind(parseCheckpoint);
          continue;
        }
        if (targetIntroducedEdit &&
            targetVisibleLeafDelta < ctx.stabilityTokenCount) {
          ctx.rewind(parseCheckpoint);
          continue;
        }
        budgetScope.commitOverflowEdits();
        return true;
      }
      ctx.rewind(parseCheckpoint);
    }
    ctx.rewind(recoveryCheckpoint);
    if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    }
    return false;
  }

  [[nodiscard]] bool try_resync_skip_to_parent_follow(
      RecoveryContext &ctx, const char *parseStart,
      TextOffset parseStartOffset, bool currentEntryRecoverable) const {
    if (!ctx.allowDelete || ctx.maxResyncSkipBytes == 0u ||
        ctx.cursor() != parseStart || ctx.cursor() >= ctx.end ||
        ctx.probeFollowAcceptsHere() || currentEntryRecoverable) {
      return false;
    }
    if (ctx.hasPendingCommittedRecoveryEdits() &&
        parseStartOffset < ctx.committedRecoveryResumeFloor) {
      return false;
    }
    const auto recoveryCheckpoint = ctx.mark();
    const char *const savedFurthestExploredCursor =
        ctx.furthestExploredCursor();
    detail::ScopedBoolOverride destructiveContinuationGuard{
        ctx.allowDestructiveWindowContinuation,
        ctx.allowDestructiveWindowContinuation || ctx.hasHadEdits()};
    (void)destructiveContinuationGuard;
    detail::ExtendedDeleteScanBudgetScope<RecoveryContext> budgetScope{ctx};
    if (!budgetScope.tryEnable()) {
      return false;
    }
    const std::uint32_t codepointBudget = ctx.maxResyncSkipBytes;
    std::uint32_t deleted = 0u;
    while (deleted < codepointBudget && ctx.cursor() < ctx.end) {
      if (!ctx.deleteOneCodepoint()) {
        break;
      }
      ++deleted;
      const bool reachedImplicitEofFollow =
          ctx.cursor() == ctx.end && ctx._followProbeFn == nullptr &&
          ctx._recoverableFollowProbeFn == nullptr;
      if (ctx.probeFollowAcceptsHere() || reachedImplicitEofFollow) {
        budgetScope.commitOverflowEdits();
        return true;
      }
    }
    ctx.rewind(recoveryCheckpoint);
    if (savedFurthestExploredCursor > ctx.furthestExploredCursor()) {
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    }
    return false;
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
      PEGIUM_STEP_TRACE_INC(detail::StepCounter::RepetitionFastAttempts);
      const bool startedWithoutEdits =
          probe_started_without_edits(ctx, _element);
      PEGIUM_STEP_TRACE_INC(startedWithoutEdits
                                ? detail::StepCounter::RepetitionFastSuccess
                                : detail::StepCounter::RepetitionFastFailures);
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
    if (skipBetweenIterations) {
      ctx.skip();
    }
    const auto parseCheckpoint = ctx.mark();
    const char *const parseStart = ctx.cursor();
    const char *const parseStartFurthestExploredCursor =
        ctx.furthestExploredCursor();
    const TextOffset parseStartOffset = ctx.cursorOffset();
    const bool replayingStrictPrefixBeforeLocalWindow =
        ctx.hasPendingRecoveryWindows() &&
        parseStartOffset < ctx.pendingRecoveryWindowActivationOffset();
    if (replayingStrictPrefixBeforeLocalWindow) {
      const bool replayedStrictPrefix = parse(_element, ctx);
      if (!replayedStrictPrefix) {
        ctx.rewind(iterationCheckpoint);
        if (parseStartFurthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
        }
      }
      return replayedStrictPrefix;
    }
    const bool parseStartAtIterationEntryBoundary =
        detail::post_skip_cursor_offset(ctx) == parseStartOffset;
    // Capture the cheap element strict probe at the parseStart cursor
    // (where the iteration would begin a fresh attempt). The recoverable
    // entry probe is deliberately lazy inside `enumerate_iteration_plans`:
    // it can parse deeply, while the boundary table needs it only when no
    // strict element/follow/started-iteration fact already decides the case.
    detail::IterationElementProbeResult elementProbes;
    {
      detail::ProbeRestoreScope guard{ctx};
      elementProbes.firstStrictAccepts = attempt_fast_probe(ctx, _element);
    }
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
        deepStartedOptionalAfterPriorRecovery, elementProbes);
    const bool hasLegalRetryPlan =
        has_legal_iteration_retry_plan(iterationPlans);
    const char *furthestExploredCursor = ctx.furthestExploredCursor();
    if (iterationPlans.front().legal && !hasLegalRetryPlan) {
      return true;
    }
    if (!iterationPlans.front().legal) {
      const auto singleRetryIndex =
          single_legal_iteration_retry_plan_index(iterationPlans);
      if (singleRetryIndex.has_value()) {
        ctx.rewind(parseCheckpoint);
        ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
        const auto &singlePlan = iterationPlans[*singleRetryIndex];
        const bool replayed = replay_iteration(
            ctx, singlePlan, parseStart, skipBetweenIterations || min == 0);
        const bool insertRetryValid =
            singlePlan.kind != IterationReplayKind::InsertRetry ||
            insert_retry_candidate_is_valid(ctx, singlePlan, parseStartOffset,
                                            recoveryEditCountBefore);
        if (replayed && insertRetryValid) {
          IterationAttempt directAttempt{.kind = singlePlan.kind};
          directAttempt.candidate = capture_iteration_candidate(
              ctx, iterationCheckpoint, parseStartOffset,
              recoveryEditCountBefore);
          directAttempt.envelope = detail::to_candidate_envelope(
              directAttempt.candidate,
              detail::CandidateOrigin::RepetitionIteration);
          directAttempt.hasDestructiveEdits =
              has_destructive_retry_edits(ctx, recoveryEditCountBefore);
          directAttempt.mutatesDestructiveSuffixBeyondStrictExploration =
              mutates_destructive_suffix_beyond_strict_exploration(
                  ctx, observation.noInsertProbe.furthestExploredOffset,
                  recoveryEditCountBefore);
          bool blockedByBoundaryUnsafeRetry = false;
          if (retry_attempt_is_boundary_safe(
                  ctx, singlePlan, directAttempt, parseStartOffset,
                  observation.noInsertProbe.furthestExploredOffset,
                  recoveryEditCountBefore, blockedByBoundaryUnsafeRetry)) {
            const bool iterationTouchedActiveWindow =
                observation.failedInActiveWindow ||
                iteration_starts_in_active_window(ctx, parseStartOffset);
            const auto directStrength = classify_iteration_attempt_strength(
                directAttempt, parseStartOffset);
            const bool weakRetryEscapesActiveWindow =
                skipBetweenIterations && recoveryEditCountBefore > 0u &&
                directStrength == IterationRecoveryStrength::Weak &&
                !iterationTouchedActiveWindow;
            if (!weakRetryEscapesActiveWindow) {
              return true;
            }
          }
        }
      }
    }
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    if (!observation.noInsertProgressed &&
        !hasLegalRetryPlan) {
      const bool atLocalIterationBoundary =
          (skipBetweenIterations || min == 0u) &&
          parseStartAtIterationEntryBoundary;
      const bool parentMayOwnWeakStartedRetry =
          atLocalIterationBoundary &&
          !shapeFacts().hasRequiredLiteralAnchor &&
          observation.startedWithoutEdits && !ctx.probeFollowAcceptsHere() &&
          !detail::committed_prefix_imposes_continuation(ctx) &&
          ctx.probeRecoverableFollowConsumesVisibleHere();
      bool localEntryRecoveryRepairsBoundary = true;
      if (parentMayOwnWeakStartedRetry) {
        const auto probeCheckpoint = ctx.mark();
        detail::ScopedBoolOverride leadingInsertProbeGuard{
            ctx.allowLeadingTerminalInsertScope,
            ctx.allowLeadingTerminalInsertScope || !is_optional};
        (void)leadingInsertProbeGuard;
        const auto candidate = detail::evaluate_editable_recovery_candidate(
            ctx, probeCheckpoint, ctx.currentEditCost(),
            ctx.recoveryEditCount(), [this, &ctx, parseStartOffset]() {
              return with_iteration_element_follow(ctx, [&]() {
                return parse(_element, ctx) &&
                       ctx.cursorOffset() > parseStartOffset;
              });
            });
        localEntryRecoveryRepairsBoundary =
            candidate.matched && candidate.editCount != 0u &&
            candidate.firstEditOffset == parseStartOffset &&
            candidate.postSkipCursorOffset > parseStartOffset;
      }
      if (parentMayOwnWeakStartedRetry && !localEntryRecoveryRepairsBoundary) {
        ctx.rewind(iterationCheckpoint);
        if (furthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(furthestExploredCursor);
        }
        return false;
      }
      if ((skipBetweenIterations || min == 0u) &&
          parseStartAtIterationEntryBoundary &&
          !observation.startedWithoutEdits &&
          shapeFacts().hasInsertableTerminal &&
          (ctx.probeFollowAcceptsHere() || ctx.probeRecoverableFollowHere())) {
        ctx.rewind(iterationCheckpoint);
        if (furthestExploredCursor > ctx.furthestExploredCursor()) {
          ctx.restoreFurthestExploredCursor(furthestExploredCursor);
        }
        return false;
      }
      if (try_resync_skip_iteration(ctx, parseStart, parseStartOffset)) {
        return true;
      }
      if ((skipBetweenIterations || min == 0u) &&
          try_resync_skip_to_parent_follow(
              ctx, parseStart, parseStartOffset,
              elementProbes.firstRecoverableAccepts)) {
        return true;
      }
      ctx.rewind(iterationCheckpoint);
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      return false;
    }
    const auto selection = select_iteration_attempt(
        ctx, iterationPlans, observation, iterationCheckpoint, parseCheckpoint,
        parseStart, parseStartFurthestExploredCursor, parseStartOffset,
        furthestExploredCursor, recoveryEditCountBefore,
        skipBetweenIterations);
    const bool iterationTouchedActiveWindow =
        observation.failedInActiveWindow ||
        iteration_starts_in_active_window(ctx, parseStartOffset);
    if (!selection.bestAttempt.candidate.matched) {
      if (try_resync_skip_iteration(ctx, parseStart, parseStartOffset)) {
        return true;
      }
      if ((skipBetweenIterations || min == 0u) &&
          try_resync_skip_to_parent_follow(
              ctx, parseStart, parseStartOffset,
              elementProbes.firstRecoverableAccepts)) {
        return true;
      }
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
      (void)blockedFrontier;
      return false;
    }
    const auto bestStrength = classify_iteration_attempt_strength(
        selection.bestAttempt, parseStartOffset);
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
      return false;
    }
    // Rebuild only the winning branch from the shared parse checkpoint instead
    // of keeping success checkpoints alive across competing recovery parses.
    ctx.rewind(parseCheckpoint);
    ctx.restoreFurthestExploredCursor(parseStartFurthestExploredCursor);
    const bool replayed =
        replay_iteration(ctx, selection.winningPlan, parseStart,
                         skipBetweenIterations || min == 0);
    if (!replayed) {
      ctx.rewind(iterationCheckpoint);
      if (furthestExploredCursor > ctx.furthestExploredCursor()) {
        ctx.restoreFurthestExploredCursor(furthestExploredCursor);
      }
      return false;
    }
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

  /// Pure-grammar facts derived from `_element` once per Repetition
  /// instance. The four walkers used to be called on every iteration
  /// boundary; caching turns each call into a single field read.
  struct ShapeFacts {
    bool hasInsertableTerminal = false;
    bool hasRequiredLiteralAnchor = false;
    bool hasSyntheticTerminalThenLexicalTail = false;
    bool isLexicalTail = false;
  };

  mutable std::optional<ShapeFacts> _shapeFacts;

  [[nodiscard]] const ShapeFacts &shapeFacts() const noexcept {
    if (!_shapeFacts.has_value()) {
      ShapeFacts f;
      f.hasInsertableTerminal = starts_with_insertable_terminal(_element);
      f.hasRequiredLiteralAnchor =
          starts_with_required_literal_anchor(_element);
      f.hasSyntheticTerminalThenLexicalTail =
          starts_with_synthetic_terminal_then_unassigned_lexical_tail(
              _element);
      f.isLexicalTail = is_unassigned_lexical_tail(_element);
      _shapeFacts = f;
    }
    return *_shapeFacts;
  }

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
