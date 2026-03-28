#pragma once

/// Global recovery orchestration and shared attempt-order helpers.

#include <cstdint>
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

struct EditTrace {
  TextOffset firstEditOffset = 0;
  TextOffset lastEditOffset = 0;
  TextOffset editSpan = 0;
  std::size_t insertCount = 0;
  std::size_t deleteCount = 0;
  std::size_t replaceCount = 0;
  std::size_t tokenInsertCount = 0;
  std::size_t tokenDeleteCount = 0;
  std::size_t codepointDeleteCount = 0;
  std::size_t editCount = 0;
  std::size_t entryCount = 0;
  std::uint32_t editCost = 0;
  bool hasEdits = false;
};

struct RecoverySelectionScore {
  bool entryRuleMatched = false;
  bool stable = false;
  bool credible = false;
  bool fullMatch = false;
};

struct RecoveryEditScore {
  std::uint32_t editCost = 0;
  TextOffset editSpan = 0;
  std::uint32_t entryCount = 0;
  TextOffset firstEditOffset = 0;
};

struct RecoveryProgressScore {
  TextOffset parsedLength = 0;
  TextOffset maxCursorOffset = 0;
};

struct RecoveryScore {
  RecoverySelectionScore selection;
  RecoveryEditScore edits;
  RecoveryProgressScore progress;
};

struct RecoveryAttemptSpec {
  std::vector<RecoveryWindow> windows;
  bool allowTrailingEofTrim = true;
};

struct RecoveryAttempt {
  std::unique_ptr<RootCstNode> cst;
  std::vector<SyntaxScriptEntry> recoveryEdits;
  std::vector<RecoveryWindow> replayWindows;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  TextOffset stablePrefixOffset = 0;
  std::uint32_t configuredMaxEditCost = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  std::uint32_t completedRecoveryWindows = 0;
  bool entryRuleMatched = false;
  bool fullMatch = false;
  bool reachedRecoveryTarget = false;
  bool stableAfterRecovery = false;
  bool hasStablePrefix = false;
  bool trimmedVisibleTailToEof = false;
  RecoveryAttemptStatus status = RecoveryAttemptStatus::StrictFailure;
  EditTrace editTrace;
  RecoveryScore score;
};

struct PlannedRecoveryWindow {
  RecoveryWindow window;
  std::uint32_t requestedTokenCount = 0;
};

class WindowPlanner {
public:
  explicit WindowPlanner(const ParseOptions &options) noexcept
      : _options(options) {}

  void seedAcceptedWindows(std::span<const RecoveryWindow> acceptedWindows);

  void begin(const FailureSnapshot &failureSnapshot,
             const RecoveryAttempt &selectedAttempt) noexcept;

  [[nodiscard]] PlannedRecoveryWindow plan() const noexcept;

  [[nodiscard]] bool advance(const RecoveryWindow &currentWindow,
                             bool preferBackwardContext = false,
                             bool preferImmediateMaxForwardWiden = false,
                             bool preferImmediateMaxBackwardWiden = false) noexcept;

  [[nodiscard]] RecoveryAttemptSpec
  buildAttemptSpec(const RecoveryWindow &window) const;

  void accept(const RecoveryAttempt &attempt, const RecoveryWindow &window);

  [[nodiscard]] std::span<const RecoveryWindow> acceptedWindows() const noexcept {
    return _acceptedWindows;
  }

private:
  ParseOptions _options{};
  std::vector<RecoveryWindow> _acceptedWindows;
  const FailureSnapshot *_failureSnapshot = nullptr;
  TextOffset _editFloorOffset = 0;
  TextOffset _stablePrefixOffset = 0;
  bool _hasStablePrefix = false;
  std::uint32_t _windowTokenCount = 1u;
  std::uint32_t _forwardWindowTokenCount = 1u;
  bool _triedFullHistoryWindow = false;
  bool _allowBackwardWidenAfterForwardExhausted = false;
};

struct RecoverySearchRunResult {
  RecoveryAttempt selectedAttempt;
  std::vector<RecoveryWindow> selectedWindows;
  TextOffset failureVisibleCursorOffset = 0;
  std::uint32_t strictParseRuns = 0;
  std::uint32_t recoveryWindowsTried = 0;
  std::uint32_t recoveryAttemptRuns = 0;
  std::uint32_t budgetedRecoveryAttemptRuns = 0;
};

[[nodiscard]] RecoveryAttempt
run_recovery_attempt(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const text::TextSnapshot &text,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken = {});

[[nodiscard]] RecoverySearchRunResult
run_recovery_search(const grammar::ParserRule &entryRule,
                    const Skipper &skipper, const ParseOptions &options,
                    const text::TextSnapshot &text,
                    const utils::CancellationToken &cancelToken = {});

void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept;

void score_recovery_attempt(RecoveryAttempt &attempt) noexcept;

[[nodiscard]] NormalizedRecoveryOrderKey
recovery_attempt_order_key(const RecoveryScore &score) noexcept;

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
