#include <gtest/gtest.h>

#include <requirements/parser/Parser.hpp>
#include <requirements/services/Module.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace requirements::tests {
namespace {

template <typename References>
std::string summarize_reference_texts(const References &references) {
  std::string summary;
  for (const auto &reference : references) {
    if (!summary.empty()) {
      summary += ", ";
    }
    summary += reference.getRefText();
  }
  return summary;
}

enum class SampleLanguage : std::uint8_t {
  Requirements,
  Tests,
};

struct RecoverySampleCase {
  pegium::test::NamedSampleFile file;
  SampleLanguage language = SampleLanguage::Requirements;
};

void PrintTo(const RecoverySampleCase &sample, std::ostream *os) {
  *os << sample.file.label;
}

void validate_requirements_sample(const RecoverySampleCase &sample,
                                  const pegium::workspace::Document &document) {
  const auto &parsed = document.parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);

  if (sample.language == SampleLanguage::Requirements) {
    auto *model = dynamic_cast<const ast::RequirementModel *>(parsed.value.get());
    ASSERT_NE(model, nullptr) << sample.file.label << " :: " << parseDump;

    if (sample.file.label == "requirements/extra_contact_colon.req") {
      ASSERT_NE(model->contact, nullptr);
      EXPECT_EQ(model->contact->userName, "team");
      ASSERT_EQ(model->environments.size(), 1u);
      ASSERT_EQ(model->requirements.size(), 1u);
      return;
    }

    if (sample.file.label == "requirements/missing_environment_colon.req") {
      ASSERT_EQ(model->environments.size(), 1u);
      ASSERT_EQ(model->requirements.size(), 1u);
      return;
    }

    if (sample.file.label == "requirements/extra_environment_colon.req") {
      ASSERT_EQ(model->environments.size(), 1u);
      ASSERT_EQ(model->requirements.size(), 1u);
      auto *environment = model->environments.front().get();
      ASSERT_NE(environment, nullptr);
      EXPECT_EQ(environment->name, "prod");
      EXPECT_EQ(environment->description, "Production");
      return;
    }

    if (sample.file.label == "requirements/missing_contact_colon.req") {
      ASSERT_NE(model->contact, nullptr);
      EXPECT_EQ(model->contact->userName, "team");
      ASSERT_EQ(model->requirements.size(), 1u);
      return;
    }

    if (sample.file.label == "requirements/extra_applicable_comma.req" ||
        sample.file.label == "requirements/missing_applicable_comma.req") {
      ASSERT_EQ(model->requirements.size(), 1u);
      auto *requirement = model->requirements.front().get();
      ASSERT_NE(requirement, nullptr);
      EXPECT_EQ(requirement->name, "login");
      ASSERT_EQ(requirement->environments.size(), 2u)
          << sample.file.label << " :: " << parseDump << " :: "
          << summarize_reference_texts(requirement->environments);
      EXPECT_EQ(requirement->environments[0].getRefText(), "prod");
      EXPECT_EQ(requirement->environments[1].getRefText(), "staging");
      return;
    }

    if (sample.file.label == "requirements/long_garbage_prefix.req") {
      ASSERT_EQ(model->requirements.size(), 1u);
      auto *requirement = model->requirements.front().get();
      ASSERT_NE(requirement, nullptr);
      EXPECT_EQ(requirement->name, "login");
      return;
    }

    if (sample.file.label ==
        "requirements/git-merge/conflicting_environments.req") {
      ASSERT_EQ(model->requirements.size(), 1u);
      auto *requirement = model->requirements.front().get();
      ASSERT_NE(requirement, nullptr);
      EXPECT_EQ(requirement->name, "login");
      return;
    }

    return;
  }

  auto *model = dynamic_cast<const ast::TestModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << sample.file.label << " :: " << parseDump;

  if (sample.file.label == "tests/extra_contact_colon.tst") {
    ASSERT_NE(model->contact, nullptr);
    EXPECT_EQ(model->contact->userName, "qa");
    ASSERT_EQ(model->tests.size(), 1u);
    return;
  }

  if (sample.file.label == "tests/missing_testfile_equals.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    ASSERT_TRUE(test->testFile.has_value());
    EXPECT_EQ(*test->testFile, "qa.spec");
    EXPECT_EQ(test->requirements.size(), 1u);
    return;
  }

