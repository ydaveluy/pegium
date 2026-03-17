#include <pegium/parser/PegiumParser.hpp>

#include <algorithm>
#include <optional>

#include <pegium/parser/AssignmentHelpers.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/ParseDiagnostics.hpp>
#include <pegium/parser/RecoveryAnalysis.hpp>
#include <pegium/parser/RecoveryDebug.hpp>
#include <pegium/parser/RecoverySearch.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::parser {

namespace {

struct StandaloneCoreServices {
  services::SharedCoreServices shared;
  services::CoreServices core;

  StandaloneCoreServices() : core(shared) {
    services::installDefaultSharedCoreServices(shared);
    services::installDefaultCoreServices(core);
  }
};

const services::CoreServices &standalone_core_services() noexcept {
  static const StandaloneCoreServices services;
  return services.core;
}

void trace_recovery_json(const char *label, const services::JsonValue &value) {
  PEGIUM_RECOVERY_TRACE(label, " ",
                        value.toJsonString({.pretty = false}));
}

void trace_strict_summary(const detail::StrictParseSummary &summary) {
  trace_recovery_json("[parser strict]",
                      detail::strict_parse_summary_to_json(summary));
}

void trace_failure_snapshot(const detail::FailureSnapshot &snapshot) {
  trace_recovery_json("[parser snapshot]",
                      detail::failure_snapshot_to_json(snapshot));
}

void trace_recovery_window(std::uint32_t windowIndex,
                           std::uint32_t tokenCount,
                           const detail::RecoveryWindow &window,
                           std::size_t selectedWindowCount) {
  auto payload = detail::recovery_window_to_json(window);
  auto &object = payload.object();
  object["selectedWindowCount"] =
      static_cast<std::int64_t>(selectedWindowCount);
  object["windowIndex"] = static_cast<std::int64_t>(windowIndex);
  object["requestedTokenCount"] = static_cast<std::int64_t>(tokenCount);
  trace_recovery_json("[parser window]", payload);
}

void trace_recovery_attempt(const detail::RecoveryAttempt &attempt,
                            const detail::RecoveryAttemptSpec &spec) {
  trace_recovery_json("[parser attempt]",
                      detail::recovery_attempt_to_json(attempt, &spec));
}

[[nodiscard]] bool
has_trailing_failure_leaf(const detail::FailureSnapshot &snapshot) noexcept {
  return snapshot.hasFailureToken &&
         snapshot.failureTokenIndex + 1u == snapshot.failureLeafHistory.size();
}

[[nodiscard]] bool
cursor_outruns_last_visible_failure_leaf(
    const detail::FailureSnapshot &snapshot) noexcept {
  return has_trailing_failure_leaf(snapshot) &&
         !snapshot.failureLeafHistory.empty() &&
         snapshot.failureLeafHistory.back().endOffset < snapshot.maxCursorOffset;
}

[[nodiscard]] bool
is_near_eof_tail_failure(const detail::FailureSnapshot &snapshot,
                         TextOffset inputSize) noexcept {
  constexpr TextOffset kLargeInputThreshold = 4 * 1024;
  constexpr TextOffset kLateRecoveryTailThreshold = 16;
  return inputSize >= kLargeInputThreshold &&
         cursor_outruns_last_visible_failure_leaf(snapshot) &&
         snapshot.maxCursorOffset < inputSize &&
         inputSize - snapshot.maxCursorOffset <= kLateRecoveryTailThreshold;
}

[[nodiscard]] bool is_inert_trailing_eof_attempt(
    const detail::RecoveryAttempt &attempt,
    const detail::RecoveryAttempt &baseline,
    const detail::FailureSnapshot &snapshot,
    TextOffset inputSize) noexcept {
  if (!is_near_eof_tail_failure(snapshot, inputSize) ||
      !attempt.entryRuleMatched ||
      attempt.fullMatch) {
    return false;
  }
  if (attempt.parsedLength != baseline.parsedLength ||
      attempt.lastVisibleCursorOffset != baseline.lastVisibleCursorOffset ||
      attempt.maxCursorOffset != baseline.maxCursorOffset) {
    return false;
  }
  if (attempt.editCount != 0 || !attempt.parseDiagnostics.empty() ||
      attempt.completedRecoveryWindows != 0) {
    return false;
  }
  return !attempt.reachedRecoveryTarget && !attempt.stableAfterRecovery;
}

} // namespace

PegiumParser::PegiumParser() noexcept
    : services::DefaultCoreService(standalone_core_services()) {}

