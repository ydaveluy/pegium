#include <gtest/gtest.h>

#include <algorithm>

#include <requirements/core/CoreModule.hpp>

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

  auto parser = createRequirementsParser();
  const auto summary = pegium::test::run_recovery_probe_batch(
      *parser, samples, "requirements-lang", "requirements");
  pegium::test::expect_recovery_probe_summary(
      "requirements", summary,
      {.total = 3u,
       .withValue = 2u,
       .fullMatch = 2u,
       .recovered = 2u,
       .incomplete = 1u,
       .completeRecovery = 2u});
}

TEST(RequirementsRecoveryProbeBatchTest, ReportsTestsBatchBehavior) {
  const auto samples = tests_probe_samples();
  ASSERT_GE(samples.size(), 1u);

  auto parser = createTestsParser();
  const auto summary =
      pegium::test::run_recovery_probe_batch(*parser, samples, "tests-lang",
                                             "tests");
  pegium::test::expect_recovery_probe_summary(
      "tests", summary,
      {.total = 3u,
       .withValue = 3u,
       .fullMatch = 3u,
       .recovered = 3u,
       .incomplete = 0u,
       .completeRecovery = 3u});
}

TEST(RequirementsRecoveryProbeBatchTest,
     Diff3ConflictingApplicableKeepsRecoveredAlternatives) {
  const auto samples = requirements_probe_samples();
  const auto sample = std::find_if(
      samples.begin(), samples.end(), [](const auto &candidate) {
        return candidate.label ==
               "git-merge/diff3_conflicting_applicable.req";
      });
  ASSERT_NE(sample, samples.end());

  auto parser = createRequirementsParser();
  auto document = pegium::test::parse_document(
      *parser, pegium::test::read_text_file(sample->path),
      pegium::test::make_file_uri(sample->label), "requirements-lang");
  const auto observation =
      pegium::test::observe_recovery_probe(sample->label, *document);
  EXPECT_TRUE(observation.hasValue);
  EXPECT_TRUE(observation.recovered);
  EXPECT_TRUE(observation.fullMatch);
  EXPECT_FALSE(observation.incomplete);

  auto *model =
      dynamic_cast<ast::RequirementModel *>(document->parseResult.value);
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->environments.size(), 2u);
  EXPECT_EQ(model->environments[0]->name, "prod");
  EXPECT_EQ(model->environments[1]->name, "staging");
  ASSERT_EQ(model->requirements.size(), 3u);
  auto *headRequirement = model->requirements[0];
  auto *baseRequirement = model->requirements[1];
  auto *featureRequirement = model->requirements[2];
  ASSERT_NE(headRequirement, nullptr);
  ASSERT_NE(baseRequirement, nullptr);
  ASSERT_NE(featureRequirement, nullptr);
  EXPECT_EQ(headRequirement->name, "login");
  EXPECT_EQ(baseRequirement->name, "login");
  EXPECT_EQ(featureRequirement->name, "login");
  ASSERT_EQ(headRequirement->environments.size(), 2u);
  EXPECT_EQ(headRequirement->environments[0].getRefText(), "prod");
  EXPECT_EQ(headRequirement->environments[1].getRefText(), "staging");
  EXPECT_TRUE(baseRequirement->environments.empty());
  ASSERT_EQ(featureRequirement->environments.size(), 1u);
  EXPECT_EQ(featureRequirement->environments[0].getRefText(), "prod");
}

} // namespace
} // namespace requirements::tests
