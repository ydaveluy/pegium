#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <pegium/grammar/ParserRule.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/parser/RecoveryAnalysis.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::workspace {
struct Document;
}

namespace pegium::parser::detail {

enum class RecoveryAttemptStatus : std::uint8_t {
  StrictFailure,
  RecoveredButNotCredible,
  Credible,
  Stable,
  Selected,
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
  std::size_t diagnosticCount = 0;
  std::uint32_t editCost = 0;
  bool hasEdits = false;
};

struct RecoveryScore {
  bool entryRuleMatched = false;
  bool stable = false;
  bool credible = false;
  std::uint32_t editCost = 0;
  bool fullMatch = false;
  TextOffset editSpan = 0;
  std::uint32_t diagnosticCount = 0;
  TextOffset firstEditOffset = 0;
  TextOffset parsedLength = 0;
  TextOffset maxCursorOffset = 0;
};

struct RecoveryAttemptSpec {
  std::vector<RecoveryWindow> windows;
};

struct RecoveryAttempt {
  std::unique_ptr<RootCstNode> cst;
  std::vector<ParseDiagnostic> parseDiagnostics;
  std::optional<FailureSnapshot> failureSnapshot;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  TextOffset editFloorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  std::uint32_t completedRecoveryWindows = 0;
  bool entryRuleMatched = false;
  bool fullMatch = false;
  bool reachedRecoveryTarget = false;
  bool stableAfterRecovery = false;
  RecoveryAttemptStatus status = RecoveryAttemptStatus::StrictFailure;
  EditTrace editTrace;
  RecoveryScore score;
};

[[nodiscard]] RecoveryAttempt
run_recovery_attempt(const grammar::ParserRule &entryRule,
                     const Skipper &skipper, const ParseOptions &options,
                     const workspace::Document &document,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken = {});

void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept;

void score_recovery_attempt(RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
is_selectable_recovery_attempt(const RecoveryAttempt &attempt) noexcept;

[[nodiscard]] bool
is_better_recovery_attempt(const RecoveryAttempt &lhs,
                           const RecoveryAttempt &rhs) noexcept;

[[nodiscard]] RecoveryAttemptSpec
build_recovery_attempt_spec(std::span<const RecoveryWindow> selectedWindows,
                            const RecoveryWindow &window) noexcept;

} // namespace pegium::parser::detail