void PegiumParser::parse(workspace::Document &document,
                         const utils::CancellationToken &cancelToken) const {
  const auto &entryRule = getEntryRule();
  const auto &skipper = getSkipper();
  const ParseOptions options = getParseOptions();
  ParseResult result;
  const auto text = document.textView();
  const TextOffset inputSize = static_cast<TextOffset>(text.size());
  detail::stepTraceReset();
  detail::stepTraceInc(detail::StepCounter::ParsePhaseRuns);
  std::uint32_t strictParseRuns = 1;
  std::uint32_t recoveryWindowsTried = 0;
  std::uint32_t recoveryAttemptRuns = 0;
  TextOffset failureVisibleCursorOffset = 0;
  detail::RecoveryAttempt selectedAttempt;
  {
    auto strictResult =
        detail::run_strict_parse(entryRule, skipper, document, cancelToken);
    selectedAttempt.cst = std::move(strictResult.cst);
    selectedAttempt.entryRuleMatched = strictResult.summary.entryRuleMatched;
    selectedAttempt.parsedLength = strictResult.summary.parsedLength;
    selectedAttempt.lastVisibleCursorOffset =
        strictResult.summary.lastVisibleCursorOffset;
    selectedAttempt.fullMatch = strictResult.summary.fullMatch;
    selectedAttempt.maxCursorOffset = strictResult.summary.maxCursorOffset;
    selectedAttempt.editTrace.firstEditOffset = inputSize;
    failureVisibleCursorOffset = selectedAttempt.lastVisibleCursorOffset;
    detail::score_recovery_attempt(selectedAttempt);
  }
  std::vector<detail::RecoveryWindow> selectedWindows;

  if (!selectedAttempt.fullMatch && options.recoveryEnabled) {
    const detail::StrictParseSummary strictSummary{
        .inputSize = inputSize,
        .parsedLength = selectedAttempt.parsedLength,
        .lastVisibleCursorOffset = selectedAttempt.lastVisibleCursorOffset,
        .maxCursorOffset = selectedAttempt.maxCursorOffset,
        .entryRuleMatched = selectedAttempt.entryRuleMatched,
        .fullMatch = selectedAttempt.fullMatch,
    };
    trace_strict_summary(strictSummary);
    const auto initialFailureAnalysis =
        detail::analyze_failure(entryRule, skipper, document, strictSummary,
                                cancelToken);
    if (initialFailureAnalysis.snapshot.hasFailureToken &&
        initialFailureAnalysis.snapshot.failureTokenIndex <
            initialFailureAnalysis.snapshot.failureLeafHistory.size()) {
      failureVisibleCursorOffset =
          initialFailureAnalysis
              .snapshot
              .failureLeafHistory[initialFailureAnalysis.snapshot.failureTokenIndex]
              .endOffset;
    } else if (!initialFailureAnalysis.snapshot.failureLeafHistory.empty()) {
      failureVisibleCursorOffset =
          initialFailureAnalysis.snapshot.failureLeafHistory.back().endOffset;
    }
    trace_failure_snapshot(initialFailureAnalysis.snapshot);

    for (std::uint32_t windowIndex = 0; windowIndex < options.maxRecoveryWindows;
         ++windowIndex) {
      if (selectedAttempt.fullMatch) {
        break;
      }

      const auto failureSnapshot =
          windowIndex == 0
              ? initialFailureAnalysis.snapshot
              : (selectedAttempt.failureSnapshot.has_value()
                     ? *selectedAttempt.failureSnapshot
                     : detail::snapshot_from_committed_cst(
                           *selectedAttempt.cst,
                           selectedAttempt.maxCursorOffset));
      std::uint32_t windowTokenCount =
          std::max<std::uint32_t>(1u, options.recoveryWindowTokenCount);
      bool acceptedWindow = false;
      bool triedFullHistoryWindow = false;
      while (true) {
        const auto window =
            detail::compute_recovery_window(failureSnapshot, windowTokenCount);
        trace_recovery_window(windowIndex, windowTokenCount, window,
                              selectedWindows.size());
        if (!selectedWindows.empty()) {
          const auto &lastWindow = selectedWindows.back();
              if (lastWindow.beginOffset == window.beginOffset &&
              lastWindow.maxCursorOffset == window.maxCursorOffset) {
            PEGIUM_RECOVERY_TRACE("[parser window] duplicate window, stop");
            break;
          }
        }
        ++recoveryWindowsTried;

        const auto spec =
            detail::build_recovery_attempt_spec(selectedWindows, window);
        if (windowIndex == 0 && selectedWindows.empty() &&
            is_near_eof_tail_failure(failureSnapshot, inputSize)) {
          PEGIUM_RECOVERY_TRACE(
              "[parser window] skip inert late EOF recovery attempt");
          break;
        }

        bool credibleAttemptFound = false;
        std::optional<detail::RecoveryAttempt> bestWindowAttempt;
        bool sawRejectedAttempt = false;
        bool onlyInertTrailingRejectedAttempts = true;
        if (options.maxRecoveryAttempts > 0) {
          ++recoveryAttemptRuns;
          detail::stepTraceInc(detail::StepCounter::RecoveryPhaseRuns);
          auto attempt = detail::run_recovery_attempt(entryRule, skipper, options,
                                                      document, spec,
                                                      cancelToken);
          detail::classify_recovery_attempt(attempt);
          detail::score_recovery_attempt(attempt);
          trace_recovery_attempt(attempt, spec);

          if (!detail::is_selectable_recovery_attempt(attempt)) {
            sawRejectedAttempt = true;
            if (!is_inert_trailing_eof_attempt(attempt, selectedAttempt,
                                               failureSnapshot, inputSize)) {
              onlyInertTrailingRejectedAttempts = false;
            }
            PEGIUM_RECOVERY_TRACE(
                "[parser attempt] rejected for selection status=",
                detail::recovery_attempt_status_name(attempt.status));
          } else {
            credibleAttemptFound = true;
            if (!bestWindowAttempt.has_value() ||
                detail::is_better_recovery_attempt(attempt,
                                                   *bestWindowAttempt)) {
              bestWindowAttempt = std::move(attempt);
            }
          }
        }

        if (bestWindowAttempt.has_value()) {
          const auto &candidate = *bestWindowAttempt;
          if ((candidate.fullMatch && !selectedAttempt.fullMatch) ||
              candidate.parsedLength > selectedAttempt.parsedLength ||
              (candidate.parsedLength == selectedAttempt.parsedLength &&
               detail::is_better_recovery_attempt(candidate, selectedAttempt))) {
            selectedAttempt = std::move(*bestWindowAttempt);
            selectedWindows.push_back(window);
            acceptedWindow = true;
            PEGIUM_RECOVERY_TRACE(
                "[parser window] accepted index=", windowIndex, " begin=",
                window.beginOffset, " max=", window.maxCursorOffset,
                " selectedWindows=", selectedWindows.size(), " full=",
                selectedAttempt.fullMatch, " len=",
                selectedAttempt.parsedLength, " cost=",
                selectedAttempt.editCost, " status=",
                detail::recovery_attempt_status_name(selectedAttempt.status));
            trace_recovery_json("[parser accepted-windows]",
                                detail::recovery_windows_to_json(
                                    selectedWindows));
            trace_recovery_json("[parser selected-attempt]",
                                detail::recovery_attempt_to_json(
                                    selectedAttempt));
          }
        }

        if (credibleAttemptFound) {
          if (!acceptedWindow) {
            PEGIUM_RECOVERY_TRACE(
                "[parser window] credible attempts found, no better candidate");
          }
          break;
        }
        if (sawRejectedAttempt && onlyInertTrailingRejectedAttempts) {
          PEGIUM_RECOVERY_TRACE(
              "[parser window] stop widening after inert trailing EOF failure");
          break;
        }
        const auto nextWindowTokenCount =
            detail::next_recovery_window_token_count(windowTokenCount, options);
        if (!nextWindowTokenCount.has_value()) {
          const auto fullHistoryTokenCount =
              static_cast<std::uint32_t>(std::min<std::size_t>(
                  failureSnapshot.failureLeafHistory.size(),
                  std::numeric_limits<std::uint32_t>::max()));
          if (!triedFullHistoryWindow && window.beginOffset > 0 &&
              fullHistoryTokenCount > windowTokenCount) {
            PEGIUM_RECOVERY_TRACE(
                "[parser window] widen to full-history tokenCount ",
                windowTokenCount, " -> ", fullHistoryTokenCount);
            windowTokenCount = fullHistoryTokenCount;
            triedFullHistoryWindow = true;
            continue;
          }
          PEGIUM_RECOVERY_TRACE(
              "[parser window] no credible attempt and cannot widen");
          break;
        }
        PEGIUM_RECOVERY_TRACE("[parser window] widen tokenCount ",
                              windowTokenCount, " -> ", *nextWindowTokenCount);
        windowTokenCount = *nextWindowTokenCount;
      }

      if (!acceptedWindow) {
        PEGIUM_RECOVERY_TRACE("[parser parse] stop after window index=",
                              windowIndex);
        break;
      }

      if (!selectedAttempt.fullMatch) {
        PEGIUM_RECOVERY_TRACE("[parser parse] resume strict after window index=",
                              windowIndex, " parsed=",
                              selectedAttempt.parsedLength, " max=",
                              selectedAttempt.maxCursorOffset);
      }
    }
  }
  utils::throw_if_cancelled(cancelToken);

  result.cst = std::move(selectedAttempt.cst);
  result.fullMatch = selectedAttempt.fullMatch;
  result.parsedLength = selectedAttempt.parsedLength;
  result.lastVisibleCursorOffset =
      selectedAttempt.failureSnapshot.has_value() &&
              !selectedAttempt.failureSnapshot->failureLeafHistory.empty()
          ? selectedAttempt.failureSnapshot->failureLeafHistory.back().endOffset
          : selectedAttempt.lastVisibleCursorOffset;
  result.failureVisibleCursorOffset = failureVisibleCursorOffset;
  result.maxCursorOffset = selectedAttempt.maxCursorOffset;
  result.parseDiagnostics = std::move(selectedAttempt.parseDiagnostics);
  std::optional<RecoveryWindowReport> lastRecoveryWindow;
  if (!selectedWindows.empty()) {
    lastRecoveryWindow = RecoveryWindowReport{
        .beginOffset = selectedWindows.back().beginOffset,
        .maxCursorOffset = selectedWindows.back().maxCursorOffset,
        .backwardTokenCount = selectedWindows.back().tokenCount,
        .forwardTokenCount = selectedWindows.back().tokenCount,
    };
  }
  result.recoveryReport = {
      .hasRecovered = !selectedWindows.empty(),
      .fullRecovered = !selectedWindows.empty() && result.fullMatch,
      .recoveryCount = static_cast<std::uint32_t>(selectedWindows.size()),
      .recoveryWindowsTried = recoveryWindowsTried,
      .strictParseRuns = strictParseRuns,
      .recoveryAttemptRuns = recoveryAttemptRuns,
      .recoveryEdits = selectedAttempt.editCount,
      .lastRecoveryWindow = std::move(lastRecoveryWindow),
  };
  detail::ensure_parse_diagnostic(result.parseDiagnostics, result.cst.get(),
                                  result.parsedLength,
                                  result.failureVisibleCursorOffset,
                                  result.fullMatch);
  if (selectedAttempt.entryRuleMatched) {
    auto matchedNode = detail::findFirstRootMatchingNode(*result.cst, &entryRule);
    if (!matchedNode.has_value()) {
      matchedNode = detail::findFirstMatchingNode(*result.cst, &entryRule);
    }
    if (matchedNode.has_value()) {
      const ValueBuildContext context{
          .references = &result.references,
          .linker = coreServices.references.linker.get(),
          .property = {},
          .diagnostics = &result.parseDiagnostics,
      };
      result.value = entryRule.getValue(*matchedNode, context);
    }
  }

  detail::stepTraceDumpSummary(entryRule.getName(), result.fullMatch,
                               !result.parseDiagnostics.empty(),
                               result.parsedLength, inputSize);
  PEGIUM_RECOVERY_TRACE("[parser result] full=", result.fullMatch,
                        " parsed=", result.parsedLength, "/", inputSize,
                        " diag=", result.parseDiagnostics.size(),
                        " recovered=", result.recoveryReport.hasRecovered,
                        " fullRecovered=", result.recoveryReport.fullRecovered,
                        " windows=", result.recoveryReport.recoveryCount,
                        " edits=", result.recoveryReport.recoveryEdits);
  document.parseResult = std::move(result);
  document.references = document.parseResult.references;
}

ExpectResult PegiumParser::expect(
    std::string_view text, TextOffset offset,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &entryRule = getEntryRule();
  const auto &skipper = getSkipper();
  const auto options = getParseOptions();
  ExpectContext ctx{text, skipper, offset, cancelToken};
  ctx.maxConsecutiveCodepointDeletes = options.maxConsecutiveCodepointDeletes;
  ctx.maxEditsPerAttempt = options.maxRecoveryEditsPerAttempt;
  ctx.maxEditCost = options.maxRecoveryEditCost;
  ctx.skip();

  ExpectResult result;
  result.offset =
      std::min<TextOffset>(offset, static_cast<TextOffset>(text.size()));
  result.reachedAnchor = false;
  if (entryRule.expect(ctx)) {
    result.frontier = std::move(ctx.frontier);
    result.reachedAnchor = ctx.reachedAnchor() || !result.frontier.empty();
  }
  return result;
}

} // namespace pegium::parser
