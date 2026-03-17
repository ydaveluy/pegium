#include <gtest/gtest.h>

#include <algorithm>

#include <domainmodel/services/Module.hpp>

#include "../src/lsp/DomainModelFormatter.hpp"
#include "../src/lsp/DomainModelRenameProvider.hpp"
#include "../src/references/DomainModelScopeComputation.hpp"
#include "../src/references/QualifiedNameProvider.hpp"

#include <pegium/ExampleTestSupport.hpp>
#include <pegium/services/Services.hpp>

namespace domainmodel::tests {
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

TEST(DomainModelModuleTest, InstallsLanguageSpecificOverrides) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      domainmodel::services::create_language_services(*shared, "domain-model");

  ASSERT_NE(languageServices, nullptr);
  EXPECT_NE(dynamic_cast<domainmodel::services::references::DomainModelScopeComputation *>(
                languageServices->references.scopeComputation.get()),
            nullptr);
  EXPECT_NE(dynamic_cast<domainmodel::services::lsp::DomainModelRenameProvider *>(
                languageServices->lsp.renameProvider.get()),
            nullptr);
  EXPECT_NE(dynamic_cast<domainmodel::services::lsp::DomainModelFormatter *>(
                languageServices->lsp.formatter.get()),
            nullptr);

  domainmodel::services::references::QualifiedNameProvider qualifiedNames;
  domainmodel::ast::PackageDeclaration parent;
  parent.name = "blog";
  domainmodel::ast::PackageDeclaration child;
  child.name = "internal";
  child.setContainer<domainmodel::ast::PackageDeclaration,
                     &domainmodel::ast::PackageDeclaration::elements>(parent,
                                                                        0);
  EXPECT_EQ(qualifiedNames.getQualifiedName(child, "Author"), "blog.internal.Author");
}

TEST(DomainModelModuleTest, ValidatorWarnsOnLowerCaseTypeName) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(domainmodel::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("domainmodel-module.dmodel"),
      "domain-model", "entity person {}");

  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(
      pegium::test::has_diagnostic_message(*document, "Type name should start"));
}

TEST(DomainModelModuleTest, FormatterFormatsCompactModel) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      domainmodel::services::create_language_services(*shared, "domain-model");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(
      shared->serviceRegistry->registerServices(std::move(languageServices)));
  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("formatting.dmodel"), "domain-model",
      "package foo.bar { datatype Complex entity E2 extends E1 { next: E2 other: Complex }}");
  ASSERT_NE(document, nullptr);
  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("domain-model");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 4;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(
      apply_text_edits(*document, edits),
      "package foo.bar {\n"
      "    datatype Complex\n"
      "    entity E2 extends E1 {\n"
      "        next: E2\n"
      "        other: Complex\n"
      "    }\n"
      "}");
}

TEST(DomainModelModuleTest, FormatterPreservesCommentsWhileReindenting) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      domainmodel::services::create_language_services(*shared, "domain-model");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(
      shared->serviceRegistry->registerServices(std::move(languageServices)));
  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("formatting-comments.dmodel"),
      "domain-model",
      "package foo.bar {\n"
      "// package comment\n"
      "entity E1 {\n"
      "// feature comment\n"
      "next:E1\n"
      "}\n"
      "}");
  ASSERT_NE(document, nullptr);
  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("domain-model");
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
  EXPECT_EQ(
      apply_text_edits(*document, edits),
      "package foo.bar {\n"
      "  // package comment\n"
      "  entity E1 {\n"
      "    // feature comment\n"
      "    next: E1\n"
      "  }\n"
      "}");
}

} // namespace
} // namespace domainmodel::tests
