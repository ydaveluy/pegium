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
  Credible,
  Stable,
};

struct RecoveryAttemptSpec {
  RecoveryWindow window;
  std::vector<SyntaxScriptEntry> committedRecoveryEdits;
  TextOffset committedRecoveryResumeFloor = 0;
};

/// Derived facts about a `RecoveryAttempt`. These are pure functions of
/// the attempt's edits, cursor offsets, and result fields. Computed once
/// at classification time and cached on `RecoveryAttempt::facts` so that
/// downstream predicates can read fields instead of re-deriving them on
/// every call.
struct AttemptFacts {
  TextOffset firstEditOffset = std::numeric_limits<TextOffset>::max();
  TextOffset lastEditOffset = 0;
  bool continuesAfterFirstEdit = false;

  bool hasEdits = false;
  bool hasInsertedRecovery = false;
  bool hasDeleteOnlyRecovery = false;

  bool hasCursorProgressPastLastEdit = false;
  bool hasVisibleProgressPastLastEdit = false;
  bool hasCursorProgressBetweenOrPastEdits = false;

  bool preservesStablePrefixBeforeFirstEdit = false;
  bool startsWithUnstablePrefixDelete = false;
  bool startsWithStableBoundaryDeleteWithoutContinuation = false;
  bool startsWithUnstablePrefixInsertThenDestructiveSuffixEdit = false;

  bool hasEditPastReplayWindowHorizon = false;
  bool hasCommittedReplayPrefix = false;

  bool allowsLocalGapRecoveryWithoutStablePrefix = false;
  bool allowsLocalDeleteGapRecoveryWithoutStablePrefix = false;
};

struct RecoveryAttempt {
  std::unique_ptr<RootCstNode> cst;
  std::vector<SyntaxScriptEntry> recoveryEdits;
  std::optional<RecoveryWindow> replayWindow;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  TextOffset noEditFirstEditOffset = 0;
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

struct PlannedRecoveryWindow {
  RecoveryWindow window;
  std::uint32_t requestedTokenCount = 0;
};

class WindowPlanner {
public:
  explicit WindowPlanner(const ParseOptions &options) noexcept
      : _options(options) {}

  void begin(const FailureSnapshot &failureSnapshot,
             const RecoveryAttempt &selectedAttempt) noexcept;

  [[nodiscard]] PlannedRecoveryWindow plan() const noexcept;

  /// Extend the current site's forward window to the configured upper
  /// bound. Returns true when widening actually changes the plan. A site
  /// is widened at most once per acceptance attempt cycle.
  [[nodiscard]] bool tryWiden() noexcept;

private:
  ParseOptions _options{};
  const FailureSnapshot *_failureSnapshot = nullptr;
  TextOffset _editFloorOffset = 0;
  TextOffset _stablePrefixOffset = 0;
  bool _hasStablePrefix = false;
  std::uint32_t _windowTokenCount = 1u;
  std::uint32_t _forwardWindowTokenCount = 1u;
};

struct RecoverySearchRunResult {
  RecoveryAttempt selectedAttempt;
  std::vector<RecoveryWindow> selectedWindows;
  TextOffset failureVisibleCursorOffset = 0;
  std::uint32_t strictParseRuns = 0;
  std::uint32_t recoveryWindowsTried = 0;
  std::uint32_t recoveryAttemptRuns = 0;
  std::uint32_t budgetedRecoveryAttemptRuns = 0;
  std::uint64_t choiceRecoverCacheHits = 0;
  std::uint64_t choiceRecoverCacheMisses = 0;
};

[[nodiscard]] RecoveryAttempt
execute_recovery_parse(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const text::TextSnapshot &text,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken = {});

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
