#include <pegium/parser/RecoverySearch.hpp>

#include <algorithm>

#include <pegium/parser/ParseContext.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::parser::detail {

RecoveryAttempt run_recovery_attempt(const grammar::ParserRule &entryRule,
                                     const Skipper &skipper,
                                     const ParseOptions &options,
                     const workspace::Document &document,
                     const RecoveryAttemptSpec &spec,
                     const utils::CancellationToken &cancelToken) {
  RecoveryAttempt attempt;
  attempt.editFloorOffset =
      spec.windows.empty() ? 0 : spec.windows.front().beginOffset;
  const auto inputSize =
      static_cast<TextOffset>(document.textView().size());

  FailureHistoryRecorder failureRecorder(document.textView().data());
  auto cst = std::make_unique<RootCstNode>(document);
  CstBuilder builder(*cst);
  RecoveryContext parseCtx{builder, skipper, failureRecorder, cancelToken};
  std::vector<RecoveryContext::EditWindow> editWindows;
  editWindows.reserve(spec.windows.size());
  for (const auto &window : spec.windows) {
    editWindows.push_back({.beginOffset = window.beginOffset,
                           .maxCursorOffset = window.maxCursorOffset,
                           .forwardTokenCount = window.tokenCount});
  }
  parseCtx.setEditWindows(std::move(editWindows));
  parseCtx.trackEditState = true;
  parseCtx.recoveryState.inRecoveryPhase = false;
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
  }

  parseCtx.skip();
  parseCtx.finalizeRecoveryAtEof();
  attempt.parsedLength =
      attempt.entryRuleMatched ? parseCtx.cursorOffset() : failureParsedLength;
  attempt.lastVisibleCursorOffset = parseCtx.lastVisibleCursorOffset();
  attempt.parseDiagnostics = attempt.entryRuleMatched
                                 ? RecoveryContext::materializeRecoveryEdits(
                                       parseCtx.takeRecoveryEdits())
                                 : RecoveryContext::materializeRecoveryEdits(
                                       failureRecoveryEdits);
  attempt.fullMatch =
      attempt.entryRuleMatched && attempt.parsedLength == inputSize;
  attempt.maxCursorOffset = attempt.entryRuleMatched
                                ? parseCtx.maxCursorOffset()
                                : failureMaxCursorOffset;
  if (!attempt.fullMatch && parseCtx.isFailureHistoryRecordingEnabled()) {
    attempt.failureSnapshot = failureRecorder.snapshot(attempt.maxCursorOffset);
  }
  attempt.editCost =
      attempt.entryRuleMatched ? parseCtx.currentEditCost() : failureEditCost;
  attempt.editCount =
      attempt.entryRuleMatched ? parseCtx.currentEditCount() : failureEditCount;
  attempt.completedRecoveryWindows = attempt.entryRuleMatched
                                         ? parseCtx.completedRecoveryWindowCount()
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

void classify_recovery_attempt(RecoveryAttempt &attempt) noexcept {
  if (!attempt.entryRuleMatched) {
    attempt.status = RecoveryAttemptStatus::StrictFailure;
    return;
  }

  if (attempt.fullMatch || attempt.stableAfterRecovery) {
    attempt.status = RecoveryAttemptStatus::Stable;
  } else if (attempt.reachedRecoveryTarget) {
    attempt.status = RecoveryAttemptStatus::Credible;
  } else if (attempt.completedRecoveryWindows > 0 ||
             !attempt.parseDiagnostics.empty()) {
    attempt.status = RecoveryAttemptStatus::RecoveredButNotCredible;
  } else {
    attempt.status = RecoveryAttemptStatus::StrictFailure;
  }
}

