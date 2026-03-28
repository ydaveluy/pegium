#include <gtest/gtest.h>

#include <requirements/parser/Parser.hpp>
#include <requirements/services/Module.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>

#include <sstream>

namespace requirements::tests {
namespace {

std::string dump_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    std::ostringstream current;
    current << diagnostic.kind;
    if (diagnostic.element != nullptr) {
      current << ":" << *diagnostic.element;
    }
    if (!diagnostic.message.empty()) {
      current << ":" << diagnostic.message;
    }
    current << "@" << diagnostic.beginOffset << "-" << diagnostic.endOffset;
    dump += current.str();
  }
  return dump;
}

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
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(requirements::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("linking.req"), "requirements-lang",
      "environment prod: \"Production\"\n"
      "environment staging: \"Staging\"\n"
      "req login \"Users can login\" applicable for prod, staging, missing\n");

  ASSERT_NE(document, nullptr);
  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  auto *model =
      dynamic_cast<ast::RequirementModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << "fullMatch=" << parsed.fullMatch
                            << " recovered=" << parsed.recoveryReport.hasRecovered
                            << " parsedLength=" << parsed.parsedLength << " "
                            << parseDump;
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

} // namespace
} // namespace requirements::tests
