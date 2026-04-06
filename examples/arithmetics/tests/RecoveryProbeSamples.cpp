#include <gtest/gtest.h>

#include <algorithm>

#include <arithmetics/parser/Parser.hpp>

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
  ASSERT_EQ(samples.size(), 3u);

  parser::ArithmeticParser parser;
  const auto summary = pegium::test::run_recovery_probe_batch(
      parser, samples, "arithmetics", "arithmetics");
  pegium::test::expect_recovery_probe_summary(
      "arithmetics", summary,
      {.total = 3u,
       .withValue = 3u,
       .fullMatch = 1u,
       .recovered = 1u,
       .incomplete = 2u,
       .completeRecovery = 1u});
}

TEST(ArithmeticsRecoveryProbeBatchTest,
     Diff3ConflictingCallRecoversCompletely) {
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
  EXPECT_TRUE(pegium::test::is_complete_recovery(observation));

  auto *module = dynamic_cast<ast::Module *>(document->parseResult.value.get());
  ASSERT_NE(module, nullptr);
  const auto evaluation = std::find_if(
      module->statements.begin(), module->statements.end(),
      [](const auto &statement) {
        const auto *parsedEvaluation =
            dynamic_cast<const ast::Evaluation *>(statement.get());
        if (parsedEvaluation == nullptr) {
          return false;
        }
        const auto *call =
            dynamic_cast<const ast::FunctionCall *>(
                parsedEvaluation->expression.get());
        return call != nullptr && call->func.getRefText() == "root" &&
               call->args.size() == 2u;
      });
  ASSERT_NE(evaluation, module->statements.end());
}

} // namespace
} // namespace arithmetics::tests
