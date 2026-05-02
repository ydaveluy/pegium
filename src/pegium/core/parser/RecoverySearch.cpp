#include <pegium/core/parser/RecoverySearch.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>

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

namespace {

[[nodiscard]] bool
recovery_attempt_establishes_replay_contract(
    const RecoveryAttempt &attempt) noexcept {
  return attempt.fullMatch || attempt.stableAfterRecovery ||
         attempt.reachedRecoveryTarget;
}

} // namespace

AttemptFacts
derive_attempt_facts(const RecoveryAttempt &attempt) noexcept {
  AttemptFacts facts;
  facts.hasEdits = !attempt.recoveryEdits.empty();
  if (!facts.hasEdits) {
    facts.firstEditOffset = attempt.noEditFirstEditOffset;
    return facts;
  }

  // Pass 1 — aggregate per-edit data.
  facts.hasDeleteOnlyRecovery = true;
  bool firstEditIsDeleteAtZero = false;
  bool firstEditIsInsertAtZero = false;
  bool destructiveEditAfterRootInsert = false;
  bool anyDeleteAtStablePrefix = false;
  for (std::size_t index = 0; index < attempt.recoveryEdits.size(); ++index) {
    const auto &entry = attempt.recoveryEdits[index];
    facts.firstEditOffset =
        std::min(facts.firstEditOffset, entry.beginOffset);
    facts.lastEditOffset =
        std::max(facts.lastEditOffset, entry.endOffset);
    if (entry.kind == ParseDiagnosticKind::Inserted) {
      facts.hasInsertedRecovery = true;
    }
    if (entry.kind != ParseDiagnosticKind::Deleted) {
      facts.hasDeleteOnlyRecovery = false;
    }
    if (entry.kind == ParseDiagnosticKind::Deleted && entry.offset == 0) {
      firstEditIsDeleteAtZero = true;
    }
    if (index == 0u && entry.kind == ParseDiagnosticKind::Inserted &&
        entry.beginOffset == 0 && entry.endOffset == 0) {
      firstEditIsInsertAtZero = true;
    }
    const bool entryIsDestructive =
        entry.kind == ParseDiagnosticKind::Deleted ||
        entry.kind == ParseDiagnosticKind::Replaced;
    if (firstEditIsInsertAtZero && index > 0u && entryIsDestructive &&
        entry.beginOffset > 0) {
      destructiveEditAfterRootInsert = true;
    }
    if (attempt.hasStablePrefix &&
        entry.kind == ParseDiagnosticKind::Deleted &&
        entry.beginOffset == attempt.stablePrefixOffset) {
      anyDeleteAtStablePrefix = true;
    }
  }

  // Pass 2 — derive composite booleans from aggregates + attempt fields.
  facts.continuesAfterFirstEdit =
      attempt.parsedLength > facts.lastEditOffset;
  facts.hasCursorProgressPastLastEdit =
      attempt.maxCursorOffset > facts.lastEditOffset;
  facts.hasVisibleProgressPastLastEdit =
      attempt.lastVisibleCursorOffset > facts.lastEditOffset;
  facts.hasCursorProgressBetweenOrPastEdits =
      facts.hasCursorProgressPastLastEdit ||
      (attempt.recoveryEdits.size() > 1u &&
       attempt.lastVisibleCursorOffset > facts.firstEditOffset &&
       facts.lastEditOffset <= attempt.lastVisibleCursorOffset);

  facts.preservesStablePrefixBeforeFirstEdit =
      attempt.hasStablePrefix &&
      facts.firstEditOffset != std::numeric_limits<TextOffset>::max() &&
      facts.firstEditOffset >= attempt.stablePrefixOffset;

  facts.startsWithUnstablePrefixDelete =
      !attempt.hasStablePrefix && facts.firstEditOffset == 0 &&
      firstEditIsDeleteAtZero;
  facts.startsWithUnstablePrefixInsertThenDestructiveSuffixEdit =
      !attempt.hasStablePrefix && firstEditIsInsertAtZero &&
      destructiveEditAfterRootInsert;

  facts.startsWithStableBoundaryDeleteWithoutContinuation =
      attempt.hasStablePrefix &&
      facts.firstEditOffset == attempt.stablePrefixOffset &&
      !facts.hasDeleteOnlyRecovery &&
      !facts.continuesAfterFirstEdit &&
      anyDeleteAtStablePrefix;

  facts.hasEditPastReplayWindowHorizon =
      attempt.replayWindow.has_value() &&
      facts.lastEditOffset > attempt.replayWindow->maxCursorOffset;

  facts.hasCommittedReplayPrefix =
      recovery_attempt_establishes_replay_contract(attempt) ||
      facts.continuesAfterFirstEdit;

  const bool singleInsertAtNonZero =
      attempt.recoveryEdits.size() == 1u &&
      attempt.recoveryEdits.front().kind == ParseDiagnosticKind::Inserted &&
      attempt.recoveryEdits.front().beginOffset ==
          attempt.recoveryEdits.front().endOffset &&
      facts.firstEditOffset != 0;
  facts.allowsLocalGapRecoveryWithoutStablePrefix =
      !attempt.hasStablePrefix && !attempt.fullMatch &&
      !attempt.stableAfterRecovery && attempt.reachedRecoveryTarget &&
      singleInsertAtNonZero && facts.continuesAfterFirstEdit &&
      facts.hasVisibleProgressPastLastEdit &&
      facts.hasCursorProgressPastLastEdit;

  facts.allowsLocalDeleteGapRecoveryWithoutStablePrefix =
      !attempt.hasStablePrefix && !attempt.fullMatch &&
      facts.firstEditOffset != 0 && facts.hasDeleteOnlyRecovery &&
      facts.continuesAfterFirstEdit &&
      facts.hasVisibleProgressPastLastEdit &&
      facts.hasCursorProgressPastLastEdit;

  return facts;
}

#if defined(PEGIUM_ENABLE_RECOVERY_TRACE)
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
#else
// With recovery tracing disabled, these helpers are unused. Keeping them
// behind the macro avoids building JsonValue payloads (and pulling
// `RecoveryDebug.cpp`'s formatter into the hot path) when tracing is off,
// which is the case for release and the standard test build.
inline void trace_recovery_json(const char *, const pegium::JsonValue &) noexcept {}
inline void trace_strict_summary(const StrictParseSummary &) noexcept {}
inline void trace_failure_snapshot(const FailureSnapshot &) noexcept {}
inline void trace_recovery_window(std::uint32_t, std::uint32_t,
                                  const RecoveryWindow &,
                                  std::size_t) noexcept {}
inline void trace_recovery_attempt(const RecoveryAttempt &,
                                   const RecoveryAttemptSpec &) noexcept {}
#endif

struct RecoveryWindowPrefixContract {
  TextOffset editFloorOffset = 0;
  TextOffset stablePrefixOffset = 0;
  bool hasStablePrefix = false;
};

struct CommittedPrefixContract {
  TextOffset resumeFloor = 0;
  TextOffset boundaryFloor = 0;
  std::size_t preservedEditCount = 0u;
};

struct StrictFailureStageResult {
  RecoveryAttempt strictAttempt;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset failureVisibleCursorOffset = 0;
};

struct WindowRecoverySearchResult {
  std::optional<RecoveryAttempt> attempt;
  std::optional<FailureSnapshot> relocalizedFailure;
  bool isFallback = false;
};