void score_recovery_attempt(RecoveryAttempt &attempt) noexcept {
  attempt.editTrace.diagnosticCount = attempt.parseDiagnostics.size();
  attempt.editTrace.editCount = attempt.editCount;
  attempt.editTrace.editCost = attempt.editCost;
  for (const auto &diagnostic : attempt.parseDiagnostics) {
    switch (diagnostic.kind) {
    case ParseDiagnosticKind::Inserted:
      ++attempt.editTrace.insertCount;
      ++attempt.editTrace.tokenInsertCount;
      break;
    case ParseDiagnosticKind::Deleted:
      ++attempt.editTrace.deleteCount;
      ++attempt.editTrace.codepointDeleteCount;
      break;
    case ParseDiagnosticKind::Replaced:
      ++attempt.editTrace.replaceCount;
      break;
    case ParseDiagnosticKind::Incomplete:
    case ParseDiagnosticKind::Recovered:
    case ParseDiagnosticKind::ConversionError:
      break;
    }
  }
  if (!attempt.parseDiagnostics.empty()) {
    const auto first = std::ranges::min(
        attempt.parseDiagnostics, {}, &ParseDiagnostic::offset);
    const auto last = std::ranges::max(
        attempt.parseDiagnostics, {}, &ParseDiagnostic::offset);
    attempt.editTrace.hasEdits = true;
    attempt.editTrace.firstEditOffset = first.offset;
    attempt.editTrace.lastEditOffset = last.offset;
    attempt.editTrace.editSpan =
        attempt.editTrace.lastEditOffset - attempt.editTrace.firstEditOffset;
  }

  attempt.score = {
      .entryRuleMatched = attempt.entryRuleMatched,
      .stable = attempt.status == RecoveryAttemptStatus::Stable,
      .credible = attempt.status == RecoveryAttemptStatus::Credible ||
                  attempt.status == RecoveryAttemptStatus::Stable,
      .editCost = attempt.editCost,
      .fullMatch = attempt.fullMatch,
      .editSpan = attempt.editTrace.editSpan,
      .diagnosticCount =
          static_cast<std::uint32_t>(attempt.editTrace.diagnosticCount),
      .firstEditOffset = attempt.editTrace.firstEditOffset,
      .parsedLength = attempt.parsedLength,
      .maxCursorOffset = attempt.maxCursorOffset,
  };
}

bool is_selectable_recovery_attempt(const RecoveryAttempt &attempt) noexcept {
  return attempt.status == RecoveryAttemptStatus::Credible ||
         attempt.status == RecoveryAttemptStatus::Stable;
}

bool is_better_recovery_attempt(const RecoveryAttempt &lhs,
                                const RecoveryAttempt &rhs) noexcept {
  if (lhs.score.entryRuleMatched != rhs.score.entryRuleMatched) {
    return lhs.score.entryRuleMatched && !rhs.score.entryRuleMatched;
  }
  if (lhs.score.stable != rhs.score.stable) {
    return lhs.score.stable && !rhs.score.stable;
  }
  if (lhs.score.credible != rhs.score.credible) {
    return lhs.score.credible && !rhs.score.credible;
  }
  if (lhs.score.editCost != rhs.score.editCost) {
    return lhs.score.editCost < rhs.score.editCost;
  }
  if (lhs.score.fullMatch != rhs.score.fullMatch) {
    return lhs.score.fullMatch && !rhs.score.fullMatch;
  }
  if (lhs.score.parsedLength != rhs.score.parsedLength) {
    return lhs.score.parsedLength > rhs.score.parsedLength;
  }
  if (lhs.score.maxCursorOffset != rhs.score.maxCursorOffset) {
    return lhs.score.maxCursorOffset > rhs.score.maxCursorOffset;
  }
  if (lhs.score.editSpan != rhs.score.editSpan) {
    return lhs.score.editSpan < rhs.score.editSpan;
  }
  if (lhs.score.diagnosticCount != rhs.score.diagnosticCount) {
    return lhs.score.diagnosticCount < rhs.score.diagnosticCount;
  }
  if (lhs.score.firstEditOffset != rhs.score.firstEditOffset) {
    return lhs.score.firstEditOffset > rhs.score.firstEditOffset;
  }
  return false;
}

RecoveryAttemptSpec
build_recovery_attempt_spec(std::span<const RecoveryWindow> selectedWindows,
                            const RecoveryWindow &window) noexcept {
  RecoveryAttemptSpec spec;
  spec.windows.assign(selectedWindows.begin(), selectedWindows.end());
  spec.windows.push_back(window);
  return spec;
}

} // namespace pegium::parser::detail
