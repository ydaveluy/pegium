#include <gtest/gtest.h>

#include <algorithm>

#include <arithmetics/core/ArithmeticParser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

std::vector<pegium::test::NamedSampleFile> probe_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "probe-samples", {".calc"});
}

TEST(ArithmeticsRecoveryProbeBatchTest, ReportsBatchBehavior) {
  const auto samples = probe_samples();
  ASSERT_EQ(samples.size(), 2u);

  parser::ArithmeticParser parser;
  const auto summary = pegium::test::run_recovery_probe_batch(
      parser, samples, "arithmetics", "arithmetics");
  pegium::test::expect_recovery_probe_summary(
      "arithmetics", summary,
      {.total = 2u,
       .withValue = 2u,
       .fullMatch = 2u,
       .recovered = 2u,
       .incomplete = 0u,
       .completeRecovery = 2u});
}

TEST(ArithmeticsRecoveryProbeBatchTest,
     Diff3ConflictingCallKeepsRecoveredRootCallAfterCompleteRecovery) {
  const auto samples = probe_samples();
  const auto sample = std::find_if(
      samples.begin(), samples.end(), [](const auto &candidate) {
        return candidate.label == "git-merge/diff3_conflicting_call.calc";
      });
  ASSERT_NE(sample, samples.end());

  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, pegium::test::read_text_file(sample->path),
      pegium::test::make_file_uri(sample->label), "arithmetics");
  const auto observation =
      pegium::test::observe_recovery_probe(sample->label, *document);
  EXPECT_TRUE(observation.hasValue);
  EXPECT_TRUE(observation.recovered);
  EXPECT_TRUE(observation.fullMatch);
  EXPECT_FALSE(observation.incomplete);

  auto *module = dynamic_cast<ast::Module *>(document->parseResult.value);
  ASSERT_NE(module, nullptr);
  const auto evaluation = std::find_if(
      module->statements.begin(), module->statements.end(),
      [](const auto &statement) {
        const auto *parsedEvaluation =
            dynamic_cast<const ast::Evaluation *>(statement);
        if (parsedEvaluation == nullptr) {
          return false;
        }
        const auto *call =
            dynamic_cast<const ast::FunctionCall *>(
                parsedEvaluation->expression);
        return call != nullptr && call->func.getRefText() == "root" &&
               call->args.size() == 2u;
      });
  ASSERT_NE(evaluation, module->statements.end());
}

} // namespace
} // namespace arithmetics::tests
