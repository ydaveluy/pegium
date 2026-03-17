#include <gtest/gtest.h>

#include <requirements/parser/Parser.hpp>
#include <requirements/services/Module.hpp>

#include <pegium/ExampleTestSupport.hpp>

namespace requirements::tests {
namespace {

TEST(RequirementsLanguageTest, ParsesRequirementModel) {
  parser::RequirementsParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "contact: \"team\"\n"
      "environment prod: \"Production\"\n"
      "req login \"Users can login\"\n",
      pegium::test::make_file_uri("requirements.req"), "requirements-lang");

  ASSERT_TRUE(document->parseSucceeded());
  auto *model =
      dynamic_cast<ast::RequirementModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->requirements.size(), 1u);
}

TEST(RequirementsLanguageTest, LinksEnvironmentMultiReferencesAndReportsUnresolvedOnes) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(requirements::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("linking.req"), "requirements-lang",
      "environment prod: \"Production\"\n"
      "environment staging: \"Staging\"\n"
      "req login \"Users can login\" applicable for prod, staging, missing\n");

  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<ast::RequirementModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->requirements.size(), 1u);

  auto *requirement = model->requirements.front().get();
  ASSERT_NE(requirement, nullptr);
  ASSERT_EQ(requirement->environments.size(), 3u);

  ASSERT_TRUE(requirement->environments[0]);
  ASSERT_TRUE(requirement->environments[1]);
  EXPECT_EQ(requirement->environments[0]->name, "prod");
  EXPECT_EQ(requirement->environments[1]->name, "staging");

  EXPECT_FALSE(requirement->environments[2]);
  EXPECT_TRUE(requirement->environments[2].hasError());
  EXPECT_NE(requirement->environments[2].getErrorMessage().find("missing"),
            std::string::npos);
  EXPECT_TRUE(pegium::test::has_diagnostic_message(*document,
                                                   "Unresolved reference: missing"));
}

TEST(RequirementsLanguageTest,
     RecoveryDeletesUnexpectedColonInOptionalContactHeader) {
  parser::RequirementsParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "contact:: \"team\"\n"
      "environment prod: \"Production\"\n"
      "req login \"Users can login\"\n",
      pegium::test::make_file_uri("requirements-recovery.req"),
      "requirements-lang");

  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<ast::RequirementModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->environments.size(), 1u);
  ASSERT_EQ(model->requirements.size(), 1u);
  EXPECT_TRUE(document->parseResult.recoveryReport.hasRecovered);
  EXPECT_FALSE(document->parseResult.parseDiagnostics.empty());
}

TEST(RequirementsLanguageTest,
     TestsParserRecoversOptionalContactHeaderBeforeTests) {
  parser::TestsParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "contact:: \"qa\"\n"
      "tst T1 tests Req1\n",
      pegium::test::make_file_uri("tests-recovery.tst"), "tests-lang");

  ASSERT_NE(document, nullptr);
  auto *model = dynamic_cast<ast::TestModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->tests.size(), 1u);
  EXPECT_TRUE(document->parseResult.recoveryReport.hasRecovered);
  EXPECT_FALSE(document->parseResult.parseDiagnostics.empty());
}

TEST(RequirementsLanguageTest,
     TestsParserRecoversMalformedOptionalContactHeaderBeforeTests) {
  parser::TestsParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "conttact:: \"qa\"\n"
      "tst T1 tests Req1\n",
      pegium::test::make_file_uri("tests-recovery-double-contact.tst"),
      "tests-lang");

  ASSERT_NE(document, nullptr);
  auto *model = dynamic_cast<ast::TestModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->tests.size(), 1u);
  EXPECT_TRUE(document->parseResult.recoveryReport.hasRecovered);
  EXPECT_FALSE(document->parseResult.parseDiagnostics.empty());
}

} // namespace
} // namespace requirements::tests
