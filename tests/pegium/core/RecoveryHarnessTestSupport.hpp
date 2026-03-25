#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

namespace pegium::test {

inline converter::CstJsonConversionOptions recovery_cst_json_options() {
  return {
      .includeText = true,
      .includeGrammarSource = true,
      .includeHidden = false,
      .includeRecovered = true,
  };
}

template <typename Result>
struct RecoveryHarnessResult {
  Result result;
};

template <typename RuleType>
auto StrictRecoveryDocument(const RuleType &entryRule, std::string_view text,
                            const parser::Skipper &skipper,
                            const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessResult<parser::detail::StrictParseResult> harness;
  const auto snapshot = text::TextSnapshot::copy(text);
  harness.result = parser::detail::run_strict_parse(
      entryRule, skipper, snapshot, cancelToken);
  return harness;
}

template <typename RuleType>
auto FailureAnalysisDocument(
    const RuleType &entryRule, std::string_view text,
    const parser::Skipper &skipper,
    const utils::CancellationToken &cancelToken = {}) {
  auto harness = StrictRecoveryDocument(entryRule, text, skipper, cancelToken);
  RecoveryHarnessResult<parser::detail::FailureAnalysisResult> failureHarness;
  const auto snapshot = text::TextSnapshot::copy(text);
  failureHarness.result = parser::detail::analyze_failure(
      entryRule, skipper, snapshot, harness.result.summary, cancelToken);
  return failureHarness;
}

} // namespace pegium::test
