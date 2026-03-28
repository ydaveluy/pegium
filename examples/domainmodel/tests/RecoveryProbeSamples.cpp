#include <gtest/gtest.h>

#include <domainmodel/parser/Parser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace domainmodel::tests {
namespace {

std::vector<pegium::test::NamedSampleFile> probe_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "probe-samples", {".dmodel"});
}

TEST(DomainModelRecoveryProbeBatchTest, ReportsBatchBehavior) {
  const auto samples = probe_samples();
  ASSERT_GE(samples.size(), 2u);

  parser::DomainModelParser parser;
  const auto summary = pegium::test::run_recovery_probe_batch(
      parser, samples, "domain-model", "domainmodel");
  pegium::test::expect_recovery_probe_summary(
      "domainmodel", summary,
      {.total = 5u,
       .withValue = 1u,
       .fullMatch = 1u,
       .recovered = 1u,
       .incomplete = 4u,
       .completeRecovery = 1u});
}

} // namespace
} // namespace domainmodel::tests
