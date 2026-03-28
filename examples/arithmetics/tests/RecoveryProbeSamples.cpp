#include <gtest/gtest.h>

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
       .fullMatch = 0u,
       .recovered = 0u,
       .incomplete = 3u,
       .completeRecovery = 0u});
}

} // namespace
} // namespace arithmetics::tests