namespace {

void consider_window_attempt_candidate(
    WindowRecoverySearchResult &result, RecoveryAttempt attempt,
    const RecoveryAttempt &selectedAttempt, bool continuingRecovery);

[[nodiscard]] CommittedPrefixContract
committed_replay_prefix_contract(
    const RecoveryAttempt &selectedAttempt) noexcept;

[[nodiscard]] bool localizes_current_failure_site(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept;

[[nodiscard]] constexpr bool
same_syntax_script_entry(const SyntaxScriptEntry &lhs,
                         const SyntaxScriptEntry &rhs) noexcept;

[[nodiscard]] bool
deleted_source_starts_with_inserted_literal(const SyntaxScriptEntry &inserted,
                                            const SyntaxScriptEntry &deleted,
                                            std::string_view source) noexcept {
  if (inserted.element == nullptr ||
      inserted.element->getKind() != grammar::ElementKind::Literal) {
    return false;
  }
  const auto &literal =
      static_cast<const grammar::Literal &>(*inserted.element);
  const auto value = literal.getValue();
  if (value.empty() || deleted.beginOffset > source.size() ||
      deleted.endOffset > source.size() ||
      deleted.endOffset - deleted.beginOffset < value.size()) {
    return false;
  }
  return source.substr(deleted.beginOffset, value.size()) == value;
}

[[nodiscard]] bool
plausible_insert_delete_compensation_pair(const SyntaxScriptEntry &lhs,
                                          const SyntaxScriptEntry &rhs,
                                          std::string_view source) noexcept {
  const bool lhsInserted = lhs.kind == ParseDiagnosticKind::Inserted;
  const bool rhsInserted = rhs.kind == ParseDiagnosticKind::Inserted;
  const bool lhsDeleted = lhs.kind == ParseDiagnosticKind::Deleted;
  const bool rhsDeleted = rhs.kind == ParseDiagnosticKind::Deleted;
  if (!((lhsInserted && rhsDeleted) || (rhsInserted && lhsDeleted))) {
    return false;
  }

  const auto &inserted = lhsInserted ? lhs : rhs;
  const auto &deleted = lhsDeleted ? lhs : rhs;
  return inserted.beginOffset == inserted.endOffset &&
         deleted.beginOffset < deleted.endOffset &&
         inserted.beginOffset <= deleted.beginOffset &&
         (elements_equivalent_for_replay(inserted.element, deleted.element) ||
          deleted_source_starts_with_inserted_literal(inserted, deleted,
                                                      source));
}

[[nodiscard]] std::size_t
replay_horizon_preserved_edit_count(const RecoveryAttempt &attempt) noexcept {
  if (!attempt.replayWindow.has_value() ||
      !attempt.facts.hasEditPastReplayWindowHorizon) {
    return attempt.recoveryEdits.size();
  }
  // Preserve every edit the parser actually parsed past. The original
  // window's `maxCursorOffset` is the strict-failure anchor; once the
  // recovery committed and the parser advanced through these edits to
  // `parsedLength`, they are not speculative anymore. Dropping them would
  // force the next window to rediscover them, but its editFloorOffset
  // typically blocks edits below the previous window's anchor — leaving
  // the cascade with no valid edit position and stranding the parse.
  const auto horizon =
      std::max(attempt.parsedLength, attempt.facts.lastEditOffset);
  std::size_t count = 0u;
  while (count < attempt.recoveryEdits.size()) {
    const auto &entry = attempt.recoveryEdits[count];
    if (entry.beginOffset > horizon || entry.endOffset > horizon) {
      break;
    }
    ++count;
  }
  return count;
}

[[nodiscard]] TextOffset
replay_horizon_resume_floor(const RecoveryAttempt &attempt,
                            std::size_t preservedEditCount) noexcept {
  if (preservedEditCount < attempt.recoveryEdits.size()) {
    return attempt.recoveryEdits[preservedEditCount].beginOffset;
  }
  return std::max(attempt.parsedLength, attempt.facts.lastEditOffset);
}

[[nodiscard]] TextOffset
visible_leaf_stable_prefix_offset(const FailureSnapshot &snapshot) noexcept {
  const auto stableLeafEnd =
      snapshot.hasFailureToken &&
              snapshot.failureTokenIndex < snapshot.failureLeafHistory.size()
          ? snapshot.failureTokenIndex
          : snapshot.failureLeafHistory.size();
  TextOffset stablePrefixOffset = 0;
  for (std::size_t index = 0; index < stableLeafEnd; ++index) {
    const auto &leaf = snapshot.failureLeafHistory[index];
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
  if (selectedAttempt.status ==
          RecoveryAttemptStatus::RecoveredButNotCredible &&
      selectedAttempt.facts.hasEditPastReplayWindowHorizon) {
    if (selectedAttempt.stableAfterRecovery &&
        selectedAttempt.facts.hasVisibleProgressPastLastEdit) {
      const auto boundaryFloor = std::max(selectedAttempt.parsedLength,
                                          selectedAttempt.facts.lastEditOffset);
      return {.editFloorOffset = boundaryFloor,
              .stablePrefixOffset = boundaryFloor,
              .hasStablePrefix = true};
    }
    const auto preservedEditCount =
        replay_horizon_preserved_edit_count(selectedAttempt);
    if (preservedEditCount < selectedAttempt.recoveryEdits.size()) {
      const auto resumeFloor =
          replay_horizon_resume_floor(selectedAttempt, preservedEditCount);
      return {.editFloorOffset = resumeFloor,
              .stablePrefixOffset = resumeFloor,
              .hasStablePrefix = true};
    }
  }

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
  if (!selectedAttempt.recoveryEdits.empty() &&
      selectedAttempt.parsedLength != 0) {
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

void fill_strict_attempt_from_summary(RecoveryAttempt &attempt,
                                      const StrictParseSummary &summary,
                                      TextOffset inputSize) noexcept {
  attempt.entryRuleMatched = summary.entryRuleMatched;
  attempt.parsedLength = summary.parsedLength;
  attempt.lastVisibleCursorOffset = summary.lastVisibleCursorOffset;
  attempt.fullMatch = summary.fullMatch;
  attempt.maxCursorOffset = summary.maxCursorOffset;
  attempt.noEditFirstEditOffset = inputSize;
}

[[nodiscard]] StrictFailureStageResult
parse_strict_capturing_failure(const grammar::ParserRule &entryRule,
                         const Skipper &skipper, const ParseOptions &options,
                         const text::TextSnapshot &text,
                         const utils::CancellationToken &cancelToken) {
  StrictFailureStageResult result;
  const auto inputSize = static_cast<TextOffset>(text.size());

  // Fast path: run strict parse without failure history.
  //
  // On fully-valid inputs this is the only pass we need; the failure history
  // recorder is only required when recovery has to reconstruct where the
  // parse derailed. Paying its per-leaf bookkeeping on every leaf of every
  // successful parse is wasteful, so we first try a bare `ParseContext` and
  // only re-run with a `TrackedParseContext` if the strict parse did not
  // reach `fullMatch`.
  const StrictFailureEngine strictFailureEngine;
  auto strictResult =
      strictFailureEngine.runStrictParse(entryRule, skipper, text, cancelToken);
  result.strictAttempt.cst = std::move(strictResult.cst);
  fill_strict_attempt_from_summary(result.strictAttempt, strictResult.summary,
                                   inputSize);
  result.failureVisibleCursorOffset =
      result.strictAttempt.lastVisibleCursorOffset;

  if (!options.recoveryEnabled || result.strictAttempt.fullMatch) {
    return result;
  }

  // Slow path: re-run with failure history to build the recovery snapshot.
  auto analysis =
      run_strict_parse_with_failure_snapshot(entryRule, skipper, text, cancelToken);
  result.strictAttempt.cst = std::move(analysis.strictResult.cst);
  const auto &summary = analysis.strictResult.summary;
  fill_strict_attempt_from_summary(result.strictAttempt, summary, inputSize);
  result.failureVisibleCursorOffset =
      result.strictAttempt.lastVisibleCursorOffset;

  if (result.strictAttempt.fullMatch) {
    return result;
  }

  trace_strict_summary(summary);
  result.failureVisibleCursorOffset = failure_visible_cursor_offset(
      analysis.snapshot, result.failureVisibleCursorOffset);
  trace_failure_snapshot(analysis.snapshot);
  result.failureSnapshot = std::move(analysis.snapshot);
  return result;
}

[[nodiscard]] bool
recovery_attempt_budget_exhausted(const RecoverySearchRunResult &result,
                                  const ParseOptions &options) noexcept {
  return result.budgetedRecoveryAttemptRuns >= options.maxRecoveryAttempts ||
         result.recoveryAttemptRuns >= options.maxTotalRecoveryAttemptRuns;
}

[[nodiscard]] TextOffset
last_failure_leaf_end_offset(const FailureSnapshot &snapshot) noexcept {
  return snapshot.failureLeafHistory.empty()
             ? 0
             : snapshot.failureLeafHistory.back().endOffset;
}

/// Phase F/B — Edit pruning post-hoc.
///
/// Greedy exploration inside a single recovery attempt can commit to more
/// edits than strictly needed. For example, inserting the leading `(` of a
/// `FunctionCall`'s `option("(" Args ")")` opens an opportunistic path that
/// then needs 4 more edits to fabricate the call; the same input often has
/// a 1-edit `delete` that also reaches `fullMatch` but is not surfaced as a
/// separate attempt.
///
/// After a `fullMatch` attempt, drop each edit in turn and re-run with the
/// remaining edits pinned as the committed prefix. If the parse still
/// reaches `fullMatch` with a strictly smaller edit count, keep the
/// smaller set. Iterate until stable. O(N²) per accepted attempt in the
/// worst case, bounded by `maxRecoveryEditsPerAttempt`.
///
/// Pruning probes are post-hoc and do not count against
/// `recoveryAttemptRuns` or `budgetedRecoveryAttemptRuns`: they fire only
/// after an attempt already reached `fullMatch`, i.e. the global budget
/// was already paid to obtain that candidate. Adding them to the counter
/// would defeat tests that cap the budget at 1 attempt.
[[nodiscard]] RecoveryAttempt
minimize_recovery_edits(const grammar::ParserRule &entryRule,
                        const Skipper &skipper, const ParseOptions &options,
                        const text::TextSnapshot &text,
                        const RecoveryAttemptSpec &spec,
                        RecoveryAttempt attempt,
                        std::uint64_t &choiceRecoverCacheHits,
                        std::uint64_t &choiceRecoverCacheMisses,
                        const utils::CancellationToken &cancelToken) {
  if (!attempt.fullMatch || attempt.recoveryEdits.size() <= 1u) {
    return attempt;
  }
  // Keep post-hoc pruning a small-script compensation. Large edit sets usually
  // represent panic/resync or repeated local repairs; replaying the whole file
  // once per edit turns recovery into an O(N^2) global search and duplicates
  // work the local enumerators should own.
  constexpr std::size_t kMaxPostHocPruningEditCount = 4u;
  if (attempt.recoveryEdits.size() > kMaxPostHocPruningEditCount) {
    return attempt;
  }
  PEGIUM_STEP_TRACE_INC(StepCounter::MinimizeRecoveryEditsRuns);
  const auto editCountBeforePruning = attempt.recoveryEdits.size();

  // Keep pruning until no edit can be dropped while preserving a full match.
  // Each outer iteration restarts the scan from the first edit because dropping
  // one edit can unlock another whose necessity depended on the earlier one.
  // Pair pruning is intentionally narrow and reserved for small local scripts:
  // only an inserted element followed by a delete of the same replay-equivalent
  // element is considered. That covers the common "insert early delimiter,
  // delete real later delimiter" compensation without turning pruning into a
  // second speculative recovery search.
  const auto tryPrunedEditSet =
      [&](auto shouldDrop) -> std::optional<RecoveryAttempt> {
    RecoveryAttemptSpec prunedSpec;
    prunedSpec.window = spec.window;
    prunedSpec.committedRecoveryResumeFloor = spec.committedRecoveryResumeFloor;
    prunedSpec.committedRecoveryEdits.reserve(attempt.recoveryEdits.size());
    for (std::size_t i = 0; i < attempt.recoveryEdits.size(); ++i) {
      if (!shouldDrop(i)) {
        prunedSpec.committedRecoveryEdits.push_back(attempt.recoveryEdits[i]);
      }
    }
    if (prunedSpec.committedRecoveryEdits.size() >=
        attempt.recoveryEdits.size()) {
      return std::nullopt;
    }

    // Run the probe with recovery OFF. Committed edits in the spec are
    // still replayed; only *new* edits are forbidden. If the parse still
    // reaches `fullMatch`, the dropped edit was genuinely redundant. If
    // we left recovery on, the parser would just re-discover the dropped
    // edit and return an equivalent set.
    ParseOptions probeOptions = options;
    probeOptions.recoveryEnabled = false;
    probeOptions.maxRecoveryEditsPerAttempt =
        static_cast<std::uint32_t>(prunedSpec.committedRecoveryEdits.size());
    auto prunedAttempt = execute_recovery_parse(
        entryRule, skipper, probeOptions, text, prunedSpec, cancelToken);
    classify_recovery_attempt(prunedAttempt);
    choiceRecoverCacheHits += prunedAttempt.choiceRecoverCacheHits;
    choiceRecoverCacheMisses += prunedAttempt.choiceRecoverCacheMisses;

    const bool replayedOnlyCommittedEdits =
        prunedAttempt.recoveryEdits.size() ==
            prunedSpec.committedRecoveryEdits.size() &&
        std::equal(prunedAttempt.recoveryEdits.begin(),
                   prunedAttempt.recoveryEdits.end(),
                   prunedSpec.committedRecoveryEdits.begin(),
                   same_syntax_script_entry);
    if (prunedAttempt.fullMatch && replayedOnlyCommittedEdits &&
        prunedAttempt.recoveryEdits.size() < attempt.recoveryEdits.size()) {
      return prunedAttempt;
    }
    return std::nullopt;
  };

  bool changed = true;
  while (changed && attempt.recoveryEdits.size() > 1u) {
    changed = false;
    for (std::size_t i = 0; i < attempt.recoveryEdits.size(); ++i) {
      if (auto prunedAttempt =
              tryPrunedEditSet([i](std::size_t candidateIndex) noexcept {
                return candidateIndex == i;
              })) {
        attempt = std::move(*prunedAttempt);
        changed = true;
        break;
      }
    }
    if (changed || attempt.recoveryEdits.size() > 4u) {
      continue;
    }
    for (std::size_t i = 0; i + 1u < attempt.recoveryEdits.size(); ++i) {
      for (std::size_t j = i + 1u; j < attempt.recoveryEdits.size(); ++j) {
        if (!plausible_insert_delete_compensation_pair(attempt.recoveryEdits[i],
                                                       attempt.recoveryEdits[j],
                                                       text.view())) {
          continue;
        }
        if (auto prunedAttempt =
                tryPrunedEditSet([i, j](std::size_t candidateIndex) noexcept {
                  return candidateIndex == i || candidateIndex == j;
                })) {
          attempt = std::move(*prunedAttempt);
          changed = true;
          break;
        }
      }
      if (changed) {
        break;
      }
    }
  }

  if (attempt.recoveryEdits.size() < editCountBeforePruning) {
    PEGIUM_STEP_TRACE_INC(
        StepCounter::MinimizeRecoveryEditsDropped,
        editCountBeforePruning - attempt.recoveryEdits.size());
  }
  return attempt;
}

[[nodiscard]] WindowRecoverySearchResult try_recovery_window(
    const grammar::ParserRule &entryRule, const Skipper &skipper,
    const ParseOptions &options, const text::TextSnapshot &text,
    const RecoveryAttemptSpec &spec, const RecoveryAttempt &selectedAttempt,
    bool continuingRecovery,
    std::uint32_t &recoveryAttemptRuns,
    std::uint32_t &budgetedRecoveryAttemptRuns,
    std::uint64_t &choiceRecoverCacheHits,
    std::uint64_t &choiceRecoverCacheMisses,
    const utils::CancellationToken &cancelToken) {
  WindowRecoverySearchResult result;
  ++recoveryAttemptRuns;
  ++budgetedRecoveryAttemptRuns;
  PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
  auto primaryAttempt = execute_recovery_parse(entryRule, skipper, options, text,
                                             spec, cancelToken);
  classify_recovery_attempt(primaryAttempt);
  trace_recovery_attempt(primaryAttempt, spec);
  choiceRecoverCacheHits += primaryAttempt.choiceRecoverCacheHits;
  choiceRecoverCacheMisses += primaryAttempt.choiceRecoverCacheMisses;

  // Phase F/B — minimize the edit set before the ranker sees the attempt.
  primaryAttempt = minimize_recovery_edits(
      entryRule, skipper, options, text, spec, std::move(primaryAttempt),
      choiceRecoverCacheHits, choiceRecoverCacheMisses, cancelToken);

  // Phase F/A — minimal-edit strategy probe. Complements F/D (option
  // insert gate) and the InfixRule rhs-region constraint: those remove
  // enumeration-time speculation, but edits accumulated via a rebound
  // recovery that has already crossed an earlier committed edit can
  // still produce multi-edit paths. Running a second attempt with
  // `maxRecoveryEditsPerAttempt = 1` exposes a single-edit candidate
  // to the shared `RecoveryKey` ranker. Not counted against the
  // attempt budget: the probe only reinterprets the same window with a
  // tighter cap after the greedy attempt already paid the budget.
  //
  // The probe runs in two cases:
  //   - primary `fullMatch` with multiple edits: the original case,
  //     where the greedy path may have over-spent edits;
  //   - primary `RecoveredButNotCredible` with multiple edits: a
  //     greedy non-credible path may obscure a single-edit candidate
  //     that would actually reach `fullMatch` (e.g. a fuzzy keyword
  //     repair like `extend -> extends` whose 1-cost Replace is
  //     hidden behind a 3-insert "skip option + fabricate body"
  //     primary that fails downstream and surfaces as non-credible).
  const bool minimalProbeWorthRunning =
      (primaryAttempt.fullMatch ||
       primaryAttempt.status ==
           RecoveryAttemptStatus::RecoveredButNotCredible) &&
      primaryAttempt.recoveryEdits.size() > 1u &&
      options.maxRecoveryEditsPerAttempt > 1u;
  if (minimalProbeWorthRunning) {
    PEGIUM_STEP_TRACE_INC(StepCounter::MinimalEditProbeRuns);
    ParseOptions minimalOptions = options;
    // The committed prefix replay consumes from the same edit budget,
    // so cap at "committed count + 1 new edit" rather than just 1.
    // Without this, a window with N committed edits cannot add any
    // new edit at all, defeating the purpose of the probe in
    // continuing-recovery contexts.
    minimalOptions.maxRecoveryEditsPerAttempt =
        static_cast<std::uint32_t>(spec.committedRecoveryEdits.size()) + 1u;
    PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
    auto minimalAttempt = execute_recovery_parse(entryRule, skipper,
                                               minimalOptions, text, spec,
                                               cancelToken);
    classify_recovery_attempt(minimalAttempt);
    trace_recovery_attempt(minimalAttempt, spec);
    choiceRecoverCacheHits += minimalAttempt.choiceRecoverCacheHits;
    choiceRecoverCacheMisses += minimalAttempt.choiceRecoverCacheMisses;
    if (minimalAttempt.fullMatch &&
        is_better_recovery_attempt(minimalAttempt, primaryAttempt)) {
      PEGIUM_STEP_TRACE_INC(StepCounter::MinimalEditProbeWins);
      primaryAttempt = std::move(minimalAttempt);
    }
  }

  // Phase F/A2 — alternative-strategy probes. When the primary attempt
  // settles for `RecoveredButNotCredible` because its greedy local
  // recovery picked the wrong cascade (consumed past a missing closing
  // delimiter, then resorted to delete-scan), alternative attempts
  // with different edit-budget configurations can let an Insert-only
  // path reach `fullMatch`. The probes stay cheap: they only run when
  // the primary is incomplete and they reuse the same window. Like the
  // minimal probe, they are intentionally NOT counted against the
  // attempt budget.
  // Run alternative-strategy probes for any non-fullMatch primary that
  // wasn't classified as a clean Stable. Specifically, also probe for
  // `StrictFailure` outcomes — these arise when the dispatcher's first
  // pass through a continuing-recovery window cannot place a single
  // edit cleanly inside the editFloor and the original edit budget is
  // not enough to cascade through the structure. The widened budget /
  // relaxed stability probes give the dispatcher a chance to find a
  // credible cascade in this scenario.
  const bool alternativeProbeWorthRunning =
      !primaryAttempt.fullMatch &&
      (primaryAttempt.status ==
           RecoveryAttemptStatus::RecoveredButNotCredible ||
       primaryAttempt.status == RecoveryAttemptStatus::StrictFailure);
  if (alternativeProbeWorthRunning) {
    // Each probe re-runs the recovery with a different policy axis relaxed,
    // looking for a fullMatch path the primary missed. Probes are ordered
    // cheap → expensive; the first one that reaches fullMatch short-circuits
    // the rest via the early `primaryAttempt.fullMatch` guard.
    const auto runProbe = [&](ParseOptions probeOptions) {
      if (primaryAttempt.fullMatch) {
        return;
      }
      PEGIUM_STEP_TRACE_INC(StepCounter::RecoveryPhaseRuns);
      auto attempt = execute_recovery_parse(entryRule, skipper, probeOptions,
                                            text, spec, cancelToken);
      classify_recovery_attempt(attempt);
      trace_recovery_attempt(attempt, spec);
      choiceRecoverCacheHits += attempt.choiceRecoverCacheHits;
      choiceRecoverCacheMisses += attempt.choiceRecoverCacheMisses;
      if (attempt.fullMatch &&
          is_better_recovery_attempt(attempt, primaryAttempt)) {
        primaryAttempt = std::move(attempt);
      }
    };

    // Probe 1: forbid consecutive deletes. Targets cases where the primary's
    // delete-scan produced a non-credible "skip ahead" path while an
    // Insert-only repair would actually reach `fullMatch` (e.g. a missing
    // `}` whose Insert lands cleanly when the dispatcher cannot fall back to
    // deleting bytes).
    const bool primaryHasDeletes = std::ranges::any_of(
        primaryAttempt.recoveryEdits, [](const auto &edit) noexcept {
          return edit.kind == ParseDiagnosticKind::Deleted;
        });
    if (primaryHasDeletes && options.maxConsecutiveCodepointDeletes > 0u) {
      ParseOptions probe = options;
      probe.maxConsecutiveCodepointDeletes = 0u;
      runProbe(probe);
    }

    // Probe 2: 4x cost budget. Targets cascade cases where the primary
    // stopped short because the cumulative edit cost across multiple typoed
    // tokens exceeded the per-attempt budget. The wider budget lets a longer
    // Replace cascade reach `fullMatch` when each individual Replace is cheap
    // (cost 1) but there are many of them.
    if (options.maxRecoveryEditCost <
        std::numeric_limits<std::uint32_t>::max() / 4u) {
      ParseOptions probe = options;
      probe.maxRecoveryEditCost = options.maxRecoveryEditCost * 4u;
      runProbe(probe);
    }

    // Probe 3: full-width window. Targets cascade cases where the primary's
    // narrow initial window committed to a path that cannot reach the latest
    // typo before exhausting its forward stability budget.
    if (options.recoveryWindowTokenCount <
        options.maxRecoveryWindowTokenCount) {
      ParseOptions probe = options;
      probe.recoveryWindowTokenCount = options.maxRecoveryWindowTokenCount;
      runProbe(probe);
    }

    // Probe 4: relaxed stability requirement. Targets cases where the
    // primary's `RecoveredButNotCredible` verdict came from the strict
    // 2-token stability check rejecting a partial repair that could have
    // continued.
    if (options.recoveryStabilityTokenCount > 0u) {
      ParseOptions probe = options;
      probe.recoveryStabilityTokenCount = 0u;
      runProbe(probe);
    }

    // Probe 5: extended attempt budget combined with the previous relaxations.
    // Targets the deepest cascade explorations.
    if (options.maxRecoveryAttempts <
        std::numeric_limits<std::uint32_t>::max() / 4u) {
      ParseOptions probe = options;
      probe.maxRecoveryAttempts = options.maxRecoveryAttempts * 4u;
      probe.maxRecoveryEditCost = options.maxRecoveryEditCost * 4u;
      probe.recoveryStabilityTokenCount = 0u;
      probe.recoveryWindowTokenCount = options.maxRecoveryWindowTokenCount;
      runProbe(probe);
    }
  }

  consider_window_attempt_candidate(result, std::move(primaryAttempt),
                                    selectedAttempt, continuingRecovery);
  return result;
}

[[nodiscard]] bool preserves_committed_replay_prefix(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt,
    CommittedPrefixContract &contract) noexcept;

[[nodiscard]] bool rewrites_committed_replay_boundary(
    const RecoveryAttempt &candidate,
    const CommittedPrefixContract &contract) noexcept;

[[nodiscard]] bool extends_current_recovery_site(
    const RecoveryAttempt &candidate,
    const CommittedPrefixContract &contract) noexcept {
  if (candidate.recoveryEdits.size() <= contract.preservedEditCount) {
    return false;
  }
  return candidate.fullMatch || candidate.parsedLength > contract.resumeFloor ||
         candidate.maxCursorOffset > contract.resumeFloor;
}

/// Window-attempt classification used to drive `validate_window_attempt`.
///
/// The two paths the validator serves diverge in their ranking rules and
/// in which gates apply:
///   - `Selectable`: the candidate is `is_selectable_recovery_attempt`
///     (Credible or Stable). It may replace the current selected attempt
///     based on the central `is_better_recovery_attempt` ranking.
///   - `NonCredibleFallback`: the candidate is not selectable but the
///     entry rule matched. It may replace a non-fullMatch selected
///     attempt provided it strictly outranks an existing non-credible
///     fallback.
enum class WindowAttemptKind : std::uint8_t {
  Selectable,
  NonCredibleFallback,
};

/// Verdict returned by `validate_window_attempt`. `Accept` is the only
/// accepting value; every other value names the gate that rejected the
/// candidate. The named rejections allow tracing and diagnostics to
/// refer to a single source of truth instead of inferring the reason
/// from boolean control flow.
enum class WindowAttemptVerdict : std::uint8_t {
  Accept,
  RejectCommittedPrefixViolation,
  RejectBoundaryRewrite,
  RejectSelectedFullMatch,
  RejectNoSiteExtension,
  RejectNotBetterThanSelected,
};

/// Single decision predicate for window-attempt acceptance. Replaces
/// the previously-separate `qualifies_selectable_window_attempt` and
/// `qualifies_fallback_window_attempt` whose committed-prefix /
/// boundary-rewrite / continuation gates were duplicated. The two
/// paths now share the same gate sequence; only the final ranking
/// rule differs by `kind`.
[[nodiscard]] WindowAttemptVerdict validate_window_attempt(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt,
    bool continuingRecovery, WindowAttemptKind kind) noexcept {
  // Selectable escape clause: a continuing full match strictly better
  // than the current non-credible fallback is accepted unconditionally
  // (otherwise a late window that discovers a credible global parse
  // would be rejected on the sole ground that its edit list is shorter
  // than the tentatively-committed one).
  if (kind == WindowAttemptKind::Selectable) {
    const bool replacesNonCredibleFallbackWithFullMatch =
        continuingRecovery && candidate.fullMatch &&
        candidate.status != RecoveryAttemptStatus::RecoveredButNotCredible &&
        selectedAttempt.status ==
            RecoveryAttemptStatus::RecoveredButNotCredible &&
        !selectedAttempt.fullMatch &&
        is_better_recovery_attempt(candidate, selectedAttempt);
    if (replacesNonCredibleFallbackWithFullMatch) {
      return WindowAttemptVerdict::Accept;
    }
  }

  CommittedPrefixContract committedReplayContract;
  if (!preserves_committed_replay_prefix(candidate, selectedAttempt,
                                         committedReplayContract)) {
    return WindowAttemptVerdict::RejectCommittedPrefixViolation;
  }
  if (selectedAttempt.facts.hasCommittedReplayPrefix &&
      rewrites_committed_replay_boundary(candidate, committedReplayContract)) {
    return WindowAttemptVerdict::RejectBoundaryRewrite;
  }
  if (continuingRecovery && candidate.fullMatch && !selectedAttempt.fullMatch &&
      candidate.recoveryEdits.size() == selectedAttempt.recoveryEdits.size() &&
      std::equal(candidate.recoveryEdits.begin(), candidate.recoveryEdits.end(),
                 selectedAttempt.recoveryEdits.begin(),
                 same_syntax_script_entry)) {
    return WindowAttemptVerdict::Accept;
  }

  // Fallback-only veto: if the selected attempt already produced a full
  // match, no fallback can improve on it.
  if (kind == WindowAttemptKind::NonCredibleFallback &&
      selectedAttempt.fullMatch) {
    return WindowAttemptVerdict::RejectSelectedFullMatch;
  }

  // Continuation rule (shared): a candidate that does not extend the
  // current recovery site cannot accept under continuation.
  if (continuingRecovery) {
    return extends_current_recovery_site(candidate, committedReplayContract)
               ? WindowAttemptVerdict::Accept
               : WindowAttemptVerdict::RejectNoSiteExtension;
  }

  // Non-continuing ranking rules diverge by kind.
  if (kind == WindowAttemptKind::Selectable) {
    return is_better_recovery_attempt(candidate, selectedAttempt)
               ? WindowAttemptVerdict::Accept
               : WindowAttemptVerdict::RejectNotBetterThanSelected;
  }

  // Non-credible fallback, non-continuing: only beat a non-credible
  // selected when strictly better. The eligibility of `candidate` as a
  // non-credible fallback (`satisfies_non_credible_fallback_contract`)
  // is gated at the call site once and is not re-checked here.
  if (selectedAttempt.entryRuleMatched &&
      selectedAttempt.status == RecoveryAttemptStatus::RecoveredButNotCredible &&
      !is_better_recovery_attempt(candidate, selectedAttempt)) {
    return WindowAttemptVerdict::RejectNotBetterThanSelected;
  }
  return WindowAttemptVerdict::Accept;
}

void consider_window_attempt_candidate(
    WindowRecoverySearchResult &result, RecoveryAttempt attempt,
    const RecoveryAttempt &selectedAttempt, bool continuingRecovery) {
  if (is_selectable_recovery_attempt(attempt)) {
    const auto verdict =
        validate_window_attempt(attempt, selectedAttempt, continuingRecovery,
                                WindowAttemptKind::Selectable);
    if (verdict == WindowAttemptVerdict::Accept) {
      result.attempt = std::move(attempt);
      result.isFallback = false;
    }
    return;
  }
  if (attempt.entryRuleMatched &&
      (continuingRecovery ||
       satisfies_non_credible_fallback_contract(attempt, selectedAttempt))) {
    const auto verdict =
        validate_window_attempt(attempt, selectedAttempt, continuingRecovery,
                                WindowAttemptKind::NonCredibleFallback);
    if (verdict == WindowAttemptVerdict::Accept) {
      result.attempt = std::move(attempt);
      result.isFallback = true;
    } else if (continuingRecovery &&
               localizes_current_failure_site(attempt, selectedAttempt)) {
      result.relocalizedFailure = *attempt.failureSnapshot;
    }
    return;
  }
  PEGIUM_RECOVERY_TRACE("[parser attempt] rejected for selection status=",
                        recovery_attempt_status_name(attempt.status));
}

void apply_window_acceptance(RecoveryAttempt acceptedAttempt,
                             const RecoveryWindow &window,
                             RecoveryAttempt &selectedAttempt,
                             bool fallback) {
  selectedAttempt = std::move(acceptedAttempt);
#if defined(PEGIUM_ENABLE_RECOVERY_TRACE)
  PEGIUM_RECOVERY_TRACE(
      fallback ? "[parser window] accepted fallback begin="
               : "[parser window] accepted begin=",
      window.beginOffset, " max=", window.maxCursorOffset,
      " full=", selectedAttempt.fullMatch,
      " len=", selectedAttempt.parsedLength, " cost=", selectedAttempt.editCost,
      " status=", recovery_attempt_status_name(selectedAttempt.status));
  trace_recovery_json("[parser selected-attempt]",
                      recovery_attempt_to_json(selectedAttempt));
#else
  (void)window;
  (void)fallback;
#endif
}

[[nodiscard]] bool apply_qualified_window_acceptance(
    WindowRecoverySearchResult &windowSearch,
    const RecoveryWindow &window,
    RecoveryAttempt &selectedAttempt) {
  if (!windowSearch.attempt.has_value()) {
    return false;
  }
  apply_window_acceptance(std::move(*windowSearch.attempt), window,
                          selectedAttempt, windowSearch.isFallback);
  return true;
}

[[nodiscard]] constexpr bool same_syntax_script_entry(
    const SyntaxScriptEntry &lhs,
    const SyntaxScriptEntry &rhs) noexcept {
  return lhs.kind == rhs.kind && lhs.offset == rhs.offset &&
         lhs.beginOffset == rhs.beginOffset && lhs.endOffset == rhs.endOffset &&
         elements_equivalent_for_replay(lhs.element, rhs.element) &&
         lhs.message == rhs.message;
}

[[nodiscard]] bool
can_try_prefix_delete_retry(const grammar::ParserRule &entryRule,
                            const RecoveryAttemptSpec &spec) noexcept {
  // The grammar-shape predicate is in `RecoveryAnalysis` so the
  // orchestrator no longer carries 100+ lines of recursive grammar
  // introspection. The recovery-side condition (window starts at 0
  // and admits an edit at offset 0) stays here.
  return spec.window.beginOffset == 0 && spec.window.editFloorOffset == 0 &&
         entry_rule_has_unguarded_leading_visible_entry(entryRule);
}

[[nodiscard]] bool starts_with_root_insert_then_destructive_suffix_edit(
    std::span<const SyntaxScriptEntry> recoveryEdits) noexcept {
  if (recoveryEdits.empty()) {
    return false;
  }
  const auto &first = recoveryEdits.front();
  if (first.kind != ParseDiagnosticKind::Inserted || first.beginOffset != 0 ||
      first.endOffset != 0) {
    return false;
  }
  return std::ranges::any_of(recoveryEdits.begin() + 1, recoveryEdits.end(),
                             [](const SyntaxScriptEntry &entry) noexcept {
                               const bool destructive =
                                   entry.kind == ParseDiagnosticKind::Deleted ||
                                   entry.kind == ParseDiagnosticKind::Replaced;
                               return destructive && entry.beginOffset > 0;
                             });
}

[[nodiscard]] bool
try_prefix_delete_retry_entry_rule(RecoveryContext &ctx,
                                   const grammar::ParserRule &entryRule) {
  if (!ctx.canDelete() || ctx.cursor() != ctx.begin) {
    return false;
  }

  const bool savedAllowInsert = ctx.allowInsert;
  const bool previousSkipAfterDelete = ctx.skipAfterDelete;
  auto deleteOnlyGuard = ctx.withEditPermissions(false, true);
  (void)deleteOnlyGuard;
  return detail::recover_by_guarded_delete_retry(
      ctx,
      [](const detail::DeleteRetryVisitState &) noexcept { return true; },
      [&](const detail::DeleteRetryVisitState &) {
        const auto retryEditCountBefore = ctx.recoveryEditCount();
        const auto retryStartOffset = ctx.cursorOffset();
        const auto retryVisibleLeafCount = ctx.failureHistorySize();
        detail::ProbeRestoreScope guard{ctx};
        ctx.skip();
        ctx.skipAfterDelete = previousSkipAfterDelete;
        auto retryEditGuard = ctx.withEditPermissions(savedAllowInsert, false);
        (void)retryEditGuard;
        const bool matched = entryRule.recover(ctx);
        if (matched) {
          ctx.skip();
          const auto retryEdits = ctx.recoveryEditsView();
          bool retryInsertThenDestructiveSuffix = false;
          if (retryEdits.size() > retryEditCountBefore) {
            const auto &firstRetryEdit = retryEdits[retryEditCountBefore];
            const bool startsWithSyntheticInsert =
                firstRetryEdit.kind == ParseDiagnosticKind::Inserted &&
                firstRetryEdit.beginOffset == retryStartOffset &&
                firstRetryEdit.endOffset == retryStartOffset;
            if (startsWithSyntheticInsert) {
              retryInsertThenDestructiveSuffix = std::ranges::any_of(
                  retryEdits.begin() +
                      static_cast<std::ptrdiff_t>(retryEditCountBefore + 1u),
                  retryEdits.end(),
                  [retryStartOffset](const SyntaxScriptEntry &entry) {
                    const bool destructive =
                        entry.kind == ParseDiagnosticKind::Deleted ||
                        entry.kind == ParseDiagnosticKind::Replaced;
                    return destructive && entry.beginOffset > retryStartOffset;
                  });
            }
          }
          const auto recoveredVisibleLeafCount = ctx.failureHistorySize();
          const bool exposesStructuredEntry =
              recoveredVisibleLeafCount >= retryVisibleLeafCount + 2u ||
              (ctx.cursor() == ctx.end &&
               recoveredVisibleLeafCount > retryVisibleLeafCount);
          if (ctx.cursor() == ctx.end && exposesStructuredEntry &&
              !retryInsertThenDestructiveSuffix) {
            guard.commit();
            return true;
          }
        }
        ctx.skipAfterDelete = false;
        return false;
      },
      {.disableDeleteRetry = false, .extendThroughHiddenTrivia = false});
}

} // namespace

void WindowPlanner::begin(const FailureSnapshot &failureSnapshot,
                          const RecoveryAttempt &selectedAttempt) noexcept {
  _failureSnapshot = &failureSnapshot;
  const auto prefixContract =
      recovery_window_prefix_contract(failureSnapshot, selectedAttempt);
  _editFloorOffset = prefixContract.editFloorOffset;
  _stablePrefixOffset = prefixContract.stablePrefixOffset;
  _hasStablePrefix = prefixContract.hasStablePrefix;
  _windowTokenCount =
      std::max<std::uint32_t>(1u, _options.recoveryWindowTokenCount);
  _forwardWindowTokenCount = _windowTokenCount;
}

PlannedRecoveryWindow WindowPlanner::plan() const noexcept {
  auto window =
      compute_recovery_window(*_failureSnapshot, _windowTokenCount,
                              _forwardWindowTokenCount, _editFloorOffset);
  window.stablePrefixOffset = _stablePrefixOffset;
  window.hasStablePrefix = _hasStablePrefix;
  return {.window = window, .requestedTokenCount = _forwardWindowTokenCount};
}

bool WindowPlanner::tryWiden() noexcept {
  const auto cap =
      std::max(_options.maxRecoveryWindowTokenCount, _windowTokenCount);
  if (_forwardWindowTokenCount >= cap) {
    return false;
  }
  _forwardWindowTokenCount = cap;
  return true;
}

void fill_committed_recovery_prefix(RecoveryAttemptSpec &spec,
                                    const RecoveryAttempt &selectedAttempt) {
  const auto contract = committed_replay_prefix_contract(selectedAttempt);
  spec.committedRecoveryResumeFloor = contract.resumeFloor;
  spec.committedRecoveryEdits.clear();
  if (contract.preservedEditCount == 0u) {
    return;
  }
  spec.committedRecoveryEdits.assign(
      selectedAttempt.recoveryEdits.begin(),
      selectedAttempt.recoveryEdits.begin() +
          static_cast<std::ptrdiff_t>(contract.preservedEditCount));
}

RecoveryAttempt
execute_recovery_parse(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const text::TextSnapshot &text,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken) {
  RecoveryAttempt attempt;
  attempt.stablePrefixOffset = spec.window.stablePrefixOffset;
  attempt.hasStablePrefix = spec.window.hasStablePrefix;
  attempt.configuredMaxEditCost = options.maxRecoveryEditCost;
  const auto inputSize = static_cast<TextOffset>(text.size());

  FailureHistoryRecorder failureRecorder(text.view().data());
  auto cst = std::make_unique<RootCstNode>(text);
  CstBuilder builder(*cst);
  RecoveryContext parseCtx{builder, skipper, failureRecorder, cancelToken};
  parseCtx.setCommittedRecoveryPrefix(spec.committedRecoveryEdits,
                                      spec.committedRecoveryResumeFloor);
  parseCtx.setEditWindow(RecoveryContext::EditWindow{
      .beginOffset = spec.window.beginOffset,
      .editFloorOffset = spec.window.editFloorOffset,
      .maxCursorOffset = spec.window.maxCursorOffset,
      .forwardTokenCount = spec.window.forwardTokenCount,
      .replayForwardTokenCount = std::min(
          spec.window.forwardTokenCount, options.recoveryStabilityTokenCount)});
  parseCtx.trackEditState = true;
  parseCtx.recoveryState.windowReplay.inRecoveryPhase = false;
  parseCtx.allowTopLevelPartialSuccess = true;
  parseCtx.maxConsecutiveCodepointDeletes =
      options.maxConsecutiveCodepointDeletes;
  parseCtx.stabilityTokenCount = options.recoveryStabilityTokenCount;
  parseCtx.maxEditsPerAttempt = options.maxRecoveryEditsPerAttempt;
  parseCtx.maxEditCost = options.maxRecoveryEditCost;
  parseCtx.maxResyncSkipBytes = options.maxResyncSkipBytes;
  parseCtx.choiceRecoverCache.setDisabled(
      options.diagnostics.recoveryCacheDisabled);

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
  const bool hasRootOffsetEdit =
      !failureRecoveryEdits.empty() &&
      failureRecoveryEdits.front().beginOffset == 0;
  const bool weakMatchedZeroPrefixAttempt =
      attempt.entryRuleMatched && hasRootOffsetEdit &&
      failureParsedLength == 0 &&
      !failureReachedRecoveryTarget && !failureStableAfterRecovery;
  const bool rootInsertThenDestructiveSuffixAttempt =
      attempt.entryRuleMatched &&
      starts_with_root_insert_then_destructive_suffix_edit(
          failureRecoveryEdits);
  if (!attempt.entryRuleMatched || weakMatchedZeroPrefixAttempt ||
      rootInsertThenDestructiveSuffixAttempt) {
    const bool hadDirectMatch = attempt.entryRuleMatched;
    parseCtx.rewind(attemptCheckpoint);
    if (can_try_prefix_delete_retry(entryRule, spec)) {
      PEGIUM_STEP_TRACE_INC(StepCounter::RootPrefixRetryRuns);
      attempt.entryRuleMatched =
          try_prefix_delete_retry_entry_rule(parseCtx, entryRule);
      if (attempt.entryRuleMatched) {
        PEGIUM_STEP_TRACE_INC(StepCounter::RootPrefixRetryWins);
      }
    }
    if (!attempt.entryRuleMatched && hadDirectMatch) {
      attempt.entryRuleMatched = entryRule.recover(parseCtx);
    }
  }

  parseCtx.skip();
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
    auto window = spec.window;
    const auto replayForwardCounts = parseCtx.replayForwardTokenCounts();
    if (!replayForwardCounts.empty()) {
      window.forwardTokenCount = replayForwardCounts.front();
    }
    attempt.replayWindow = window;
  }
  if (!attempt.fullMatch && parseCtx.isFailureHistoryRecordingEnabled()) {
    auto failureSnapshotOffset =
        attempt.entryRuleMatched
            ? std::max(attempt.parsedLength, attempt.maxCursorOffset)
            : attempt.maxCursorOffset;
    auto trackedSnapshot = failureRecorder.snapshot(failureSnapshotOffset);
    if (attempt.entryRuleMatched &&
        attempt.parsedLength < failureSnapshotOffset &&
        should_fallback_to_parsed_length_snapshot(trackedSnapshot,
                                                  attempt.parsedLength)) {
      failureSnapshotOffset = attempt.parsedLength;
      trackedSnapshot = failureRecorder.snapshot(failureSnapshotOffset);
    }
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
  attempt.choiceRecoverCacheHits = parseCtx.choiceRecoverCache.hits();
  attempt.choiceRecoverCacheMisses = parseCtx.choiceRecoverCache.misses();
  return attempt;
}

RecoverySearchRunResult
orchestrate_recovery_search(const grammar::ParserRule &entryRule,
                    const Skipper &skipper, const ParseOptions &options,
                    const text::TextSnapshot &text,
                    const utils::CancellationToken &cancelToken) {
  RecoverySearchRunResult result;
  WindowPlanner windowPlanner{options};

  PEGIUM_STEP_TRACE_INC(StepCounter::ParsePhaseRuns);
  auto strictFailureStage =
      parse_strict_capturing_failure(entryRule, skipper, options, text, cancelToken);
  result.selectedAttempt = std::move(strictFailureStage.strictAttempt);
  result.failureVisibleCursorOffset =
      strictFailureStage.failureVisibleCursorOffset;
  result.strictParseRuns = 1u;
  if (!strictFailureStage.failureSnapshot.has_value()) {
    return result;
  }

  if (result.selectedAttempt.fullMatch) {
    return result;
  }

  FailureSnapshot currentFailure = *std::move(strictFailureStage.failureSnapshot);
  TextOffset previousRecoveryEditFloor = 0;
  // Tracks the last observed relocalization offset so the outer loop can
  // detect an unproductive relocalization cycle: if a continuing recovery
  // keeps reporting the same (or later) failure offset without ever
  // committing a new edit, we have converged without progress and must
  // stop rather than burn the attempt budget on identical replans.
  std::optional<TextOffset> previousRelocalizationOffset;
  while (true) {
    if (recovery_attempt_budget_exhausted(result, options)) {
      return result;
    }
    windowPlanner.begin(currentFailure, result.selectedAttempt);

    const auto initialPlan = windowPlanner.plan();
    if (result.recoveryWindowsTried > 0u &&
        initialPlan.window.editFloorOffset <= previousRecoveryEditFloor) {
      return result;
    }

    RecoveryWindow committedWindow{};
    bool siteCommitted = false;
    bool relocalizedCurrentFailure = false;
    // The committed-prefix replay info depends on the currently selected
    // attempt, which only changes when a window commits (after which we
    // break the inner loop). Compute it once per outer iteration instead of
    // re-copying the prefix vector for every widening attempt.
    RecoveryAttemptSpec spec{};
    const bool continuingRecovery = !result.selectedWindows.empty();
    fill_committed_recovery_prefix(spec, result.selectedAttempt);
    while (true) {
      if (recovery_attempt_budget_exhausted(result, options)) {
        return result;
      }
      const auto plannedWindow = windowPlanner.plan();
      const auto &window = plannedWindow.window;
      trace_recovery_window(result.recoveryWindowsTried,
                            plannedWindow.requestedTokenCount, window,
                            result.selectedWindows.size());
      ++result.recoveryWindowsTried;

      spec.window = window;
      auto windowSearch = try_recovery_window(
          entryRule, skipper, options, text, spec, result.selectedAttempt,
          continuingRecovery, result.recoveryAttemptRuns,
          result.budgetedRecoveryAttemptRuns,
          result.choiceRecoverCacheHits, result.choiceRecoverCacheMisses,
          cancelToken);
      if (apply_qualified_window_acceptance(windowSearch, window,
                                            result.selectedAttempt)) {
        committedWindow = result.selectedAttempt.replayWindow.value_or(window);
        result.selectedWindows.push_back(committedWindow);
        siteCommitted = true;
        // A real commit cleared the previous relocalization cycle (if
        // any); reset the tracker so later sites can still relocalize.
        previousRelocalizationOffset.reset();
        break;
      }
      if (windowSearch.relocalizedFailure.has_value()) {
        const auto nextOffset =
            failure_visible_cursor_offset(*windowSearch.relocalizedFailure,
                                          std::numeric_limits<TextOffset>::max());
        if (previousRelocalizationOffset.has_value() &&
            nextOffset >= *previousRelocalizationOffset) {
          // Unproductive relocalization cycle: the next candidate
          // points at the same (or later) failure offset as the
          // previous one, yet no new edit committed. Further outer
          // iterations would re-plan the identical window and produce
          // the same non-accepting attempt. Stop here.
          return result;
        }
        previousRelocalizationOffset = nextOffset;
        currentFailure = *std::move(windowSearch.relocalizedFailure);
        relocalizedCurrentFailure = true;
        break;
      }
      if (!windowPlanner.tryWiden()) {
        break;
      }
    }

    if (relocalizedCurrentFailure) {
      continue;
    }
    if (!siteCommitted) {
      return result;
    }
    if (result.selectedAttempt.fullMatch) {
      return result;
    }
    if (!result.selectedAttempt.failureSnapshot.has_value()) {
      return result;
    }
    previousRecoveryEditFloor = committedWindow.editFloorOffset;
    currentFailure = *result.selectedAttempt.failureSnapshot;
  }
}


void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept {
  using enum RecoveryAttemptStatus;
  // Refresh the cached facts on every classification call. Tests
  // frequently mutate attempt fields between calls (or build attempts
  // by hand and classify them); always recomputing keeps the cache
  // consistent without leaking complexity to callers.
  attempt.facts = derive_attempt_facts(attempt);
  const auto &facts = attempt.facts;

  if (!attempt.entryRuleMatched) {
    attempt.status = StrictFailure;
    return;
  }

  const bool editsContinueAfterFirstEdit =
      !facts.hasEdits || facts.continuesAfterFirstEdit;
  const bool preservesStablePrefixWithinReplayHorizon =
      facts.preservesStablePrefixBeforeFirstEdit &&
      !facts.hasEditPastReplayWindowHorizon;
  // A full match that reached EOF with genuine continuation past its first
  // edit is the strongest possible success signal; the budget exists to cap
  // exploratory edits, not to reject a complete parse.
  const bool fullMatchWithContinuation =
      attempt.fullMatch && editsContinueAfterFirstEdit;
  if (attempt.editCost > attempt.configuredMaxEditCost &&
      !fullMatchWithContinuation &&
      !preservesStablePrefixWithinReplayHorizon &&
      !facts.allowsLocalGapRecoveryWithoutStablePrefix) {
    attempt.status =
        facts.hasEdits ? RecoveredButNotCredible : StrictFailure;
    return;
  }

  const bool deleteOnlyWithoutContinuation =
      facts.hasDeleteOnlyRecovery && !facts.continuesAfterFirstEdit;
  if (attempt.fullMatch || attempt.stableAfterRecovery) {
    if (!attempt.fullMatch && facts.hasEditPastReplayWindowHorizon) {
      attempt.status = RecoveredButNotCredible;
      return;
    }
    if (facts.startsWithStableBoundaryDeleteWithoutContinuation ||
        deleteOnlyWithoutContinuation ||
        facts.startsWithUnstablePrefixInsertThenDestructiveSuffixEdit) {
      attempt.status = RecoveredButNotCredible;
      return;
    }
    attempt.status = Stable;
  } else if (attempt.reachedRecoveryTarget &&
             !facts.startsWithUnstablePrefixDelete &&
             editsContinueAfterFirstEdit) {
    attempt.status = Credible;
  } else if (attempt.completedRecoveryWindows > 0 || facts.hasEdits) {
    attempt.status = RecoveredButNotCredible;
  } else {
    attempt.status = StrictFailure;
  }
}

bool is_selectable_recovery_attempt(const RecoveryAttempt &attempt) noexcept {
  using enum RecoveryAttemptStatus;
  return attempt.status == Credible || attempt.status == Stable;
}

[[nodiscard]] auto build_non_credible_replay_contract(
    const RecoveryAttempt &attempt) noexcept
    -> std::optional<CommittedPrefixContract> {
  if (!attempt.entryRuleMatched ||
      attempt.status != RecoveryAttemptStatus::RecoveredButNotCredible ||
      attempt.recoveryEdits.empty()) {
    return std::nullopt;
  }

  const auto &facts = attempt.facts;

  if (facts.preservesStablePrefixBeforeFirstEdit) {
    return CommittedPrefixContract{
        .resumeFloor = attempt.stablePrefixOffset,
        .preservedEditCount = 0u,
    };
  }

  const bool establishesLocalInsertedFallback =
      attempt.recoveryEdits.size() == 1u && facts.hasInsertedRecovery &&
      facts.continuesAfterFirstEdit && facts.hasVisibleProgressPastLastEdit;
  if (establishesLocalInsertedFallback) {
    return CommittedPrefixContract{
        .resumeFloor = facts.firstEditOffset,
        .preservedEditCount = 0u,
    };
  }

  // When the entry rule's leading literal failed strictly and was
  // repaired by a fuzzy Replace at offset 0, there is no "stable
  // prefix" by definition — the strictly-matched prefix length is
  // zero. The classic credibility contract rejects such attempts
  // even when the parse subsequently makes real progress (committed
  // multiple statements after the replace, then hit a downstream
  // failure that needs its own recovery window).
  //
  // Admit the attempt as a non-credible fallback when:
  //   - first edit is a Replace at offset 0 (fuzzy keyword repair on
  //     the entry-rule prefix);
  //   - the parser advanced strictly past that Replace (parsedLength
  //     past the replace span). This is a weaker check than the
  //     usual `continuesAfterFirstEdit` because the LAST edit may
  //     be a synthetic insert at the trailing failure cursor (whose
  //     endOffset coincides with parsedLength); requiring progress
  //     past the FIRST edit alone is the right contract here.
  //
  // The next recovery window resumes from the end of the prefix
  // replace (resumeFloor) and the existing edits are preserved
  // (preservedEditCount = full edit set), letting downstream failures
  // open their own credible windows past the entry-rule prefix.
  const bool firstEditIsReplaceAtRoot =
      !attempt.recoveryEdits.empty() &&
      attempt.recoveryEdits.front().beginOffset == 0 &&
      attempt.recoveryEdits.front().kind == ParseDiagnosticKind::Replaced;
  if (!attempt.hasStablePrefix && firstEditIsReplaceAtRoot &&
      attempt.parsedLength > attempt.recoveryEdits.front().endOffset) {
    return CommittedPrefixContract{
        .resumeFloor = attempt.recoveryEdits.front().endOffset,
        .preservedEditCount =
            static_cast<std::uint32_t>(attempt.recoveryEdits.size()),
    };
  }

  if (!facts.allowsLocalDeleteGapRecoveryWithoutStablePrefix) {
    return std::nullopt;
  }

  return CommittedPrefixContract{
      .resumeFloor = facts.firstEditOffset,
      .preservedEditCount = 0u,
  };
}

namespace {

[[nodiscard]] bool preserves_committed_replay_prefix(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt,
    CommittedPrefixContract &contract) noexcept {
  contract = committed_replay_prefix_contract(selectedAttempt);
  if (candidate.recoveryEdits.size() < contract.preservedEditCount) {
    return false;
  }
  for (std::size_t index = 0; index < contract.preservedEditCount; ++index) {
    const auto &selectedEntry = selectedAttempt.recoveryEdits[index];
    const auto &candidateEntry = candidate.recoveryEdits[index];
    if (!same_syntax_script_entry(selectedEntry, candidateEntry)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool rewrites_committed_replay_boundary(
    const RecoveryAttempt &candidate,
    const CommittedPrefixContract &contract) noexcept {
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

  if (contract.resumeFloor >= contract.boundaryFloor) {
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

namespace {

[[nodiscard]] CommittedPrefixContract
committed_replay_prefix_contract(
    const RecoveryAttempt &selectedAttempt) noexcept {
  CommittedPrefixContract contract;
  if (selectedAttempt.recoveryEdits.empty()) {
    return contract;
  }
  const auto &facts = selectedAttempt.facts;
  contract.boundaryFloor =
      std::max(selectedAttempt.parsedLength, facts.lastEditOffset);
  if (selectedAttempt.status == RecoveryAttemptStatus::RecoveredButNotCredible &&
      facts.hasCursorProgressBetweenOrPastEdits) {
    if (selectedAttempt.stableAfterRecovery &&
        facts.hasVisibleProgressPastLastEdit) {
      contract.resumeFloor = contract.boundaryFloor;
      contract.preservedEditCount = selectedAttempt.recoveryEdits.size();
      return contract;
    }
    const auto preservedEditCount =
        replay_horizon_preserved_edit_count(selectedAttempt);
    contract.resumeFloor =
        replay_horizon_resume_floor(selectedAttempt, preservedEditCount);
    contract.preservedEditCount = preservedEditCount;
    return contract;
  }
  if (recovery_attempt_establishes_replay_contract(selectedAttempt)) {
    contract.resumeFloor = contract.boundaryFloor;
    contract.preservedEditCount = selectedAttempt.recoveryEdits.size();
    return contract;
  }
  contract.resumeFloor = contract.boundaryFloor;
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
  return contract;
}

[[nodiscard]] bool localizes_current_failure_site(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  if (!candidate.entryRuleMatched || !candidate.failureSnapshot.has_value() ||
      !selectedAttempt.failureSnapshot.has_value() ||
      selectedAttempt.status != RecoveryAttemptStatus::RecoveredButNotCredible ||
      candidate.recoveryEdits.size() != selectedAttempt.recoveryEdits.size()) {
    return false;
  }
  if (!std::equal(candidate.recoveryEdits.begin(), candidate.recoveryEdits.end(),
                  selectedAttempt.recoveryEdits.begin(),
                  same_syntax_script_entry)) {
    return false;
  }
  const auto candidateFailureOffset =
      failure_visible_cursor_offset(*candidate.failureSnapshot,
                                    candidate.parsedLength);
  const auto selectedFailureOffset =
      failure_visible_cursor_offset(*selectedAttempt.failureSnapshot,
                                    selectedAttempt.parsedLength);
  return candidateFailureOffset < selectedFailureOffset;
}

} // namespace

bool satisfies_non_credible_fallback_contract(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept {
  auto contract = build_non_credible_replay_contract(candidate);
  if (!contract.has_value() ||
      !preserves_committed_replay_prefix(candidate, selectedAttempt,
                                         *contract) ||
      rewrites_committed_replay_boundary(candidate, *contract)) {
    return false;
  }

  const auto &candidateFacts = candidate.facts;
  const auto &selectedFacts = selectedAttempt.facts;

  if (selectedFacts.hasEdits &&
      selectedAttempt.status == RecoveryAttemptStatus::RecoveredButNotCredible &&
      selectedFacts.hasCursorProgressPastLastEdit &&
      contract->preservedEditCount == selectedAttempt.recoveryEdits.size()) {
    const auto committedSourceFloor =
        std::max(selectedAttempt.parsedLength, selectedFacts.lastEditOffset);
    const auto newEdits = std::span(candidate.recoveryEdits)
                              .subspan(contract->preservedEditCount);
    const bool rewritesCommittedSource = std::ranges::any_of(
        newEdits, [&](const SyntaxScriptEntry &entry) noexcept {
          return entry.beginOffset == committedSourceFloor &&
                 entry.kind != ParseDiagnosticKind::Inserted;
        });
    if (rewritesCommittedSource) {
      return false;
    }
  }

  if (!candidateFacts.hasCursorProgressBetweenOrPastEdits ||
      candidate.maxCursorOffset <= contract->resumeFloor) {
    return false;
  }

  if (contract->preservedEditCount == 0u) {
    // Root-replace recovery escape hatch: the entry rule's leading
    // literal was repaired by a fuzzy Replace at offset 0. No stable
    // prefix exists by definition, but the parser advanced past the
    // replace (parsedLength > replace.endOffset). Treat the
    // post-replace region as the de-facto stable continuation.
    const bool firstEditIsReplaceAtRoot =
        !candidate.recoveryEdits.empty() &&
        candidate.recoveryEdits.front().beginOffset == 0 &&
        candidate.recoveryEdits.front().kind ==
            ParseDiagnosticKind::Replaced &&
        candidate.parsedLength >
            candidate.recoveryEdits.front().endOffset;
    if (!candidate.hasStablePrefix &&
        !candidateFacts.hasVisibleProgressPastLastEdit &&
        !firstEditIsReplaceAtRoot) {
      return false;
    }
    return !candidateFacts.hasDeleteOnlyRecovery ||
           candidateFacts.continuesAfterFirstEdit ||
           firstEditIsReplaceAtRoot;
  }

  return candidate.parsedLength > selectedAttempt.parsedLength ||
         candidate.maxCursorOffset > selectedAttempt.maxCursorOffset;
}

[[nodiscard]] RecoveryKey
recovery_attempt_key(const RecoveryAttempt &attempt) noexcept {
  const bool selectableAttempt =
      attempt.status == RecoveryAttemptStatus::Credible ||
      attempt.status == RecoveryAttemptStatus::Stable;
  const auto &facts = attempt.facts;
  const TextOffset effectiveFirstEditOffset =
      facts.hasEdits ? facts.firstEditOffset : attempt.parsedLength;
  return {
      .fullMatch = attempt.fullMatch,
      .matched = selectableAttempt,
      .firstEditOffset = effectiveFirstEditOffset,
      .editCost = attempt.editCost,
      .progressAfterEdits = attempt.parsedLength,
  };
}

bool is_better_recovery_attempt(const RecoveryAttempt &lhs,
                                const RecoveryAttempt &rhs) noexcept {
  return is_better_recovery_key(recovery_attempt_key(lhs),
                                recovery_attempt_key(rhs));
}

} // namespace pegium::parser::detail
