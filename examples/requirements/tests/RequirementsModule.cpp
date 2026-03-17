#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>

#include <requirements/services/Module.hpp>

#include "../src/lsp/RequirementsFormatter.hpp"
#include "../src/validation/RequirementsValidator.hpp"
#include "../src/validation/TestsValidator.hpp"

#include <pegium/ExampleTestSupport.hpp>
#include <pegium/services/Services.hpp>

namespace requirements::tests {
namespace {

std::string apply_text_edits(const pegium::workspace::Document &document,
                             std::vector<::lsp::TextEdit> edits) {
  auto text = document.text();
  std::sort(edits.begin(), edits.end(),
            [&document](const auto &left, const auto &right) {
              return document.positionToOffset(left.range.start) >
                     document.positionToOffset(right.range.start);
            });
  for (const auto &edit : edits) {
    const auto begin = document.positionToOffset(edit.range.start);
    const auto end = document.positionToOffset(edit.range.end);
    text.replace(begin, end - begin, edit.newText);
  }
  return text;
}

TEST(RequirementsModuleTest, SplitsValidationIntoDedicatedClasses) {
  static_assert(std::is_class_v<
                requirements::services::validation::RequirementsValidator>);
  static_assert(std::is_class_v<
                requirements::services::validation::TestsValidator>);
  SUCCEED();
}

TEST(RequirementsModuleTest, InstallsRequirementsAndTestsFormatters) {
  auto shared = pegium::test::make_shared_services();
  auto requirementsServices =
      requirements::services::create_requirements_language_services(
          *shared, "requirements-lang");
  auto testsServices = requirements::services::create_tests_language_services(
      *shared, "tests-lang");

  ASSERT_NE(requirementsServices, nullptr);
  ASSERT_NE(testsServices, nullptr);
  EXPECT_NE(dynamic_cast<requirements::services::lsp::RequirementsFormatter *>(
                requirementsServices->lsp.formatter.get()),
            nullptr);
  EXPECT_NE(dynamic_cast<requirements::services::lsp::TestsFormatter *>(
                testsServices->lsp.formatter.get()),
            nullptr);
}

TEST(RequirementsModuleTest, RegistersRequirementsAndTestsLanguages) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(requirements::services::register_language_services(*shared));

  ASSERT_NE(shared->serviceRegistry->getServicesByLanguageId("requirements-lang"),
            nullptr);
  ASSERT_NE(shared->serviceRegistry->getServicesByLanguageId("tests-lang"),
            nullptr);
  EXPECT_NE(shared->serviceRegistry->getServices(
                pegium::test::make_file_uri("model.req")),
            nullptr);
  EXPECT_NE(shared->serviceRegistry->getServices(
                pegium::test::make_file_uri("suite.tst")),
            nullptr);
}

TEST(RequirementsModuleTest, RequirementsValidatorPublishesExpectedWarnings) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(requirements::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("requirements-validator.req"),
      "requirements-lang", "req Login \"Users can login\"\n");

  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(pegium::test::has_diagnostic_message(
      *document, "Requirement name Login should contain a number."));
  EXPECT_TRUE(pegium::test::has_diagnostic_message(
      *document, "Requirement Login not covered by a test."));
}

TEST(RequirementsModuleTest, TestsValidatorPublishesEnvironmentWarnings) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(requirements::services::register_language_services(*shared));

  auto requirementsDocument = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("requirements-model.req"),
      "requirements-lang",
      "environment prod: \"Production\"\n"
      "environment staging: \"Staging\"\n"
      "req Req1 \"Users can login\" applicable for prod\n");
  ASSERT_NE(requirementsDocument, nullptr);

  auto testsDocument = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("tests-model.tst"), "tests-lang",
      "tst T1 tests Req1 applicable for staging\n");

  ASSERT_NE(testsDocument, nullptr);
  EXPECT_TRUE(pegium::test::has_diagnostic_message(
      *testsDocument,
      "Test T1 references environment staging which is not used by any "
      "referenced requirement."));
}

TEST(RequirementsModuleTest, RequirementsFormatterFormatsCompactModel) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      requirements::services::create_requirements_language_services(
          *shared, "requirements-lang");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("requirements-format.req"),
      "requirements-lang",
      "contact:\"team\" environment prod:\"Production\" environment staging:\"Staging\" "
      "req Req1 \"Users can login\" applicable for prod,staging");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("requirements-lang");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "contact: \"team\"\n"
            "environment prod: \"Production\"\n"
            "environment staging: \"Staging\"\n"
            "req Req1 \"Users can login\" applicable for prod, staging");
}

TEST(RequirementsModuleTest, TestsFormatterFormatsCompactModel) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices = requirements::services::create_tests_language_services(
      *shared, "tests-lang");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("tests-format.tst"), "tests-lang",
      "contact:\"qa\" tst T1 testfile=\"suite.ts\" tests Req1,Req2 applicable for prod,staging");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("tests-lang");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "contact: \"qa\"\n"
            "tst T1 testfile = \"suite.ts\" tests Req1, Req2 applicable for prod, staging");
}

} // namespace
} // namespace requirements::tests
