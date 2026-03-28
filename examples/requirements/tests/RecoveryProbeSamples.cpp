#include <gtest/gtest.h>

#include <requirements/parser/Parser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace requirements::tests {
namespace {

std::vector<pegium::test::NamedSampleFile> requirements_probe_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "probe-samples/requirements",
      {".req"});
}

std::vector<pegium::test::NamedSampleFile> tests_probe_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "probe-samples/tests",
      {".tst"});
}

TEST(RequirementsRecoveryProbeBatchTest, ReportsRequirementsBatchBehavior) {
  const auto samples = requirements_probe_samples();
  ASSERT_GE(samples.size(), 1u);

  parser::RequirementsParser parser;
  const auto summary = pegium::test::run_recovery_probe_batch(
      parser, samples, "requirements-lang", "requirements");
  pegium::test::expect_recovery_probe_summary(
      "requirements", summary,
      {.total = 4u,
       .withValue = 1u,
       .fullMatch = 1u,
       .recovered = 1u,
       .incomplete = 3u,
       .completeRecovery = 1u});
}

TEST(RequirementsRecoveryProbeBatchTest, ReportsTestsBatchBehavior) {
  const auto samples = tests_probe_samples();
  ASSERT_GE(samples.size(), 1u);

  parser::TestsParser parser;
  const auto summary =
      pegium::test::run_recovery_probe_batch(parser, samples, "tests-lang",
                                             "tests");
  pegium::test::expect_recovery_probe_summary(
      "tests", summary,
      {.total = 4u,
       .withValue = 0u,
       .fullMatch = 0u,
       .recovered = 0u,
       .incomplete = 4u,
       .completeRecovery = 0u});
}

} // namespace
} // namespace requirements::tests
