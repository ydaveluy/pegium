#include <pegium/core/parser/RecoverySearch.hpp>

#include <algorithm>
#include <limits>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/DataTypeRule.hpp>
#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/grammar/UnorderedGroup.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryDebug.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>

namespace pegium::parser::detail {

[[nodiscard]] NormalizedRecoveryOrderKey
recovery_attempt_order_key(const RecoveryScore &score) noexcept {
  NormalizedRecoveryOrderKey key;
  key.safety.matched = score.selection.entryRuleMatched;
  key.prefix.entryRuleMatched = score.selection.entryRuleMatched;
  key.prefix.stable = score.selection.stable;
  key.prefix.credible = score.selection.credible;
  key.prefix.fullMatch = score.selection.fullMatch;
  key.prefix.firstEditOffset = score.edits.firstEditOffset;
  key.continuation.continuesAfterFirstEdit =
      score.edits.entryCount > 0u &&
      score.progress.parsedLength >
          score.edits.firstEditOffset + score.edits.editSpan;
  key.edits.editCost = score.edits.editCost;
  key.edits.editSpan = score.edits.editSpan;
  key.edits.entryCount = score.edits.entryCount;
  key.progress.parsedLength = score.progress.parsedLength;
  key.progress.maxCursorOffset = score.progress.maxCursorOffset;
  return key;
}

namespace {

void trace_recovery_json(const char *label, const pegium::JsonValue &value) {
  PEGIUM_RECOVERY_TRACE(label, " ", value.toJsonString({.pretty = false}));
}

void trace_strict_summary(const StrictParseSummary &summary) {
  trace_recovery_json("[parser strict]", strict_parse_summary_to_json(summary));
}

void trace_failure_snapshot(const FailureSnapshot &snapshot) {
  trace_recovery_json("[parser snapshot]", failure_snapshot_to_json(snapshot));
}

void trace_recovery_window(std::uint32_t windowIndex, std::uint32_t tokenCount,
                           const RecoveryWindow &window,
                           std::size_t selectedWindowCount) {
  auto payload = recovery_window_to_json(window);
  auto &object = payload.object();
  object["selectedWindowCount"] =
      static_cast<std::int64_t>(selectedWindowCount);
  object["windowIndex"] = static_cast<std::int64_t>(windowIndex);
  object["requestedTokenCount"] = static_cast<std::int64_t>(tokenCount);
  trace_recovery_json("[parser window]", payload);
}

void trace_recovery_attempt(const RecoveryAttempt &attempt,
                            const RecoveryAttemptSpec &spec) {
  trace_recovery_json("[parser attempt]",
                      recovery_attempt_to_json(attempt, &spec));
}

struct RecoveryWindowPrefixContract {
  TextOffset editFloorOffset = 0;
  TextOffset stablePrefixOffset = 0;
  bool hasStablePrefix = false;
};

struct NonCredibleReplayContract {
  TextOffset resumeFloor = 0;
  std::size_t preservedEditCount = 0u;
};

struct StrictFailureStageResult {
  RecoveryAttempt strictAttempt;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset failureVisibleCursorOffset = 0;
  std::uint32_t strictParseRuns = 1u;
};

struct WindowRecoverySearchResult {
  struct CandidateRoles {
    bool selectable = false;
    bool fallback = false;
    bool rankedSelectable = false;
    bool rankedFallback = false;
  };

  std::vector<RecoveryAttempt> attempts;
  std::optional<std::size_t> bestSelectableAttemptIndex;
  std::optional<std::size_t> bestFallbackAttemptIndex;
  std::optional<std::size_t> bestRankedSelectableAttemptIndex;
  std::optional<std::size_t> bestRankedFallbackAttemptIndex;
  bool sawRejectedAttempt = false;
  bool onlyInertTrailingRejectedAttempts = true;

  [[nodiscard]] bool hasSelectableAttempt() const noexcept {
    return bestSelectableAttempt() != nullptr;
  }

  [[nodiscard]] bool hasFallbackAttempt() const noexcept {
    return bestFallbackAttempt() != nullptr;
  }

  [[nodiscard]] bool hasQualifiedAttempt() const noexcept {
    return hasSelectableAttempt() || hasFallbackAttempt();
  }

  [[nodiscard]] const RecoveryAttempt *bestSelectableAttempt() const noexcept {
    return attempt_at(bestSelectableAttemptIndex);
  }

  [[nodiscard]] const RecoveryAttempt *bestFallbackAttempt() const noexcept {
    return attempt_at(bestFallbackAttemptIndex);
  }

  [[nodiscard]] std::optional<RecoveryAttempt>
  takeBestSelectableAttempt() noexcept {
    return take_attempt(bestSelectableAttemptIndex);
  }

  [[nodiscard]] std::optional<RecoveryAttempt> takeBestFallbackAttempt() noexcept {
    return take_attempt(bestFallbackAttemptIndex);
  }

  [[nodiscard]] const RecoveryAttempt *
  narrowingRetryCandidate(bool &selectableAttempt) const noexcept {
    if (const auto *attempt = attempt_at(bestRankedSelectableAttemptIndex)) {
      selectableAttempt = true;
      return attempt;
    }
    if (const auto *attempt = attempt_at(bestRankedFallbackAttemptIndex)) {
      selectableAttempt = false;
      return attempt;
    }
    return nullptr;
  }

  [[nodiscard]] bool hasRankedAttempt() const noexcept {
    bool selectableAttempt = false;
    return narrowingRetryCandidate(selectableAttempt) != nullptr;
  }

  [[nodiscard]] bool hasAnyCandidate() const noexcept {
    return hasQualifiedAttempt() || hasRankedAttempt();
  }

  void considerAttempt(RecoveryAttempt attempt, CandidateRoles roles) {
    const bool betterSelectable =
        roles.selectable &&
        better_than(bestSelectableAttemptIndex, attempt);
    const bool betterFallback =
        roles.fallback &&
        better_than(bestFallbackAttemptIndex, attempt);
    const bool betterRankedSelectable =
        roles.rankedSelectable &&
        better_than(bestRankedSelectableAttemptIndex, attempt);
    const bool betterRankedFallback =
        roles.rankedFallback &&
        better_than(bestRankedFallbackAttemptIndex, attempt);
    if (!(betterSelectable || betterFallback || betterRankedSelectable ||
          betterRankedFallback)) {
      return;
    }

    attempts.push_back(std::move(attempt));
    const auto index = attempts.size() - 1u;
    if (betterSelectable) {
      bestSelectableAttemptIndex = index;
    }
    if (betterFallback) {
      bestFallbackAttemptIndex = index;
    }
    if (betterRankedSelectable) {
      bestRankedSelectableAttemptIndex = index;
    }
    if (betterRankedFallback) {
      bestRankedFallbackAttemptIndex = index;
    }
  }

  void recordRejectedAttempt(bool inertTrailingRejectedAttempt) noexcept {
    sawRejectedAttempt = true;
    onlyInertTrailingRejectedAttempts =
        onlyInertTrailingRejectedAttempts && inertTrailingRejectedAttempt;
  }

  void merge(WindowRecoverySearchResult other) {
    sawRejectedAttempt = sawRejectedAttempt || other.sawRejectedAttempt;
    onlyInertTrailingRejectedAttempts =
        onlyInertTrailingRejectedAttempts &&
        other.onlyInertTrailingRejectedAttempts;
    std::vector<CandidateRoles> roles(other.attempts.size());
    if (other.bestSelectableAttemptIndex.has_value()) {
      roles[*other.bestSelectableAttemptIndex].selectable = true;
    }
    if (other.bestFallbackAttemptIndex.has_value()) {
      roles[*other.bestFallbackAttemptIndex].fallback = true;
    }
    if (other.bestRankedSelectableAttemptIndex.has_value()) {
      roles[*other.bestRankedSelectableAttemptIndex].rankedSelectable = true;
    }
    if (other.bestRankedFallbackAttemptIndex.has_value()) {
      roles[*other.bestRankedFallbackAttemptIndex].rankedFallback = true;
    }
    for (std::size_t i = 0; i < other.attempts.size(); ++i) {
      if (!(roles[i].selectable || roles[i].fallback ||
            roles[i].rankedSelectable || roles[i].rankedFallback)) {
        continue;
      }
      considerAttempt(std::move(other.attempts[i]), roles[i]);
    }
  }

private:
  [[nodiscard]] const RecoveryAttempt *
  attempt_at(std::optional<std::size_t> index) const noexcept {
    return index.has_value() ? std::addressof(attempts[*index]) : nullptr;
  }

  [[nodiscard]] bool better_than(std::optional<std::size_t> index,
                                 const RecoveryAttempt &attempt) const noexcept {
    return !index.has_value() ||
           is_better_recovery_attempt(attempt, attempts[*index]);
  }

