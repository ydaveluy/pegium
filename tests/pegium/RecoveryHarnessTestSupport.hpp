#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/parser/RecoveryAnalysis.hpp>
#include <pegium/workspace/Document.hpp>

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
struct RecoveryHarnessDocument {
  std::unique_ptr<workspace::Document> document;
  Result result;
};

template <typename RuleType>
auto StrictRecoveryDocument(const RuleType &entryRule, std::string_view text,
                            const parser::Skipper &skipper,
                            const utils::CancellationToken &cancelToken = {}) {
  RecoveryHarnessDocument<parser::detail::StrictParseResult> harness;
  harness.document = std::make_unique<workspace::Document>();
  harness.document->setText(std::string{text});
  harness.result = parser::detail::run_strict_parse(
      entryRule, skipper, *harness.document, cancelToken);
  return harness;
}

template <typename RuleType>
auto FailureAnalysisDocument(
    const RuleType &entryRule, std::string_view text,
    const parser::Skipper &skipper,
    const utils::CancellationToken &cancelToken = {}) {
  auto harness = StrictRecoveryDocument(entryRule, text, skipper, cancelToken);
  RecoveryHarnessDocument<parser::detail::FailureAnalysisResult> failureHarness;
  failureHarness.document = std::make_unique<workspace::Document>();
  failureHarness.document->setText(std::string{text});
  failureHarness.result = parser::detail::analyze_failure(
      entryRule, skipper, *failureHarness.document, harness.result.summary,
      cancelToken);
  return failureHarness;
}

} // namespace pegium::test
