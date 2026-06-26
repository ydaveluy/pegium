#pragma once

#include <string_view>

#include <gtest/gtest.h>

#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

namespace pegium::test {

inline pegium::CstJsonConversionOptions recovery_cst_json_options() {
  return {
      .includeText = true,
      .includeGrammarSource = true,
      .includeHidden = false,
      .includeRecovered = true,
  };
}

template <typename Result> struct RecoveryHarnessResult {
  Result result;
};

template <typename RuleType>
auto StrictRecoveryDocument(const RuleType &entryRule, std::string_view text,
                            const parser::Skipper &skipper,
                            const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessResult<parser::detail::StrictParseResult> harness;
  const auto snapshot = text::TextSnapshot::copy(text);
  harness.result = parser::detail::run_strict_parse(entryRule, skipper,
                                                    snapshot, cancelToken);
  return harness;
}

template <typename RuleType>
auto FailureAnalysisDocument(const RuleType &entryRule, std::string_view text,
                             const parser::Skipper &skipper,
                             const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessResult<parser::detail::StrictFailureEngineResult>
      failureHarness;
  const auto snapshot = text::TextSnapshot::copy(text);
  failureHarness.result = parser::detail::run_strict_parse_with_failure_snapshot(
      entryRule, skipper, snapshot, cancelToken);
  return failureHarness;
}

} // namespace pegium::test