  [[nodiscard]] std::optional<RecoveryAttempt>
  take_attempt(std::optional<std::size_t> index) noexcept {
    if (!index.has_value()) {
      return std::nullopt;
    }
    return std::move(attempts[*index]);
  }
};

enum class RecoveryAttemptRunCharge : std::uint8_t {
  Budgeted,
  ValidationOnly,
};

[[nodiscard]] bool trimmed_tail_delete_extends_attempt(
    const RecoveryAttempt &trimmed,
    const RecoveryAttempt &untrimmed) noexcept;

[[nodiscard]] WindowRecoverySearchResult evaluate_recovery_window_attempts(
    const grammar::ParserRule &entryRule, const Skipper &skipper,
    const ParseOptions &options, const text::TextSnapshot &text,
    const RecoveryAttemptSpec &spec, const RecoveryAttempt &selectedAttempt,
    const FailureSnapshot &failureSnapshot, TextOffset inputSize,
    std::uint32_t &recoveryAttemptRuns,
    std::uint32_t &budgetedRecoveryAttemptRuns,
    RecoveryAttemptRunCharge runCharge,
    const utils::CancellationToken &cancelToken);

void consider_window_attempt_candidate(
    WindowRecoverySearchResult &result, RecoveryAttempt attempt,
    const RecoveryAttempt &selectedAttempt,
    const FailureSnapshot &failureSnapshot, TextOffset inputSize);

[[nodiscard]] TextOffset
last_edit_offset(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
preserves_stable_prefix_before_first_edit(
    const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
continues_after_first_edit(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] std::size_t visible_leaf_count_in_offset_range(
    const RootCstNode &cst, TextOffset beginOffset,
    TextOffset endOffset) noexcept;

[[nodiscard]] TextOffset
visible_leaf_stable_prefix_offset(const FailureSnapshot &snapshot) noexcept {
  TextOffset stablePrefixOffset = 0;
  for (const auto &leaf : snapshot.failureLeafHistory) {
    if (leaf.endOffset > snapshot.maxCursorOffset) {
      break;
    }
    stablePrefixOffset = leaf.endOffset;
  }
  return stablePrefixOffset;
}

[[nodiscard]] RecoveryWindowPrefixContract recovery_window_prefix_contract(
    const FailureSnapshot &snapshot,
    const RecoveryAttempt &selectedAttempt) noexcept {
  const bool strictPrefixOnly =
      selectedAttempt.recoveryEdits.empty() && selectedAttempt.parsedLength != 0;
  if (strictPrefixOnly ||
      (selectedAttempt.parsedLength != 0 && selectedAttempt.fullMatch)) {
    return {.editFloorOffset = selectedAttempt.parsedLength,
            .stablePrefixOffset = selectedAttempt.parsedLength,
            .hasStablePrefix = true};
  }
  if (selectedAttempt.parsedLength != 0 && selectedAttempt.stableAfterRecovery) {
    if (const auto stablePrefixOffset =
            visible_leaf_stable_prefix_offset(snapshot);
        stablePrefixOffset != 0 &&
        stablePrefixOffset < selectedAttempt.parsedLength) {
      return {.editFloorOffset = stablePrefixOffset,
              .stablePrefixOffset = stablePrefixOffset,
              .hasStablePrefix = true};
    }
    return {.editFloorOffset = selectedAttempt.parsedLength,
            .stablePrefixOffset = selectedAttempt.parsedLength,
            .hasStablePrefix = true};
  }
  if (const auto stablePrefixOffset =
          visible_leaf_stable_prefix_offset(snapshot);
      stablePrefixOffset != 0) {
    return {.editFloorOffset = stablePrefixOffset,
            .stablePrefixOffset = stablePrefixOffset,
            .hasStablePrefix = true};
  }
  if (!snapshot.hasFailureToken ||
      snapshot.failureTokenIndex >= snapshot.failureLeafHistory.size()) {
    return {};
  }
  return {
      .editFloorOffset =
          snapshot.failureLeafHistory[snapshot.failureTokenIndex].beginOffset,
      .stablePrefixOffset = 0,
      .hasStablePrefix = false};
}

[[nodiscard]] TextOffset
failure_visible_cursor_offset(const FailureSnapshot &snapshot,
                              TextOffset fallbackOffset) noexcept {
  if (snapshot.hasFailureToken &&
      snapshot.failureTokenIndex < snapshot.failureLeafHistory.size()) {
    return snapshot.failureLeafHistory[snapshot.failureTokenIndex].endOffset;
  }
  if (!snapshot.failureLeafHistory.empty()) {
    return snapshot.failureLeafHistory.back().endOffset;
  }
  return fallbackOffset;
}

[[nodiscard]] StrictFailureStageResult
run_strict_failure_stage(const grammar::ParserRule &entryRule,
                         const Skipper &skipper, const ParseOptions &options,
                         const text::TextSnapshot &text,
                         const utils::CancellationToken &cancelToken) {
  StrictFailureStageResult result;
  const auto inputSize = static_cast<TextOffset>(text.size());
  const StrictFailureEngine strictFailureEngine;

  auto strictResult =
      strictFailureEngine.runStrictParse(entryRule, skipper, text, cancelToken);
  result.strictAttempt.cst = std::move(strictResult.cst);
  result.strictAttempt.entryRuleMatched = strictResult.summary.entryRuleMatched;
  result.strictAttempt.parsedLength = strictResult.summary.parsedLength;
  result.strictAttempt.lastVisibleCursorOffset =
      strictResult.summary.lastVisibleCursorOffset;
  result.strictAttempt.fullMatch = strictResult.summary.fullMatch;
  result.strictAttempt.maxCursorOffset = strictResult.summary.maxCursorOffset;
  result.strictAttempt.editTrace.firstEditOffset = inputSize;
  result.failureVisibleCursorOffset =
      result.strictAttempt.lastVisibleCursorOffset;
  score_recovery_attempt(result.strictAttempt);

  if (result.strictAttempt.fullMatch || !options.recoveryEnabled) {
    return result;
  }

  const StrictParseSummary strictSummary{
      .inputSize = inputSize,
      .parsedLength = result.strictAttempt.parsedLength,
      .lastVisibleCursorOffset = result.strictAttempt.lastVisibleCursorOffset,
      .maxCursorOffset = result.strictAttempt.maxCursorOffset,
      .entryRuleMatched = result.strictAttempt.entryRuleMatched,
      .fullMatch = result.strictAttempt.fullMatch,
  };
  trace_strict_summary(strictSummary);
  auto failureAnalysis = strictFailureEngine.inspectFailure(
      entryRule, skipper, text, strictSummary, cancelToken);
  result.failureVisibleCursorOffset = failure_visible_cursor_offset(
      failureAnalysis.snapshot, result.failureVisibleCursorOffset);
  trace_failure_snapshot(failureAnalysis.snapshot);
  result.failureSnapshot = std::move(failureAnalysis.snapshot);
  return result;
}

[[nodiscard]] bool
recovery_attempt_budget_exhausted(const RecoverySearchRunResult &result,
                                  const ParseOptions &options) noexcept {
  return result.budgetedRecoveryAttemptRuns >= options.maxRecoveryAttempts;
}

[[nodiscard]] constexpr bool
recovery_attempt_establishes_replay_contract(
    const RecoveryAttempt &attempt) noexcept {
  return attempt.fullMatch || attempt.stableAfterRecovery ||
         attempt.reachedRecoveryTarget;
}

[[nodiscard]] bool
has_trailing_failure_leaf(const FailureSnapshot &snapshot) noexcept {
  return snapshot.hasFailureToken &&
         snapshot.failureTokenIndex + 1u == snapshot.failureLeafHistory.size();
}

[[nodiscard]] TextOffset
last_failure_leaf_end_offset(const FailureSnapshot &snapshot) noexcept {
  return snapshot.failureLeafHistory.empty()
             ? 0
             : snapshot.failureLeafHistory.back().endOffset;
}

[[nodiscard]] bool cursor_outruns_last_visible_failure_leaf(
    const FailureSnapshot &snapshot) noexcept {
  return has_trailing_failure_leaf(snapshot) &&
         !snapshot.failureLeafHistory.empty() &&
         snapshot.failureLeafHistory.back().endOffset <
             snapshot.maxCursorOffset;
}

[[nodiscard]] bool is_near_eof_tail_failure(const FailureSnapshot &snapshot,
                                            TextOffset inputSize) noexcept {
  constexpr TextOffset kLargeInputThreshold = 4 * 1024;
  constexpr TextOffset kLateRecoveryTailThreshold = 16;
  return inputSize >= kLargeInputThreshold &&
         cursor_outruns_last_visible_failure_leaf(snapshot) &&
         snapshot.maxCursorOffset < inputSize &&
         inputSize - snapshot.maxCursorOffset <= kLateRecoveryTailThreshold;
}

[[nodiscard]] bool is_inert_trailing_eof_attempt(
    const RecoveryAttempt &attempt, const RecoveryAttempt &baseline,
    const FailureSnapshot &snapshot, TextOffset inputSize) noexcept {
  if (!is_near_eof_tail_failure(snapshot, inputSize) ||
      !attempt.entryRuleMatched || attempt.fullMatch) {
    return false;
  }
  if (attempt.parsedLength != baseline.parsedLength ||
      attempt.lastVisibleCursorOffset != baseline.lastVisibleCursorOffset ||
      attempt.maxCursorOffset != baseline.maxCursorOffset) {
    return false;
  }
  if (attempt.editCount != 0 || !attempt.recoveryEdits.empty() ||
      attempt.completedRecoveryWindows != 0) {
    return false;
  }
  return !attempt.reachedRecoveryTarget && !attempt.stableAfterRecovery;
}

[[nodiscard]] WindowRecoverySearchResult evaluate_recovery_window_attempts(
    const grammar::ParserRule &entryRule, const Skipper &skipper,
    const ParseOptions &options, const text::TextSnapshot &text,
    const RecoveryAttemptSpec &spec, const RecoveryAttempt &selectedAttempt,
    const FailureSnapshot &failureSnapshot, TextOffset inputSize,
    std::uint32_t &recoveryAttemptRuns,
    std::uint32_t &budgetedRecoveryAttemptRuns,
    RecoveryAttemptRunCharge runCharge,
    const utils::CancellationToken &cancelToken) {
  WindowRecoverySearchResult result;
  if (options.maxRecoveryAttempts == 0) {
    return result;
  }

  ++recoveryAttemptRuns;
  if (runCharge == RecoveryAttemptRunCharge::Budgeted) {
    ++budgetedRecoveryAttemptRuns;
  }
  PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
  auto attempt = run_recovery_attempt(entryRule, skipper, options, text, spec,
                                      cancelToken);
  classify_recovery_attempt(attempt);
  score_recovery_attempt(attempt);
  trace_recovery_attempt(attempt, spec);
  // A full match obtained only by trimming a visible tail must prove that the
  // same replay contract still holds without the trim before it can replace
  // the untrimmed continuation.
  if (attempt.entryRuleMatched && attempt.fullMatch &&
      attempt.trimmedVisibleTailToEof) {
    auto untrimmedSpec = spec;
    untrimmedSpec.allowTrailingEofTrim = false;
    ++recoveryAttemptRuns;
    PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
    auto untrimmedAttempt = run_recovery_attempt(entryRule, skipper, options,
                                                 text, untrimmedSpec,
                                                 cancelToken);
    classify_recovery_attempt(untrimmedAttempt);
    score_recovery_attempt(untrimmedAttempt);
    trace_recovery_attempt(untrimmedAttempt, untrimmedSpec);
    if (trimmed_tail_delete_extends_attempt(attempt, untrimmedAttempt) &&
        spec.windows.size() < options.maxRecoveryWindows) {
      WindowPlanner planner{options};
      planner.seedAcceptedWindows(untrimmedAttempt.replayWindows);
      const auto followUpFailureSnapshot =
          untrimmedAttempt.failureSnapshot.has_value()
              ? *untrimmedAttempt.failureSnapshot
              : snapshot_from_committed_cst(*untrimmedAttempt.cst,
                                            untrimmedAttempt.maxCursorOffset);
      planner.begin(followUpFailureSnapshot, untrimmedAttempt);
      const auto plannedWindow = planner.plan();
      const auto &window = plannedWindow.window;
      auto followUpSpec = planner.buildAttemptSpec(window);
      followUpSpec.allowTrailingEofTrim = false;
      ++recoveryAttemptRuns;
      PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
      auto followUpAttempt = run_recovery_attempt(entryRule, skipper, options,
                                                  text, followUpSpec,
                                                  cancelToken);
      classify_recovery_attempt(followUpAttempt);
      score_recovery_attempt(followUpAttempt);
      trace_recovery_attempt(followUpAttempt, followUpSpec);
      consider_window_attempt_candidate(result, std::move(followUpAttempt),
                                        selectedAttempt, followUpFailureSnapshot,
                                        inputSize);
    }
    consider_window_attempt_candidate(result, std::move(untrimmedAttempt),
                                      selectedAttempt, failureSnapshot,
                                      inputSize);
    if (result.hasAnyCandidate()) {
      return result;
    }
  }

  consider_window_attempt_candidate(result, std::move(attempt),
                                    selectedAttempt, failureSnapshot,
                                    inputSize);
  return result;
}

[[nodiscard]] bool
extends_committed_prefix_with_leading_structural_placeholder(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept;

[[nodiscard]] bool preserves_committed_replay_prefix(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt,
    NonCredibleReplayContract &contract) noexcept;

[[nodiscard]] bool
has_committed_replay_prefix(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool rewrites_committed_replay_boundary(
    const RecoveryAttempt &candidate,
    const NonCredibleReplayContract &contract) noexcept;

[[nodiscard]] constexpr bool
is_trimmed_tail_fallback_attempt(const RecoveryAttempt &attempt) noexcept {
  return attempt.entryRuleMatched && attempt.fullMatch &&
         attempt.trimmedVisibleTailToEof;
}

[[nodiscard]] bool qualifies_selectable_window_attempt(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  if (extends_committed_prefix_with_leading_structural_placeholder(
          candidate, selectedAttempt)) {
    return false;
  }
  const bool preservesCommittedReplayBoundary =
      has_committed_replay_prefix(selectedAttempt);
  NonCredibleReplayContract committedReplayContract;
  if (!preserves_committed_replay_prefix(candidate, selectedAttempt,
                                         committedReplayContract) ||
      (preservesCommittedReplayBoundary &&
       rewrites_committed_replay_boundary(candidate, committedReplayContract))) {
    return false;
  }
  return is_better_recovery_attempt(candidate, selectedAttempt);
}

[[nodiscard]] bool qualifies_fallback_window_attempt(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  if (extends_committed_prefix_with_leading_structural_placeholder(
          candidate, selectedAttempt)) {
    return false;
  }
  if (!satisfies_non_credible_fallback_contract(candidate, selectedAttempt)) {
    return false;
  }
  if (selectedAttempt.fullMatch) {
    return false;
  }
  if (selectedAttempt.entryRuleMatched &&
      selectedAttempt.status == RecoveryAttemptStatus::RecoveredButNotCredible &&
      !is_better_recovery_attempt(candidate, selectedAttempt)) {
    return false;
  }
  return true;
}

void consider_window_attempt_candidate(
    WindowRecoverySearchResult &result, RecoveryAttempt attempt,
    const RecoveryAttempt &selectedAttempt,
    const FailureSnapshot &failureSnapshot, TextOffset inputSize) {
  const bool inertTrailingRejectedAttempt =
      is_inert_trailing_eof_attempt(attempt, selectedAttempt,
                                    failureSnapshot, inputSize);
  if (is_selectable_recovery_attempt(attempt)) {
    const bool qualified =
        qualifies_selectable_window_attempt(attempt, selectedAttempt);
    result.considerAttempt(std::move(attempt),
                           {.selectable = qualified,
                            .rankedSelectable = true});
    if (!qualified) {
      result.recordRejectedAttempt(inertTrailingRejectedAttempt);
    }
    return;
  }
  if (attempt.entryRuleMatched &&
      satisfies_non_credible_fallback_contract(attempt, selectedAttempt)) {
    const bool qualified =
        qualifies_fallback_window_attempt(attempt, selectedAttempt);
    result.considerAttempt(std::move(attempt),
                           {.fallback = qualified,
                            .rankedFallback = true});
    if (!qualified) {
      result.recordRejectedAttempt(inertTrailingRejectedAttempt);
    }
    return;
  }
  if (!inertTrailingRejectedAttempt) {
    PEGIUM_RECOVERY_TRACE("[parser attempt] rejected for selection status=",
                          recovery_attempt_status_name(attempt.status));
  }
  result.recordRejectedAttempt(inertTrailingRejectedAttempt);
}

void apply_window_acceptance(RecoveryAttempt acceptedAttempt,
                             const RecoveryWindow &window,
                             std::uint32_t windowIndex,
                             RecoveryAttempt &selectedAttempt,
                             WindowPlanner &windowPlanner,
                             bool fallback) {
  windowPlanner.accept(acceptedAttempt, window);
  const auto &acceptedReplayWindow = windowPlanner.acceptedWindows().back();
  selectedAttempt = std::move(acceptedAttempt);
  PEGIUM_RECOVERY_TRACE(
      fallback ? "[parser window] accepted fallback index="
               : "[parser window] accepted index=",
      windowIndex, " begin=", acceptedReplayWindow.beginOffset,
      " max=", acceptedReplayWindow.maxCursorOffset,
      " selectedWindows=", windowPlanner.acceptedWindows().size(),
      " full=", selectedAttempt.fullMatch,
      " len=", selectedAttempt.parsedLength, " cost=", selectedAttempt.editCost,
      " status=", recovery_attempt_status_name(selectedAttempt.status));
  trace_recovery_json(
      "[parser accepted-windows]",
      recovery_windows_to_json(windowPlanner.acceptedWindows()));
  trace_recovery_json("[parser selected-attempt]",
                      recovery_attempt_to_json(selectedAttempt));
}

[[nodiscard]] bool apply_qualified_window_acceptance(
    WindowRecoverySearchResult &windowSearch,
    bool allowFallback,
    const RecoveryWindow &window, std::uint32_t windowIndex,
    RecoveryAttempt &selectedAttempt,
    WindowPlanner &windowPlanner) {
  if (auto selectableAttempt = windowSearch.takeBestSelectableAttempt()) {
    apply_window_acceptance(std::move(*selectableAttempt),
                            window, windowIndex, selectedAttempt,
                            windowPlanner, false);
    return true;
  }
  if (allowFallback) {
    if (auto fallbackAttempt = windowSearch.takeBestFallbackAttempt()) {
      apply_window_acceptance(std::move(*fallbackAttempt),
                            window, windowIndex, selectedAttempt,
                            windowPlanner, true);
      return true;
    }
  }
  return false;
}

[[nodiscard]] TextOffset
first_edit_offset(const RecoveryAttempt &attempt) noexcept {
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  for (const auto &entry : attempt.recoveryEdits) {
    firstEditOffset = std::min(firstEditOffset, entry.beginOffset);
  }
  return firstEditOffset;
}

[[nodiscard]] TextOffset
last_edit_offset(const RecoveryAttempt &attempt) noexcept {
  TextOffset lastEditOffset = 0;
  for (const auto &entry : attempt.recoveryEdits) {
    lastEditOffset = std::max(lastEditOffset, entry.endOffset);
  }
  return lastEditOffset;
}

[[nodiscard]] bool preserves_stable_prefix_before_first_edit(
    const RecoveryAttempt &attempt) noexcept {
  if (!attempt.hasStablePrefix) {
    return false;
  }
  const auto firstEditOffset = first_edit_offset(attempt);
  if (firstEditOffset == std::numeric_limits<TextOffset>::max()) {
    return false;
  }
  return firstEditOffset >= attempt.stablePrefixOffset;
}

[[nodiscard]] bool
continues_after_first_edit(const RecoveryAttempt &attempt) noexcept {
  const auto firstEditOffset = first_edit_offset(attempt);
  const auto lastEditOffset = last_edit_offset(attempt);
  return firstEditOffset != std::numeric_limits<TextOffset>::max() &&
         attempt.parsedLength > lastEditOffset;
}

[[nodiscard]] bool
has_inserted_recovery(const RecoveryAttempt &attempt) noexcept {
  return std::ranges::any_of(
      attempt.recoveryEdits, [](const auto &entry) {
        return entry.kind == ParseDiagnosticKind::Inserted;
      });
}

[[nodiscard]] bool
has_delete_only_recovery(const RecoveryAttempt &attempt) noexcept {
  return !attempt.recoveryEdits.empty() &&
         std::ranges::all_of(
             attempt.recoveryEdits, [](const auto &entry) {
               return entry.kind == ParseDiagnosticKind::Deleted;
             });
}

[[nodiscard]] bool
has_visible_progress_past_last_edit(const RecoveryAttempt &attempt) noexcept {
  return !attempt.recoveryEdits.empty() &&
         attempt.lastVisibleCursorOffset > last_edit_offset(attempt);
}

[[nodiscard]] bool
has_cursor_progress_past_last_edit(const RecoveryAttempt &attempt) noexcept {
  return !attempt.recoveryEdits.empty() &&
         attempt.maxCursorOffset > last_edit_offset(attempt);
}

[[nodiscard]] bool
has_committed_replay_prefix(const RecoveryAttempt &attempt) noexcept {
  return !attempt.recoveryEdits.empty() &&
         (recovery_attempt_establishes_replay_contract(attempt) ||
          continues_after_first_edit(attempt));
}

[[nodiscard]] TextOffset
last_edit_offset(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] constexpr bool same_syntax_script_entry(
    const SyntaxScriptEntry &lhs,
    const SyntaxScriptEntry &rhs) noexcept {
  return lhs.kind == rhs.kind && lhs.offset == rhs.offset &&
         lhs.beginOffset == rhs.beginOffset && lhs.endOffset == rhs.endOffset &&
         lhs.element == rhs.element && lhs.message == rhs.message;
}

[[nodiscard]] bool
matches_selected_attempt_without_new_progress(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  return candidate.parsedLength == selectedAttempt.parsedLength &&
         candidate.maxCursorOffset == selectedAttempt.maxCursorOffset &&
         candidate.recoveryEdits.size() == selectedAttempt.recoveryEdits.size() &&
         std::equal(selectedAttempt.recoveryEdits.begin(),
                    selectedAttempt.recoveryEdits.end(),
                    candidate.recoveryEdits.begin(), same_syntax_script_entry);
}

[[nodiscard]] bool has_visible_leaf_in_offset_range(
    const RootCstNode &cst, TextOffset beginOffset,
    TextOffset endOffset) noexcept {
  if (endOffset <= beginOffset) {
    return false;
  }
  for (NodeId id = 0;; ++id) {
    const auto node = cst.get(id);
    if (!node.valid()) {
      break;
    }
    if (node.isHidden() || !node.isLeaf() || node.getText().empty()) {
      continue;
    }
    const auto nodeBeginOffset = static_cast<TextOffset>(node.getBegin());
    if (nodeBeginOffset < beginOffset) {
      continue;
    }
    if (nodeBeginOffset >= endOffset) {
      return false;
    }
    return true;
  }
  return false;
}

[[nodiscard]] std::size_t visible_leaf_count_in_offset_range(
    const RootCstNode &cst, TextOffset beginOffset,
    TextOffset endOffset) noexcept {
  if (endOffset <= beginOffset) {
    return 0u;
  }
  std::size_t visibleLeafCount = 0u;
  for (NodeId id = 0;; ++id) {
    const auto node = cst.get(id);
    if (!node.valid()) {
      break;
    }
    if (node.isHidden() || !node.isLeaf() || node.getText().empty()) {
      continue;
    }
    const auto nodeBeginOffset = static_cast<TextOffset>(node.getBegin());
    if (nodeBeginOffset < beginOffset) {
      continue;
    }
    if (nodeBeginOffset >= endOffset) {
      break;
    }
    ++visibleLeafCount;
  }
  return visibleLeafCount;
}

[[nodiscard]] constexpr bool is_structural_placeholder_insert(
    const SyntaxScriptEntry &entry) noexcept {
  if (entry.kind != ParseDiagnosticKind::Inserted ||
      entry.beginOffset != entry.endOffset) {
    return false;
  }
  if (entry.element == nullptr) {
    return !entry.message.empty();
  }
  using enum grammar::ElementKind;
  switch (entry.element->getKind()) {
  case Literal:
  case TerminalRule:
  case DataTypeRule:
    return false;
  case Create:
  case Nest:
  case Assignment:
  case AndPredicate:
  case AnyCharacter:
  case CharacterRange:
  case Group:
  case NotPredicate:
  case OrderedChoice:
  case ParserRule:
  case Repetition:
  case UnorderedGroup:
  case InfixRule:
  case InfixOperator:
    return true;
  }
  return true;
}

[[nodiscard]] bool
extends_committed_prefix_with_leading_structural_placeholder(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  if (!recovery_attempt_establishes_replay_contract(selectedAttempt) ||
      selectedAttempt.recoveryEdits.empty() ||
      candidate.recoveryEdits.size() <= selectedAttempt.recoveryEdits.size() ||
      candidate.cst == nullptr) {
    return false;
  }
  if (!std::equal(selectedAttempt.recoveryEdits.begin(),
                  selectedAttempt.recoveryEdits.end(),
                  candidate.recoveryEdits.begin(),
                  same_syntax_script_entry)) {
    return false;
  }

  const auto committedPrefixOffset =
      std::max(selectedAttempt.parsedLength, last_edit_offset(selectedAttempt));
  const auto &firstNewEdit =
      candidate.recoveryEdits[selectedAttempt.recoveryEdits.size()];
  if (firstNewEdit.beginOffset < committedPrefixOffset ||
      !is_structural_placeholder_insert(firstNewEdit)) {
    return false;
  }

  return !has_visible_leaf_in_offset_range(*candidate.cst, committedPrefixOffset,
                                           firstNewEdit.beginOffset);
}

[[nodiscard]] bool
starts_with_unstable_prefix_delete(const RecoveryAttempt &attempt) noexcept {
  if (attempt.hasStablePrefix) {
    return false;
  }
  const auto firstEditOffset = first_edit_offset(attempt);
  if (firstEditOffset != 0) {
    return false;
  }
  return std::ranges::any_of(
      attempt.recoveryEdits, [](const SyntaxScriptEntry &entry) noexcept {
        return entry.offset == 0 &&
               entry.kind == ParseDiagnosticKind::Deleted;
      });
}

[[nodiscard]] bool
starts_with_stable_boundary_delete_without_continuation(
    const RecoveryAttempt &attempt) noexcept {
  if (!attempt.hasStablePrefix ||
      first_edit_offset(attempt) != attempt.stablePrefixOffset ||
      has_delete_only_recovery(attempt) ||
      continues_after_first_edit(attempt)) {
    return false;
  }
  return std::ranges::any_of(
      attempt.recoveryEdits, [&](const SyntaxScriptEntry &entry) noexcept {
        return entry.kind == ParseDiagnosticKind::Deleted &&
               entry.beginOffset == attempt.stablePrefixOffset;
      });
}

[[nodiscard]] bool allows_local_gap_recovery_without_stable_prefix(
    const RecoveryAttempt &attempt) noexcept {
  return !attempt.hasStablePrefix &&
         !attempt.fullMatch &&
         !attempt.stableAfterRecovery &&
         attempt.reachedRecoveryTarget &&
         attempt.recoveryEdits.size() == 1u &&
         attempt.recoveryEdits.front().kind == ParseDiagnosticKind::Inserted &&
         attempt.recoveryEdits.front().beginOffset ==
             attempt.recoveryEdits.front().endOffset &&
         first_edit_offset(attempt) != 0 &&
         continues_after_first_edit(attempt) &&
         has_visible_progress_past_last_edit(attempt) &&
         has_cursor_progress_past_last_edit(attempt);
}

[[nodiscard]] bool
allows_input_prefix_delete_recovery_without_stable_prefix(
    const RecoveryAttempt &attempt) noexcept {
  if (attempt.hasStablePrefix || !attempt.fullMatch ||
      first_edit_offset(attempt) != 0 || !has_delete_only_recovery(attempt) ||
      attempt.parsedLength <= last_edit_offset(attempt) ||
      attempt.cst == nullptr || attempt.replayWindows.size() != 1u) {
    return false;
  }
  const auto &replayWindow = attempt.replayWindows.front();
  if (replayWindow.beginOffset != 0 || replayWindow.editFloorOffset != 0) {
    return false;
  }

  std::size_t visibleLeafCount = 0u;
  for (NodeId id = 0;; ++id) {
    const auto node = attempt.cst->get(id);
    if (!node.valid()) {
      break;
    }
    if (node.isHidden() || !node.isLeaf() || node.getText().empty()) {
      continue;
    }
    ++visibleLeafCount;
    if (visibleLeafCount >= 2u) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool
allows_local_delete_gap_recovery_without_stable_prefix(
    const RecoveryAttempt &attempt) noexcept {
  return !attempt.hasStablePrefix && !attempt.fullMatch &&
         first_edit_offset(attempt) != 0 &&
         has_delete_only_recovery(attempt) &&
         continues_after_first_edit(attempt) &&
         has_visible_progress_past_last_edit(attempt) &&
         has_cursor_progress_past_last_edit(attempt);
}

[[nodiscard]] bool
preserved_prefix_last_window_overreach(
    const RecoveryAttempt &attempt) noexcept {
  if (!preserves_stable_prefix_before_first_edit(attempt) ||
      attempt.replayWindows.empty() || attempt.recoveryEdits.empty() ||
      has_delete_only_recovery(attempt)) {
    return false;
  }
  return last_edit_offset(attempt) > attempt.replayWindows.back().maxCursorOffset;
}

[[nodiscard]] bool
requires_narrowed_window_retry(const RecoveryAttempt &attempt,
                               bool selectableAttempt) noexcept {
  if (!preserved_prefix_last_window_overreach(attempt)) {
    return false;
  }
  return selectableAttempt || attempt.editCost > attempt.configuredMaxEditCost;
}

[[nodiscard]] std::uint32_t
narrowed_window_token_count(std::uint32_t tokenCount) noexcept {
  return std::max<std::uint32_t>(
      RecoveryContext::kInternalRecoveryStabilityTokenCount, tokenCount / 2u);
}

[[nodiscard]] RecoveryWindow
build_narrowed_recovery_window(const FailureSnapshot &snapshot,
                               const RecoveryWindow &window) noexcept {
  const auto narrowedBackwardTokenCount =
      narrowed_window_token_count(window.tokenCount);
  const auto narrowedForwardTokenCount =
      narrowed_window_token_count(window.forwardTokenCount);
  auto narrowedWindow = compute_recovery_window(
      snapshot, narrowedBackwardTokenCount, narrowedForwardTokenCount,
      window.editFloorOffset);
  narrowedWindow.stablePrefixOffset = window.stablePrefixOffset;
  narrowedWindow.hasStablePrefix = window.hasStablePrefix;
  return narrowedWindow;
}

[[nodiscard]] bool
requires_narrowed_window_search(const WindowRecoverySearchResult &windowSearch,
                                const RecoveryWindow &window) noexcept {
  if (window.tokenCount <= RecoveryContext::kInternalRecoveryStabilityTokenCount &&
      window.forwardTokenCount <=
          RecoveryContext::kInternalRecoveryStabilityTokenCount) {
    return false;
  }
  bool selectableAttempt = false;
  const auto *const narrowingCandidate =
      windowSearch.narrowingRetryCandidate(selectableAttempt);
  return narrowingCandidate != nullptr &&
         requires_narrowed_window_retry(*narrowingCandidate,
                                        selectableAttempt);
}

[[nodiscard]] ParseOptions
build_narrowed_recovery_search_options(const ParseOptions &options) noexcept {
  ParseOptions narrowedOptions = options;
  narrowedOptions.recoveryWindowTokenCount =
      narrowed_window_token_count(options.recoveryWindowTokenCount);
  narrowedOptions.maxRecoveryWindowTokenCount =
      std::min(narrowedOptions.maxRecoveryWindowTokenCount,
               narrowedOptions.recoveryWindowTokenCount);
  return narrowedOptions;
}

[[nodiscard]] bool
requires_narrowed_global_rerun(const RecoverySearchRunResult &result,
                               const ParseOptions &options) noexcept {
  return result.selectedAttempt.status ==
             RecoveryAttemptStatus::RecoveredButNotCredible &&
         options.recoveryWindowTokenCount >
             RecoveryContext::kInternalRecoveryStabilityTokenCount &&
         requires_narrowed_window_retry(result.selectedAttempt, false);
}

[[nodiscard]] bool trim_long_visible_tail_to_eof(RecoveryContext &ctx) {
  if (!ctx.hasHadEdits()) {
    return false;
  }

  const char *const visibleCursor = ctx.skip_without_builder(ctx.cursor());
  if (visibleCursor == ctx.end) {
    return false;
  }

  const auto visibleOffset = static_cast<TextOffset>(visibleCursor - ctx.begin);
  const auto inputEndOffset = static_cast<TextOffset>(ctx.end - ctx.begin);
  if (ctx.completedRecoveryWindowCount() == 0 &&
      !ctx.hasReachedRecoveryTarget() && !ctx.isStableAfterRecovery()) {
    return false;
  }
  if (!ctx.canEditAtOffset(visibleOffset)) {
    return false;
  }

  const auto checkpoint = ctx.mark();
  const bool needsOverflowDeleteScan =
      inputEndOffset - visibleOffset > ctx.maxConsecutiveCodepointDeletes;
  const auto savedMaxConsecutiveDeletes = ctx.maxConsecutiveCodepointDeletes;
  const auto savedMaxEditsPerAttempt = ctx.maxEditsPerAttempt;
  const auto savedMaxEditCost = ctx.maxEditCost;
  auto deleteOnlyGuard = ctx.withEditPermissions(false, true);
  (void)deleteOnlyGuard;
  if (needsOverflowDeleteScan) {
    ctx.maxConsecutiveCodepointDeletes =
        std::numeric_limits<std::uint32_t>::max();
    ctx.maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
    ctx.maxEditCost = std::numeric_limits<std::uint32_t>::max();
  }

  while (ctx.cursor() < ctx.end) {
    if (ctx.skip_without_builder(ctx.cursor()) == ctx.end) {
      ctx.skip();
      ctx.maxConsecutiveCodepointDeletes = savedMaxConsecutiveDeletes;
      ctx.maxEditsPerAttempt = savedMaxEditsPerAttempt;
      ctx.maxEditCost = savedMaxEditCost;
      ctx.allowBudgetOverflowEdits();
      return true;
    }
    if (!ctx.deleteOneCodepoint()) {
      break;
    }
  }
  if (ctx.cursor() == ctx.end) {
    ctx.maxConsecutiveCodepointDeletes = savedMaxConsecutiveDeletes;
    ctx.maxEditsPerAttempt = savedMaxEditsPerAttempt;
    ctx.maxEditCost = savedMaxEditCost;
    ctx.allowBudgetOverflowEdits();
    return true;
  }

  ctx.maxConsecutiveCodepointDeletes = savedMaxConsecutiveDeletes;
  ctx.maxEditsPerAttempt = savedMaxEditsPerAttempt;
  ctx.maxEditCost = savedMaxEditCost;
  ctx.rewind(checkpoint);
  return false;
}

[[nodiscard]] bool trimmed_tail_delete_extends_attempt(
    const RecoveryAttempt &trimmed,
    const RecoveryAttempt &untrimmed) noexcept {
  if (!trimmed.trimmedVisibleTailToEof || !trimmed.fullMatch ||
      !untrimmed.entryRuleMatched || untrimmed.fullMatch ||
      trimmed.recoveryEdits.size() <= untrimmed.recoveryEdits.size()) {
    return false;
  }
  if (!std::equal(untrimmed.recoveryEdits.begin(), untrimmed.recoveryEdits.end(),
                  trimmed.recoveryEdits.begin(), same_syntax_script_entry)) {
    return false;
  }
  const auto trailingDeletes = std::span(trimmed.recoveryEdits)
                                   .subspan(untrimmed.recoveryEdits.size());
  return std::ranges::all_of(trailingDeletes, [&](const auto &entry) noexcept {
    return entry.kind == ParseDiagnosticKind::Deleted &&
           entry.beginOffset >= untrimmed.parsedLength;
  });
}

[[nodiscard]] bool
can_try_prefix_delete_retry(const grammar::ParserRule &entryRule,
                            const RecoveryAttemptSpec &spec) noexcept {
  enum class LeadingVisibleEntryKind : std::uint8_t {
    None,
    Unguarded,
    PredicateGuarded,
  };

  const auto leadingVisibleEntryKind =
      [&]() -> LeadingVisibleEntryKind {
    const auto inspect = [](const auto &self,
                            const grammar::AbstractElement &element)
        -> LeadingVisibleEntryKind {
      using enum grammar::ElementKind;
      switch (element.getKind()) {
      case AndPredicate:
        return LeadingVisibleEntryKind::PredicateGuarded;
      case NotPredicate:
        return LeadingVisibleEntryKind::PredicateGuarded;
      case Assignment:
        return self(self,
                    *static_cast<const grammar::Assignment &>(element)
                         .getElement());
      case ParserRule:
        return self(self, *static_cast<const grammar::ParserRule &>(element)
                               .getElement());
      case InfixRule:
        return self(self, *static_cast<const grammar::InfixRule &>(element)
                               .getElement());
      case DataTypeRule:
        return self(self, *static_cast<const grammar::DataTypeRule &>(element)
                               .getElement());
      case Repetition: {
        const auto &repetition = static_cast<const grammar::Repetition &>(element);
        return repetition.getMin() == 0u
                   ? LeadingVisibleEntryKind::None
                   : self(self, *repetition.getElement());
      }
      case Group: {
        const auto &group = static_cast<const grammar::Group &>(element);
        for (std::size_t index = 0; index < group.size(); ++index) {
          const auto *child = group.get(index);
          if (child == nullptr) {
            continue;
          }
          const auto childKind = self(self, *child);
          if (childKind != LeadingVisibleEntryKind::None) {
            return childKind;
          }
          if (!child->isNullable()) {
            return LeadingVisibleEntryKind::None;
          }
        }
        return LeadingVisibleEntryKind::None;
      }
      case OrderedChoice: {
        const auto &choice =
            static_cast<const grammar::OrderedChoice &>(element);
        bool sawGuarded = false;
        for (std::size_t index = 0; index < choice.size(); ++index) {
          const auto *child = choice.get(index);
          if (child == nullptr) {
            continue;
          }
          const auto childKind = self(self, *child);
          if (childKind == LeadingVisibleEntryKind::Unguarded) {
            return LeadingVisibleEntryKind::Unguarded;
          }
          sawGuarded =
              sawGuarded ||
              childKind == LeadingVisibleEntryKind::PredicateGuarded;
        }
        return sawGuarded ? LeadingVisibleEntryKind::PredicateGuarded
                          : LeadingVisibleEntryKind::None;
      }
      case UnorderedGroup: {
        const auto &group =
            static_cast<const grammar::UnorderedGroup &>(element);
        bool sawGuarded = false;
        for (std::size_t index = 0; index < group.size(); ++index) {
          const auto *child = group.get(index);
          if (child == nullptr) {
            continue;
          }
          const auto childKind = self(self, *child);
          if (childKind == LeadingVisibleEntryKind::Unguarded) {
            return LeadingVisibleEntryKind::Unguarded;
          }
          sawGuarded =
              sawGuarded ||
              childKind == LeadingVisibleEntryKind::PredicateGuarded;
        }
        return sawGuarded ? LeadingVisibleEntryKind::PredicateGuarded
                          : LeadingVisibleEntryKind::None;
      }
      case Create:
      case Nest:
        return LeadingVisibleEntryKind::None;
      case AnyCharacter:
      case CharacterRange:
      case Literal:
      case TerminalRule:
      case InfixOperator:
        return LeadingVisibleEntryKind::Unguarded;
      }
      return LeadingVisibleEntryKind::Unguarded;
    };

    return inspect(inspect, *entryRule.getElement());
  }();

  return spec.windows.size() == 1u && spec.windows.front().beginOffset == 0 &&
         spec.windows.front().editFloorOffset == 0 &&
         entryRule.getKind() == grammar::ElementKind::ParserRule &&
         leadingVisibleEntryKind != LeadingVisibleEntryKind::PredicateGuarded;
}

[[nodiscard]] bool
try_prefix_delete_retry_entry_rule(RecoveryContext &ctx,
                                   const grammar::ParserRule &entryRule) {
  if (!ctx.canDelete() || ctx.cursor() != ctx.begin) {
    return false;
  }

  const bool savedAllowInsert = ctx.allowInsert;
  const bool savedAllowDelete = ctx.allowDelete;
  const auto recoveryCheckpoint = ctx.mark();
  const bool previousSkipAfterDelete = ctx.skipAfterDelete;
  ctx.skipAfterDelete = false;
  auto deleteOnlyGuard = ctx.withEditPermissions(false, true);
  (void)deleteOnlyGuard;
  detail::ExtendedDeleteScanBudgetScope overflowBudgetScope{ctx};

  const auto try_scan_pass = [&](bool overflowBudget) {
    while (ctx.deleteOneCodepoint()) {
      const auto retryCheckpoint = ctx.mark();
      const char *const savedFurthestExploredCursor =
          ctx.furthestExploredCursor();
      ctx.skip();
      ctx.skipAfterDelete = previousSkipAfterDelete;
      auto retryEditGuard =
          ctx.withEditPermissions(savedAllowInsert, savedAllowDelete);
      (void)retryEditGuard;
      const bool matched = entryRule.recover(ctx);
      if (matched) {
        const auto retryVisibleLeafCount =
            retryCheckpoint.parseCheckpoint.failureHistorySize;
        const auto recoveredVisibleLeafCount = ctx.failureHistorySize();
        const bool exposesStructuredEntry =
            recoveredVisibleLeafCount >= retryVisibleLeafCount + 2u ||
            (ctx.cursor() == ctx.end &&
             recoveredVisibleLeafCount > retryVisibleLeafCount);
        if (!exposesStructuredEntry) {
          ctx.rewind(retryCheckpoint);
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
          continue;
        }
        if (overflowBudget) {
          overflowBudgetScope.commitOverflowEdits();
        }
        return true;
      }
      ctx.skipAfterDelete = false;
      ctx.rewind(retryCheckpoint);
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
    }
    return false;
  };

  if (try_scan_pass(false)) {
    ctx.skipAfterDelete = previousSkipAfterDelete;
    return true;
  }

  if (overflowBudgetScope.tryEnable()) {
    if (try_scan_pass(true)) {
      ctx.skipAfterDelete = previousSkipAfterDelete;
      return true;
    }
  }

  ctx.skipAfterDelete = previousSkipAfterDelete;
  ctx.rewind(recoveryCheckpoint);
  return false;
}

} // namespace

void WindowPlanner::seedAcceptedWindows(
    std::span<const RecoveryWindow> acceptedWindows) {
  _acceptedWindows.assign(acceptedWindows.begin(), acceptedWindows.end());
}

void WindowPlanner::begin(const FailureSnapshot &failureSnapshot,
                          const RecoveryAttempt &selectedAttempt) noexcept {
  _failureSnapshot = &failureSnapshot;
  const bool carriesAcceptedFallback =
      !_acceptedWindows.empty() &&
      !recovery_attempt_establishes_replay_contract(selectedAttempt);
  _allowBackwardWidenAfterForwardExhausted = carriesAcceptedFallback;
  const auto prefixContract =
      recovery_window_prefix_contract(failureSnapshot, selectedAttempt);
  if (carriesAcceptedFallback) {
    // A carried fallback window must preserve the replay contract that was
    // already accepted, but subsequent windows should resume from the actual
    // parsed prefix that fallback replay kept alive.
    TextOffset carriedPrefixOffset = selectedAttempt.parsedLength;
    if (failureSnapshot.hasFailureToken &&
        failureSnapshot.failureTokenIndex <
            failureSnapshot.failureLeafHistory.size()) {
      carriedPrefixOffset = std::min(
          carriedPrefixOffset,
          failureSnapshot.failureLeafHistory[failureSnapshot.failureTokenIndex]
              .beginOffset);
    }
    _editFloorOffset = carriedPrefixOffset;
    _stablePrefixOffset = carriedPrefixOffset;
    _hasStablePrefix = carriedPrefixOffset != 0;
  } else {
    _editFloorOffset = prefixContract.editFloorOffset;
    _stablePrefixOffset = prefixContract.stablePrefixOffset;
    _hasStablePrefix = prefixContract.hasStablePrefix;
  }
  _windowTokenCount =
      std::max<std::uint32_t>(1u, _options.recoveryWindowTokenCount);
  _forwardWindowTokenCount =
      !_acceptedWindows.empty()
          ? std::max(_windowTokenCount, _acceptedWindows.back().forwardTokenCount)
          : _windowTokenCount;
  _triedFullHistoryWindow = false;
}

PlannedRecoveryWindow WindowPlanner::plan() const noexcept {
  auto window =
      compute_recovery_window(*_failureSnapshot, _windowTokenCount,
                              _forwardWindowTokenCount, _editFloorOffset);
  window.stablePrefixOffset = _stablePrefixOffset;
  window.hasStablePrefix = _hasStablePrefix;
  return {.window = window, .requestedTokenCount = _windowTokenCount};
}

bool WindowPlanner::advance(const RecoveryWindow &currentWindow,
                            bool preferBackwardContext,
                            bool preferImmediateMaxForwardWiden,
                            bool preferImmediateMaxBackwardWiden) noexcept {
  const auto earliestAcceptedBeginOffset =
      _acceptedWindows.empty() ? TextOffset{0}
                               : _acceptedWindows.front().beginOffset;
  const auto try_widen_backward = [&]() -> bool {
    if (preferImmediateMaxBackwardWiden &&
        _options.maxRecoveryWindowTokenCount > _windowTokenCount) {
      const auto widenedWindow = compute_recovery_window(
          *_failureSnapshot, _options.maxRecoveryWindowTokenCount,
          _forwardWindowTokenCount, _editFloorOffset);
      if (!_acceptedWindows.empty() &&
          widenedWindow.beginOffset < earliestAcceptedBeginOffset) {
        return false;
      }
      if (widenedWindow.beginOffset != currentWindow.beginOffset) {
        PEGIUM_RECOVERY_TRACE("[parser window] widen backward tokenCount ",
                              _windowTokenCount, " -> ",
                              _options.maxRecoveryWindowTokenCount);
        _windowTokenCount = _options.maxRecoveryWindowTokenCount;
        return true;
      }
    }

    auto nextBackwardTokenCount =
        next_recovery_window_token_count(_windowTokenCount, _options);
    while (nextBackwardTokenCount.has_value()) {
      const auto widenedWindow =
          compute_recovery_window(*_failureSnapshot, *nextBackwardTokenCount,
                                  _forwardWindowTokenCount, _editFloorOffset);
      if (!_acceptedWindows.empty() &&
          widenedWindow.beginOffset < earliestAcceptedBeginOffset) {
        nextBackwardTokenCount =
            next_recovery_window_token_count(*nextBackwardTokenCount, _options);
        continue;
      }
      if (widenedWindow.beginOffset != currentWindow.beginOffset) {
        PEGIUM_RECOVERY_TRACE("[parser window] widen backward tokenCount ",
                              _windowTokenCount, " -> ",
                              *nextBackwardTokenCount);
        _windowTokenCount = *nextBackwardTokenCount;
        return true;
      }
      nextBackwardTokenCount =
          next_recovery_window_token_count(*nextBackwardTokenCount, _options);
    }
    return false;
  };

  if (_hasStablePrefix) {
    if (preferBackwardContext && try_widen_backward()) {
      return true;
    }
    if (const auto nextForwardTokenCount = next_recovery_window_token_count(
            _forwardWindowTokenCount, _options);
        nextForwardTokenCount.has_value()) {
      const auto widenedForwardTokenCount =
          preferImmediateMaxForwardWiden
              ? _options.maxRecoveryWindowTokenCount
              : *nextForwardTokenCount;
      PEGIUM_RECOVERY_TRACE("[parser window] widen forward tokenCount ",
                            _forwardWindowTokenCount, " -> ",
                            widenedForwardTokenCount);
      _forwardWindowTokenCount = widenedForwardTokenCount;
      return true;
    }
    return (_allowBackwardWidenAfterForwardExhausted || preferBackwardContext) &&
           try_widen_backward();
  }

  if (_acceptedWindows.empty() && currentWindow.beginOffset == 0 &&
      _windowTokenCount ==
          std::max<std::uint32_t>(1u, _options.recoveryWindowTokenCount) &&
      _options.maxRecoveryWindowTokenCount > _windowTokenCount) {
    PEGIUM_RECOVERY_TRACE("[parser window] widen root failure tokenCount ",
                          _windowTokenCount, " -> ",
                          _options.maxRecoveryWindowTokenCount);
    _windowTokenCount = _options.maxRecoveryWindowTokenCount;
    return true;
  }

  if (const auto nextWindowTokenCount =
          next_recovery_window_token_count(_windowTokenCount, _options);
      nextWindowTokenCount.has_value()) {
    PEGIUM_RECOVERY_TRACE("[parser window] widen tokenCount ",
                          _windowTokenCount, " -> ", *nextWindowTokenCount);
    _windowTokenCount = *nextWindowTokenCount;
    return true;
  }

  const auto fullHistoryTokenCount = static_cast<std::uint32_t>(
      std::min<std::size_t>(_failureSnapshot->failureLeafHistory.size(),
                            std::numeric_limits<std::uint32_t>::max()));
  if (!_triedFullHistoryWindow && !_hasStablePrefix &&
      currentWindow.beginOffset > 0 &&
      fullHistoryTokenCount > _windowTokenCount) {
    PEGIUM_RECOVERY_TRACE("[parser window] widen to full-history tokenCount ",
                          _windowTokenCount, " -> ", fullHistoryTokenCount);
    _windowTokenCount = fullHistoryTokenCount;
    _triedFullHistoryWindow = true;
    return true;
  }

  return false;
}

RecoveryAttemptSpec
WindowPlanner::buildAttemptSpec(const RecoveryWindow &window) const {
  RecoveryAttemptSpec spec;
  spec.windows.reserve(_acceptedWindows.size() + 1u);
  for (const auto &acceptedWindow : _acceptedWindows) {
    // Accepted windows replay with the stored replay contract that originally
    // validated their kept edits. A later recovery window may extend the
    // repaired suffix, but it must not weaken an earlier replay window and
    // retroactively degrade that already accepted prefix repair.
    spec.windows.push_back(acceptedWindow);
  }
  spec.windows.push_back(window);
  return spec;
}

void WindowPlanner::accept(const RecoveryAttempt &attempt,
                           const RecoveryWindow &window) {
  if (attempt.replayWindows.size() >= _acceptedWindows.size() + 1u) {
    _acceptedWindows.assign(attempt.replayWindows.begin(),
                            attempt.replayWindows.end());
    return;
  }
  _acceptedWindows.push_back(window);
}

RecoveryAttempt
run_recovery_attempt(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const text::TextSnapshot &text,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken) {
  RecoveryAttempt attempt;
  attempt.stablePrefixOffset =
      spec.windows.empty() ? 0 : spec.windows.front().stablePrefixOffset;
  attempt.hasStablePrefix =
      !spec.windows.empty() && spec.windows.front().hasStablePrefix;
  attempt.configuredMaxEditCost = options.maxRecoveryEditCost;
  const auto inputSize = static_cast<TextOffset>(text.size());

  FailureHistoryRecorder failureRecorder(text.view().data());
  auto cst = std::make_unique<RootCstNode>(text);
  CstBuilder builder(*cst);
  RecoveryContext parseCtx{builder, skipper, failureRecorder, cancelToken};
  std::vector<RecoveryContext::EditWindow> editWindows;
  editWindows.reserve(spec.windows.size());
  for (const auto &window : spec.windows) {
    editWindows.push_back({.beginOffset = window.beginOffset,
                           .editFloorOffset = window.editFloorOffset,
                           .maxCursorOffset = window.maxCursorOffset,
                           .forwardTokenCount = window.forwardTokenCount,
                           .replayForwardTokenCount = std::min(
                               window.forwardTokenCount,
                               RecoveryContext::
                                   kInternalRecoveryStabilityTokenCount)});
  }
  parseCtx.setEditWindows(std::move(editWindows));
  parseCtx.trackEditState = true;
  parseCtx.recoveryState.windowReplay.inRecoveryPhase = false;
  parseCtx.allowTopLevelPartialSuccess = true;
  parseCtx.maxConsecutiveCodepointDeletes =
      options.maxConsecutiveCodepointDeletes;
  parseCtx.maxEditsPerAttempt = options.maxRecoveryEditsPerAttempt;
  parseCtx.maxEditCost = options.maxRecoveryEditCost;

  parseCtx.skip();
  const auto attemptCheckpoint = parseCtx.mark();
  attempt.entryRuleMatched = entryRule.recover(parseCtx);
  const auto failureParsedLength = parseCtx.cursorOffset();
  const auto failureMaxCursorOffset = parseCtx.maxCursorOffset();
  auto failureRecoveryEdits = parseCtx.snapshotRecoveryEdits();
  const auto failureEditCost = parseCtx.currentEditCost();
  const auto failureEditCount = parseCtx.currentEditCount();
  const auto failureCompletedRecoveryWindows =
      parseCtx.completedRecoveryWindowCount();
  const auto failureReachedRecoveryTarget = parseCtx.hasReachedRecoveryTarget();
  const auto failureStableAfterRecovery = parseCtx.isStableAfterRecovery();

  if (!attempt.entryRuleMatched) {
    parseCtx.rewind(attemptCheckpoint);
    if (can_try_prefix_delete_retry(entryRule, spec)) {
      attempt.entryRuleMatched =
          try_prefix_delete_retry_entry_rule(parseCtx, entryRule);
    }
  }

  parseCtx.skip();
  if (attempt.entryRuleMatched && spec.allowTrailingEofTrim) {
    attempt.trimmedVisibleTailToEof = trim_long_visible_tail_to_eof(parseCtx);
  }
  parseCtx.finalizeRecoveryAtEof();
  attempt.parsedLength =
      attempt.entryRuleMatched ? parseCtx.cursorOffset() : failureParsedLength;
  attempt.lastVisibleCursorOffset = parseCtx.lastVisibleCursorOffset();
  attempt.recoveryEdits = normalize_syntax_script(
      attempt.entryRuleMatched ? parseCtx.takeRecoveryEdits()
                               : failureRecoveryEdits);
  attempt.fullMatch =
      attempt.entryRuleMatched && attempt.parsedLength == inputSize;
  attempt.maxCursorOffset = attempt.entryRuleMatched
                                ? parseCtx.maxCursorOffset()
                                : failureMaxCursorOffset;
  if (attempt.entryRuleMatched) {
    attempt.replayWindows = spec.windows;
    const auto replayForwardCounts = parseCtx.replayForwardTokenCounts();
    const auto replayCount =
        std::min(attempt.replayWindows.size(), replayForwardCounts.size());
    for (std::size_t i = 0; i < replayCount; ++i) {
      attempt.replayWindows[i].forwardTokenCount = replayForwardCounts[i];
    }
  }
  if (!attempt.fullMatch && parseCtx.isFailureHistoryRecordingEnabled()) {
    const auto failureSnapshotOffset =
        attempt.entryRuleMatched
            ? std::max(attempt.parsedLength, attempt.maxCursorOffset)
            : attempt.maxCursorOffset;
    auto trackedSnapshot = failureRecorder.snapshot(failureSnapshotOffset);
    auto committedSnapshot =
        snapshot_from_committed_cst(*cst, failureSnapshotOffset);
    const bool trackedCarriesLaterVisibleFailureHistory =
        last_failure_leaf_end_offset(trackedSnapshot) >
        last_failure_leaf_end_offset(committedSnapshot);
    const bool committedCarriesLaterVisibleFailureHistory =
        last_failure_leaf_end_offset(committedSnapshot) >
        last_failure_leaf_end_offset(trackedSnapshot);
    if (committedCarriesLaterVisibleFailureHistory) {
      trackedSnapshot = std::move(committedSnapshot);
    } else if (!committedSnapshot.failureLeafHistory.empty() &&
               !trackedCarriesLaterVisibleFailureHistory &&
               (trackedSnapshot.failureLeafHistory.empty() ||
                (!trackedSnapshot.hasFailureToken &&
                 committedSnapshot.hasFailureToken))) {
      trackedSnapshot = std::move(committedSnapshot);
    }
    attempt.failureSnapshot = std::move(trackedSnapshot);
  }
  attempt.editCost =
      attempt.entryRuleMatched ? parseCtx.currentEditCost() : failureEditCost;
  attempt.editCount =
      attempt.entryRuleMatched ? parseCtx.currentEditCount() : failureEditCount;
  attempt.completedRecoveryWindows =
      attempt.entryRuleMatched ? parseCtx.completedRecoveryWindowCount()
                               : failureCompletedRecoveryWindows;
  attempt.reachedRecoveryTarget = attempt.entryRuleMatched
                                      ? parseCtx.hasReachedRecoveryTarget()
                                      : failureReachedRecoveryTarget;
  attempt.stableAfterRecovery = attempt.entryRuleMatched
                                    ? parseCtx.isStableAfterRecovery()
                                    : failureStableAfterRecovery;
  (void)builder.getRootCstNode();
  attempt.cst = std::move(cst);
  return attempt;
}

[[nodiscard]] RecoverySearchRunResult
run_recovery_search_pass(const grammar::ParserRule &entryRule,
                         const Skipper &skipper, const ParseOptions &options,
                         const text::TextSnapshot &text,
                         const utils::CancellationToken &cancelToken) {
  RecoverySearchRunResult result;
  const auto inputSize = static_cast<TextOffset>(text.size());
  WindowPlanner windowPlanner{options};

  PEGIUM_STEP_TRACE_INC(StepCounter::ParsePhaseRuns);
  auto strictFailureStage =
      run_strict_failure_stage(entryRule, skipper, options, text, cancelToken);
  result.selectedAttempt = std::move(strictFailureStage.strictAttempt);
  result.failureVisibleCursorOffset =
      strictFailureStage.failureVisibleCursorOffset;
  result.strictParseRuns = strictFailureStage.strictParseRuns;

  if (!strictFailureStage.failureSnapshot.has_value()) {
    return result;
  }

  std::uint32_t recoveryPhaseIndex = 0;
  std::uint32_t windowIndex = 0;
  while (recoveryPhaseIndex < options.maxRecoveryWindows) {
    if (result.selectedAttempt.fullMatch) {
      break;
    }

    const auto failureSnapshot =
        windowIndex == 0 ? *strictFailureStage.failureSnapshot
                         : (result.selectedAttempt.failureSnapshot.has_value()
                                ? *result.selectedAttempt.failureSnapshot
                                : snapshot_from_committed_cst(
                                      *result.selectedAttempt.cst,
                                      result.selectedAttempt.maxCursorOffset));

    bool acceptedWindow = false;
    windowPlanner.begin(failureSnapshot, result.selectedAttempt);
    while (true) {
      if (recovery_attempt_budget_exhausted(result, options)) {
        PEGIUM_RECOVERY_TRACE(
            "[parser window] stop after global attempt budget exhausted");
        break;
      }

      const auto plannedWindow = windowPlanner.plan();
      const auto &window = plannedWindow.window;
      trace_recovery_window(windowIndex, plannedWindow.requestedTokenCount,
                            window, windowPlanner.acceptedWindows().size());
      if (!windowPlanner.acceptedWindows().empty()) {
        const auto &lastWindow = windowPlanner.acceptedWindows().back();
        if (lastWindow.beginOffset == window.beginOffset &&
            lastWindow.maxCursorOffset == window.maxCursorOffset) {
          PEGIUM_RECOVERY_TRACE(
              "[parser window] retry duplicate window after prior progress");
        }
      }
      ++result.recoveryWindowsTried;

      const auto spec = windowPlanner.buildAttemptSpec(window);
      if (windowIndex == 0 && windowPlanner.acceptedWindows().empty() &&
          is_near_eof_tail_failure(failureSnapshot, inputSize)) {
        PEGIUM_RECOVERY_TRACE(
            "[parser window] skip inert late EOF recovery attempt");
        break;
      }

      auto windowSearch = evaluate_recovery_window_attempts(
          entryRule, skipper, options, text, spec, result.selectedAttempt,
          failureSnapshot, inputSize, result.recoveryAttemptRuns,
          result.budgetedRecoveryAttemptRuns,
          RecoveryAttemptRunCharge::Budgeted, cancelToken);
      if (requires_narrowed_window_search(windowSearch, window)) {
        const auto narrowWindow =
            build_narrowed_recovery_window(failureSnapshot, window);
        const auto narrowSpec = windowPlanner.buildAttemptSpec(narrowWindow);
        auto narrowWindowSearch = evaluate_recovery_window_attempts(
            entryRule, skipper, options, text, narrowSpec,
            result.selectedAttempt, failureSnapshot, inputSize,
            result.recoveryAttemptRuns, result.budgetedRecoveryAttemptRuns,
            RecoveryAttemptRunCharge::Budgeted, cancelToken);
        windowSearch.merge(std::move(narrowWindowSearch));
      }
      if (windowSearch.hasSelectableAttempt()) {
        const bool preferBackwardContext =
            window.hasStablePrefix &&
            window.beginOffset > window.stablePrefixOffset &&
            matches_selected_attempt_without_new_progress(
                *windowSearch.bestSelectableAttempt(), result.selectedAttempt);
        acceptedWindow = apply_qualified_window_acceptance(
            windowSearch, false, window, windowIndex, result.selectedAttempt,
            windowPlanner);
        if (acceptedWindow) {
          break;
        }
        PEGIUM_RECOVERY_TRACE(
            "[parser window] credible attempts found, widening for a better candidate");
        if (!windowPlanner.advance(window, preferBackwardContext)) {
          PEGIUM_RECOVERY_TRACE(
              "[parser window] credible attempts found, no better candidate and cannot widen");
          break;
        }
        continue;
      }
      if (windowSearch.sawRejectedAttempt &&
          windowSearch.onlyInertTrailingRejectedAttempts) {
        PEGIUM_RECOVERY_TRACE(
            "[parser window] stop widening after inert trailing EOF failure");
        break;
      }
      bool preferBackwardContext = false;
      bool preferImmediateMaxForwardWiden = false;
      bool preferImmediateMaxBackwardWiden = false;
      if (window.hasStablePrefix) {
        bool selectableAttempt = false;
        if (const auto *retryCandidate =
                windowSearch.narrowingRetryCandidate(selectableAttempt);
            retryCandidate != nullptr &&
            matches_selected_attempt_without_new_progress(*retryCandidate,
                                                         result.selectedAttempt)) {
          preferBackwardContext = true;
        }
        preferImmediateMaxForwardWiden =
            !preferBackwardContext &&
            windowSearch.bestFallbackAttempt() != nullptr;
        preferImmediateMaxBackwardWiden =
            windowSearch.bestFallbackAttempt() != nullptr &&
            windowSearch.bestFallbackAttempt()->trimmedVisibleTailToEof;
      }
      if (!windowPlanner.advance(window, preferBackwardContext,
                                 preferImmediateMaxForwardWiden,
                                 preferImmediateMaxBackwardWiden)) {
        acceptedWindow = apply_qualified_window_acceptance(
            windowSearch, true, window, windowIndex, result.selectedAttempt,
            windowPlanner);
        PEGIUM_RECOVERY_TRACE(
            "[parser window] no credible attempt and cannot widen");
        break;
      }
    }

    if (!acceptedWindow) {
      PEGIUM_RECOVERY_TRACE("[parser parse] stop after window index=",
                            windowIndex);
      break;
    }

    if (recovery_attempt_establishes_replay_contract(result.selectedAttempt)) {
      ++recoveryPhaseIndex;
    }

    if (!result.selectedAttempt.fullMatch) {
      PEGIUM_RECOVERY_TRACE(
          "[parser parse] resume strict after window index=", windowIndex,
          " parsed=", result.selectedAttempt.parsedLength,
          " max=", result.selectedAttempt.maxCursorOffset);
    }

    ++windowIndex;
  }

  result.selectedWindows.assign(windowPlanner.acceptedWindows().begin(),
                                windowPlanner.acceptedWindows().end());
  return result;
}

RecoverySearchRunResult
run_recovery_search(const grammar::ParserRule &entryRule,
                    const Skipper &skipper, const ParseOptions &options,
                    const text::TextSnapshot &text,
                    const utils::CancellationToken &cancelToken) {
  PEGIUM_STEP_TRACE_RESET();

  ParseOptions currentOptions = options;
  auto result = run_recovery_search_pass(entryRule, skipper, currentOptions,
                                         text, cancelToken);

  while (requires_narrowed_global_rerun(result, currentOptions)) {
    const auto narrowedOptions =
        build_narrowed_recovery_search_options(currentOptions);
    auto narrowedResult = run_recovery_search_pass(
        entryRule, skipper, narrowedOptions, text, cancelToken);
    if (!is_better_recovery_attempt(narrowedResult.selectedAttempt,
                                    result.selectedAttempt)) {
      break;
    }
    narrowedResult.recoveryAttemptRuns += result.recoveryAttemptRuns;
    narrowedResult.budgetedRecoveryAttemptRuns +=
        result.budgetedRecoveryAttemptRuns;
    narrowedResult.recoveryWindowsTried += result.recoveryWindowsTried;
    narrowedResult.strictParseRuns += result.strictParseRuns;
    currentOptions = narrowedOptions;
    result = std::move(narrowedResult);
  }

  return result;
}

void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept {
  using enum RecoveryAttemptStatus;
  if (!attempt.entryRuleMatched) {
    attempt.status = StrictFailure;
    return;
  }

  const bool preservesStablePrefixBeforeFirstEdit =
      preserves_stable_prefix_before_first_edit(attempt);
  const bool startsWithUnstablePrefixDelete =
      starts_with_unstable_prefix_delete(attempt);
  const bool startsWithStableBoundaryDeleteWithoutContinuation =
      starts_with_stable_boundary_delete_without_continuation(attempt);
  const bool editsContinueAfterFirstEdit =
      attempt.recoveryEdits.empty() || continues_after_first_edit(attempt);
  if (attempt.editCost > attempt.configuredMaxEditCost &&
      !preservesStablePrefixBeforeFirstEdit &&
      !allows_local_gap_recovery_without_stable_prefix(attempt) &&
      !allows_input_prefix_delete_recovery_without_stable_prefix(attempt)) {
    attempt.status =
        attempt.recoveryEdits.empty() ? StrictFailure : RecoveredButNotCredible;
    return;
  }

  if (requires_narrowed_window_retry(attempt, false)) {
    attempt.status =
        attempt.recoveryEdits.empty() ? StrictFailure : RecoveredButNotCredible;
    return;
  }

  if (attempt.fullMatch || attempt.stableAfterRecovery) {
    if (startsWithStableBoundaryDeleteWithoutContinuation) {
      attempt.status = RecoveredButNotCredible;
      return;
    }
    attempt.status = Stable;
  } else if (attempt.reachedRecoveryTarget && !startsWithUnstablePrefixDelete &&
             editsContinueAfterFirstEdit) {
    attempt.status = Credible;
  } else if (attempt.completedRecoveryWindows > 0 ||
             !attempt.recoveryEdits.empty()) {
    attempt.status = RecoveredButNotCredible;
  } else {
    attempt.status = StrictFailure;
  }
}

void score_recovery_attempt(RecoveryAttempt &attempt) noexcept {
  using enum ParseDiagnosticKind;
  using enum RecoveryAttemptStatus;
  attempt.editTrace.entryCount = attempt.recoveryEdits.size();
  attempt.editTrace.editCount = attempt.editCount;
  attempt.editTrace.editCost = attempt.editCost;
  for (const auto &entry : attempt.recoveryEdits) {
    switch (entry.kind) {
    case Inserted:
      ++attempt.editTrace.insertCount;
      ++attempt.editTrace.tokenInsertCount;
      break;
    case Deleted:
      ++attempt.editTrace.deleteCount;
      ++attempt.editTrace.codepointDeleteCount;
      break;
    case Replaced:
      ++attempt.editTrace.replaceCount;
      break;
    case Incomplete:
    case Recovered:
    case ConversionError:
      break;
    }
  }
  if (!attempt.recoveryEdits.empty()) {
    attempt.editTrace.hasEdits = true;
    attempt.editTrace.firstEditOffset =
        std::numeric_limits<TextOffset>::max();
    attempt.editTrace.lastEditOffset = 0;
    for (const auto &entry : attempt.recoveryEdits) {
      attempt.editTrace.firstEditOffset =
          std::min(attempt.editTrace.firstEditOffset, entry.beginOffset);
      attempt.editTrace.lastEditOffset =
          std::max(attempt.editTrace.lastEditOffset, entry.endOffset);
    }
    attempt.editTrace.editSpan =
        attempt.editTrace.lastEditOffset - attempt.editTrace.firstEditOffset;
  }

  attempt.score = {
      .selection =
          {
              .entryRuleMatched = attempt.entryRuleMatched,
              .stable = attempt.status == Stable,
              .credible =
                  attempt.status == Credible || attempt.status == Stable,
              .fullMatch = attempt.fullMatch,
          },
      .edits =
          {
              .editCost = attempt.editCost,
              .editSpan = attempt.editTrace.editSpan,
              .entryCount =
                  static_cast<std::uint32_t>(attempt.editTrace.entryCount),
              .firstEditOffset = attempt.editTrace.firstEditOffset,
          },
      .progress =
          {
              .parsedLength = attempt.parsedLength,
              .maxCursorOffset = attempt.maxCursorOffset,
          },
  };
}

bool is_selectable_recovery_attempt(const RecoveryAttempt &attempt) noexcept {
  using enum RecoveryAttemptStatus;
  return (attempt.status == Credible || attempt.status == Stable) &&
         !is_trimmed_tail_fallback_attempt(attempt);
}

[[nodiscard]] auto build_non_credible_replay_contract(
    const RecoveryAttempt &attempt) noexcept
    -> std::optional<NonCredibleReplayContract> {
  if (!attempt.entryRuleMatched ||
      attempt.status != RecoveryAttemptStatus::RecoveredButNotCredible ||
      attempt.recoveryEdits.empty()) {
    return std::nullopt;
  }

  if (preserves_stable_prefix_before_first_edit(attempt)) {
    return NonCredibleReplayContract{
        .resumeFloor = attempt.stablePrefixOffset,
        .preservedEditCount = 0u,
    };
  }

  const bool establishesLocalInsertedFallback =
      attempt.recoveryEdits.size() == 1u &&
      has_inserted_recovery(attempt) &&
      continues_after_first_edit(attempt) &&
      has_visible_progress_past_last_edit(attempt);
  if (establishesLocalInsertedFallback) {
    return NonCredibleReplayContract{
        .resumeFloor = first_edit_offset(attempt),
        .preservedEditCount = 0u,
    };
  }

  if (!allows_local_delete_gap_recovery_without_stable_prefix(attempt)) {
    return std::nullopt;
  }

  return NonCredibleReplayContract{
      .resumeFloor = first_edit_offset(attempt),
      .preservedEditCount = 0u,
  };
}

namespace {

[[nodiscard]] bool preserves_committed_replay_prefix(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt,
    NonCredibleReplayContract &contract) noexcept {
  if (!has_committed_replay_prefix(selectedAttempt)) {
    return true;
  }

  contract.resumeFloor =
      std::max(selectedAttempt.parsedLength, last_edit_offset(selectedAttempt));
  if (selectedAttempt.failureSnapshot.has_value()) {
    const auto &failureSnapshot = *selectedAttempt.failureSnapshot;
    if (failureSnapshot.hasFailureToken &&
        failureSnapshot.failureTokenIndex <
            failureSnapshot.failureLeafHistory.size()) {
      contract.resumeFloor =
          std::min(contract.resumeFloor,
                   failureSnapshot
                       .failureLeafHistory[failureSnapshot.failureTokenIndex]
                       .beginOffset);
    }
  }

  while (contract.preservedEditCount < selectedAttempt.recoveryEdits.size() &&
         selectedAttempt.recoveryEdits[contract.preservedEditCount].endOffset <
             contract.resumeFloor) {
    ++contract.preservedEditCount;
  }
  if (candidate.recoveryEdits.size() < contract.preservedEditCount) {
    return false;
  }
  return std::equal(selectedAttempt.recoveryEdits.begin(),
                    selectedAttempt.recoveryEdits.begin() +
                        contract.preservedEditCount,
                    candidate.recoveryEdits.begin(),
                    same_syntax_script_entry);
}

[[nodiscard]] bool rewrites_committed_replay_boundary(
    const RecoveryAttempt &candidate,
    const NonCredibleReplayContract &contract) noexcept {
  if (contract.preservedEditCount >= candidate.recoveryEdits.size()) {
    return false;
  }

  const auto firstNewEditOffset =
      candidate.recoveryEdits[contract.preservedEditCount].beginOffset;
  if (firstNewEditOffset < contract.resumeFloor) {
    return true;
  }

  if (contract.preservedEditCount == 0u && !candidate.hasStablePrefix) {
    return false;
  }

  const auto newEdits =
      std::span(candidate.recoveryEdits).subspan(contract.preservedEditCount);
  return std::ranges::any_of(
      newEdits, [&](const SyntaxScriptEntry &entry) noexcept {
        return entry.beginOffset == contract.resumeFloor &&
               entry.kind != ParseDiagnosticKind::Inserted;
      });
}

} // namespace

bool satisfies_non_credible_fallback_contract(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  if (is_trimmed_tail_fallback_attempt(candidate)) {
    NonCredibleReplayContract committedReplayContract;
    return preserves_committed_replay_prefix(candidate, selectedAttempt,
                                             committedReplayContract) &&
           !rewrites_committed_replay_boundary(candidate,
                                              committedReplayContract);
  }

  auto contract = build_non_credible_replay_contract(candidate);
  if (!contract.has_value() ||
      !preserves_committed_replay_prefix(candidate, selectedAttempt,
                                         *contract) ||
      rewrites_committed_replay_boundary(candidate, *contract)) {
    return false;
  }

  if (!has_cursor_progress_past_last_edit(candidate) ||
      candidate.maxCursorOffset <= contract->resumeFloor) {
    return false;
  }

  if (contract->preservedEditCount == 0u) {
    if (!candidate.hasStablePrefix &&
        !has_visible_progress_past_last_edit(candidate)) {
      return false;
    }
    return !has_delete_only_recovery(candidate) ||
           continues_after_first_edit(candidate);
  }

  return candidate.parsedLength > selectedAttempt.parsedLength ||
         candidate.maxCursorOffset > selectedAttempt.maxCursorOffset;
}

bool is_better_recovery_attempt(const RecoveryAttempt &lhs,
                                const RecoveryAttempt &rhs) noexcept {
  return is_better_normalized_recovery_order_key(
      recovery_attempt_order_key(lhs.score), recovery_attempt_order_key(rhs.score),
      RecoveryOrderProfile::Attempt);
}

} // namespace pegium::parser::detail