  if (sample.file.label == "tests/extra_testfile_equals.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    ASSERT_TRUE(test->testFile.has_value());
    EXPECT_EQ(*test->testFile, "qa.spec");
    ASSERT_EQ(test->requirements.size(), 1u);
    EXPECT_EQ(test->requirements[0].getRefText(), "Req1");
    return;
  }

  if (sample.file.label == "tests/missing_contact_colon.tst") {
    ASSERT_NE(model->contact, nullptr);
    EXPECT_EQ(model->contact->userName, "qa");
    ASSERT_EQ(model->tests.size(), 1u);
    return;
  }

  if (sample.file.label == "tests/malformed_optional_contact_header.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    return;
  }

  if (sample.file.label == "tests/missing_test_name.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->requirements.size(), 1u);
    EXPECT_EQ(test->requirements[0].getRefText(), "Req1");
    return;
  }

  if (sample.file.label == "tests/extra_requirements_comma.tst" ||
      sample.file.label == "tests/missing_requirements_comma.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    ASSERT_EQ(test->requirements.size(), 2u)
        << sample.file.label << " :: " << parseDump << " :: "
        << summarize_reference_texts(test->requirements);
    EXPECT_EQ(test->requirements[0].getRefText(), "Req1");
    EXPECT_EQ(test->requirements[1].getRefText(), "Req2");
    return;
  }

  if (sample.file.label == "tests/missing_applicable_comma.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->requirements.size(), 1u);
    ASSERT_EQ(test->environments.size(), 2u);
    EXPECT_EQ(test->environments[0].getRefText(), "prod");
    EXPECT_EQ(test->environments[1].getRefText(), "staging");
    return;
  }

  if (sample.file.label == "tests/long_garbage_prefix.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    return;
  }

  if (sample.file.label == "tests/multiple_broken_header_and_test.tst") {
    ASSERT_EQ(model->tests.size(), 1u);
    auto *test = model->tests.front().get();
    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->name, "T1");
    ASSERT_TRUE(test->testFile.has_value());
    EXPECT_EQ(*test->testFile, "qa.spec");
    ASSERT_EQ(test->requirements.size(), 2u)
        << sample.file.label << " :: " << parseDump << " :: "
        << summarize_reference_texts(test->requirements);
  }
}

std::vector<RecoverySampleCase> recovery_samples() {
  const auto root =
      pegium::test::current_source_directory() / "recovery-samples";
  std::vector<RecoverySampleCase> files;
  for (const auto &file :
       pegium::test::collect_named_sample_files(root, {".req", ".tst"})) {
    files.push_back(
        {.file = file,
         .language = file.path.extension() == ".tst" ? SampleLanguage::Tests
                                                     : SampleLanguage::Requirements});
  }
  return files;
}

class RequirementsRecoverySampleTest
    : public ::testing::TestWithParam<RecoverySampleCase> {};

TEST(RequirementsRecoverySampleCorpusTest, IsNotEmpty) {
  EXPECT_FALSE(recovery_samples().empty());
}

TEST_P(RequirementsRecoverySampleTest, RecoversCompletely) {
  const auto &sample = GetParam();
  parser::RequirementsParser requirementsParser;
  parser::TestsParser testsParser;
  std::shared_ptr<pegium::workspace::Document> document;

  if (sample.language == SampleLanguage::Requirements) {
    document = pegium::test::parse_document(
        requirementsParser, pegium::test::read_text_file(sample.file.path),
        pegium::test::make_file_uri(sample.file.label), "requirements-lang");
  } else {
    document = pegium::test::parse_document(
        testsParser, pegium::test::read_text_file(sample.file.path),
        pegium::test::make_file_uri(sample.file.label), "tests-lang");
  }

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << sample.file.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << sample.file.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered)
      << sample.file.label << " :: " << parseDump;
  EXPECT_FALSE(parsed.parseDiagnostics.empty())
      << sample.file.label << " :: " << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << sample.file.label << " :: " << parseDump;
  validate_requirements_sample(sample, *document);
}

INSTANTIATE_TEST_SUITE_P(
    RecoverySamples, RequirementsRecoverySampleTest,
    ::testing::ValuesIn(recovery_samples()),
    [](const ::testing::TestParamInfo<RecoverySampleCase> &info) {
      return info.param.file.testName;
    });

} // namespace
} // namespace requirements::tests
