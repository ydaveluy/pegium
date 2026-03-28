#include <pegium/core/parser/PegiumParser.hpp>

#include <optional>

#include <pegium/core/parser/AssignmentHelpers.hpp>
#include <pegium/core/parser/CstSearch.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::parser {

namespace {

struct StandaloneCoreServices {
  pegium::SharedCoreServices shared;
  pegium::CoreServices core;

  StandaloneCoreServices() : core(shared) {}
};

const pegium::CoreServices &standalone_core_services() noexcept {
  static const StandaloneCoreServices services = [] {
    StandaloneCoreServices services;
    pegium::installDefaultSharedCoreServices(services.shared);
    pegium::installDefaultCoreServices(services.core);
    return services;
  }();
  return services.core;
}

} // namespace

PegiumParser::PegiumParser() noexcept
    : pegium::DefaultCoreService(standalone_core_services()) {}

ParseResult PegiumParser::parse(text::TextSnapshot text,
                                const utils::CancellationToken &cancelToken) const {
  const auto &entryRule = getEntryRule();
  const auto &skipper = getSkipper();
  const ParseOptions options = getParseOptions();
  ParseResult result;
  const TextOffset inputSize = static_cast<TextOffset>(text.size());
  auto recoverySearch =
      detail::run_recovery_search(entryRule, skipper, options, text, cancelToken);
  auto &selectedAttempt = recoverySearch.selectedAttempt;
  auto &selectedWindows = recoverySearch.selectedWindows;
  const auto failureVisibleCursorOffset =
      recoverySearch.failureVisibleCursorOffset;
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
  auto syntaxDiagnostics = selectedAttempt.recoveryEdits;
  std::optional<RecoveryWindowReport> lastRecoveryWindow;
  if (!selectedWindows.empty()) {
    lastRecoveryWindow = RecoveryWindowReport{
        .beginOffset = selectedWindows.back().beginOffset,
        .maxCursorOffset = selectedWindows.back().maxCursorOffset,
        .backwardTokenCount = selectedWindows.back().tokenCount,
        .forwardTokenCount = selectedWindows.back().forwardTokenCount,
    };
  }
  result.recoveryReport = {
      .hasRecovered = !selectedWindows.empty(),
      .fullRecovered = !selectedWindows.empty() && result.fullMatch,
      .recoveryCount = static_cast<std::uint32_t>(selectedWindows.size()),
      .recoveryWindowsTried = recoverySearch.recoveryWindowsTried,
      .strictParseRuns = recoverySearch.strictParseRuns,
      .recoveryAttemptRuns = recoverySearch.recoveryAttemptRuns,
      .recoveryEdits = selectedAttempt.editCount,
      .lastRecoveryWindow = std::move(lastRecoveryWindow),
  };
  if (!syntaxDiagnostics.empty() || !result.fullMatch ||
      result.recoveryReport.hasRecovered) {
    detail::append_syntax_summary_entry(syntaxDiagnostics, result.cst.get(),
                                        result.parsedLength,
                                        result.lastVisibleCursorOffset,
                                        result.failureVisibleCursorOffset,
                                        inputSize,
                                        result.recoveryReport.hasRecovered,
                                        result.fullMatch);
  }
  result.parseDiagnostics =
      detail::materialize_syntax_diagnostics(syntaxDiagnostics);
  if (selectedAttempt.entryRuleMatched) {
    auto matchedNode = detail::findFirstRootMatchingNode(*result.cst, &entryRule);
    if (!matchedNode.has_value()) {
      matchedNode = detail::findFirstMatchingNode(*result.cst, &entryRule);
    }
    if (matchedNode.has_value()) {
      const ValueBuildContext context{
          .references = &result.references,
          .linker = services.references.linker.get(),
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
  return result;
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
