#pragma once

/// Global recovery orchestration and shared attempt-order helpers.

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::parser::detail {

enum class RecoveryAttemptStatus : std::uint8_t {
  StrictFailure,
  RecoveredButNotCredible,
  // A credible/stable recovery: selectable by the ranker. Production code never
  // distinguished the former `Credible` (reached the recovery target and
  // continued) from `Stable` (full match / stable-after-recovery) — every
  // consumer OR'd them — so they are one value. The full-vs-continuing
  // provenance is still observable in the attempt's fullMatch /
  // reachedRecoveryTarget / stableAfterRecovery fields (and its JSON dump).
  Selectable,
};

/// Internal probe-perturbation axes. These are NOT user options: each names one
/// greedy recovery shortcut that a post-greedy sibling probe in
/// `try_recovery_window` re-parses WITHOUT, to expose a competing candidate to
/// the shared ranker (kept only if it reaches fullMatch and outranks). They
/// default off and are set only by those probes — hence they live here, on the
/// internal per-attempt spec, rather than on the public `ParseOptions`.
struct RecoveryProbeAxes {
  /// Forbids the fuzzy WHOLE-keyword Replace at a terminal when a boundary
  /// split-Insert is also enumerable (keyword matches exactly as a prefix and a
  /// visible continuation follows). Read in `Literal::try_local_recovery`
  /// via the `RecoveryContext` mirror.
  bool forbidWholeKeywordFuzzyReplace = false;
  /// Forbids the out-of-window single-fuzzy-edit keyword Replace carve-out
  /// (`singleEditFuzzyKeywordReplace` in `ParseContext::replaceLeaf`). Read via
  /// the `RecoveryContext` mirror.
  bool forbidOutOfWindowFuzzyFold = false;
};

struct RecoveryAttemptSpec {
  RecoveryWindow window;
  std::vector<SyntaxScriptEntry> committedRecoveryEdits;
  TextOffset committedRecoveryResumeFloor = 0;
  /// Internal probe knobs (default off); set only by the sibling probes in
  /// `try_recovery_window` for the re-parse they drive.
  RecoveryProbeAxes probeAxes;
};

/// Derived facts about a `RecoveryAttempt`. These are pure functions of
/// the attempt's edits, cursor offsets, and result fields. Computed once
/// at classification time and cached on `RecoveryAttempt::facts` so that
/// downstream predicates can read fields instead of re-deriving them on
/// every call.
struct AttemptFacts {
  TextOffset lastEditOffset = 0;
  bool continuesAfterFirstEdit = false;

  bool hasEdits = false;
  bool hasDeleteOnlyRecovery = false;

  bool hasCursorProgressPastLastEdit = false;

  bool preservesStablePrefixBeforeFirstEdit = false;

  bool hasEditPastReplayWindowHorizon = false;

};

struct RecoveryAttempt {
  std::unique_ptr<RootCstNode> cst;
  std::vector<SyntaxScriptEntry> recoveryEdits;
  std::optional<RecoveryWindow> replayWindow;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  TextOffset stablePrefixOffset = 0;
  std::uint32_t configuredMaxEditCost = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  std::uint32_t completedRecoveryWindows = 0;
  std::uint64_t choiceRecoverCacheHits = 0;
  std::uint64_t choiceRecoverCacheMisses = 0;
  bool entryRuleMatched = false;
  bool fullMatch = false;
  bool reachedRecoveryTarget = false;
  bool stableAfterRecovery = false;
  bool hasStablePrefix = false;
  RecoveryAttemptStatus status = RecoveryAttemptStatus::StrictFailure;
  /// Cached facts derived from this attempt. Populated by
  /// `classify_recovery_attempt` and read by all downstream gates.
  /// Default-constructed (zero-edit shape) until classification runs.
  AttemptFacts facts;
};

/// Compute the facts of a `RecoveryAttempt`. Normally consumers should
/// read `attempt.facts` populated by `classify_recovery_attempt`; this
/// free function exists for the rare cases where facts are needed for
/// an attempt that has not been classified yet.
[[nodiscard]] AttemptFacts
derive_attempt_facts(const RecoveryAttempt &attempt) noexcept;

/// Pure planner: computes the recovery window for a given forward token
/// count. The backward span is `max(1, recoveryWindowTokenCount)` and the
/// edit-floor / stable-prefix come from the committed-prefix contract for
/// `selectedAttempt`. Widening is an explicit caller concern: re-call with a
/// larger `forwardTokenCount` (the orchestrator widens at most once per site,
/// up to `max(maxRecoveryWindowTokenCount, recoveryWindowTokenCount)`).
[[nodiscard]] RecoveryWindow
plan_recovery_window(const FailureSnapshot &failureSnapshot,
                     const RecoveryAttempt &selectedAttempt,
                     const ParseOptions &options,
                     std::uint32_t forwardTokenCount) noexcept;

struct RecoverySearchRunResult {
  RecoveryAttempt selectedAttempt;
  std::vector<RecoveryWindow> selectedWindows;
  TextOffset failureVisibleCursorOffset = 0;
  std::uint32_t strictParseRuns = 0;
  std::uint32_t recoveryWindowsTried = 0;
  std::uint32_t recoveryAttemptRuns = 0;
  std::uint64_t choiceRecoverCacheHits = 0;
  std::uint64_t choiceRecoverCacheMisses = 0;
};

/// Pool of recovery memoization caches reused across the several re-parses of a
/// single recovery search, so each window probe does not allocate a cold
/// ~1.4 MB choice cache and ~256 KB fuzzy cache. Defined in the .cpp; callers
/// outside the driver (tests, direct probes) pass nullptr and get per-attempt
/// caches owned by the `RecoveryContext`.
struct RecoveryParseCachePool;

[[nodiscard]] RecoveryAttempt
execute_recovery_parse(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const text::TextSnapshot &text,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken = {},
                     RecoveryParseCachePool *cachePool = nullptr);

[[nodiscard]] RecoverySearchRunResult
orchestrate_recovery_search(const grammar::ParserRule &entryRule,
                    const Skipper &skipper, const ParseOptions &options,
                    const text::TextSnapshot &text,
                    const utils::CancellationToken &cancelToken = {});

void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept;

[[nodiscard]] RecoveryKey
recovery_attempt_key(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
is_selectable_recovery_attempt(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
satisfies_non_credible_fallback_contract(
    const RecoveryAttempt &candidate,
    const RecoveryAttempt &selectedAttempt) noexcept;

[[nodiscard]] bool
is_better_recovery_attempt(const RecoveryAttempt &lhs,
                           const RecoveryAttempt &rhs) noexcept;

} // namespace pegium::parser::detail
