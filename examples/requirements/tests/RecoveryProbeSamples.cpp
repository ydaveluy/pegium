#include <gtest/gtest.h>

#include <algorithm>

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
       .withValue = 2u,
       .fullMatch = 1u,
       .recovered = 2u,
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
       .withValue = 1u,
       .fullMatch = 1u,
       .recovered = 1u,
       .incomplete = 3u,
       .completeRecovery = 1u});
}

TEST(RequirementsRecoveryProbeBatchTest,
     Diff3ConflictingApplicableRetainsRecoveredRequirementModel) {
  const auto samples = requirements_probe_samples();
  const auto sample = std::find_if(
      samples.begin(), samples.end(), [](const auto &candidate) {
        return candidate.label ==
               "git-merge/diff3_conflicting_applicable.req";
      });
  ASSERT_NE(sample, samples.end());

  parser::RequirementsParser parser;
  auto document = pegium::test::parse_document(
      parser, pegium::test::read_text_file(sample->path),
      pegium::test::make_file_uri(sample->label), "requirements-lang");
  const auto observation =
      pegium::test::observe_recovery_probe(sample->label, *document);
  EXPECT_TRUE(observation.hasValue);
  EXPECT_TRUE(observation.recovered);
  EXPECT_FALSE(observation.fullMatch);
  EXPECT_TRUE(observation.incomplete);

  auto *model =
      dynamic_cast<ast::RequirementModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->environments.size(), 2u);
  EXPECT_EQ(model->environments[0]->name, "prod");
  EXPECT_EQ(model->environments[1]->name, "staging");
  ASSERT_EQ(model->requirements.size(), 1u);
  auto *requirement = model->requirements.front().get();
  ASSERT_NE(requirement, nullptr);
  EXPECT_EQ(requirement->name, "login");
  ASSERT_EQ(requirement->environments.size(), 2u);
  EXPECT_EQ(requirement->environments[0].getRefText(), "prod");
  EXPECT_EQ(requirement->environments[1].getRefText(), "staging");
}

} // namespace
} // namespace requirements::tests
