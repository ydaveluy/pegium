#include <gtest/gtest.h>

#include <statemachine/parser/Parser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace statemachine::tests {
namespace {

std::vector<pegium::test::NamedSampleFile> probe_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "probe-samples",
      {".statemachine"});
}

TEST(StatemachineRecoveryProbeBatchTest, ReportsBatchBehavior) {
  const auto samples = probe_samples();
  ASSERT_GE(samples.size(), 1u);

  parser::StateMachineParser parser;
  const auto summary = pegium::test::run_recovery_probe_batch(
      parser, samples, "statemachine", "statemachine");
  pegium::test::expect_recovery_probe_summary(
      "statemachine", summary,
      {.total = 9u,
       .withValue = 8u,
       .fullMatch = 3u,
       .recovered = 3u,
       .incomplete = 6u,
       .completeRecovery = 3u});
}

} // namespace
} // namespace statemachine::tests
