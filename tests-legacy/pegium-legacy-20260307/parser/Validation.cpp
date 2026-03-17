#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/validation/DefaultDocumentValidator.hpp>
#include <pegium/validation/DefaultValidationRegistry.hpp>
#include <pegium/validation/DocumentValidator.hpp>
#include <pegium/validation/ValidationRegistry.hpp>
#include <pegium/workspace/Document.hpp>

namespace {

struct ValidationNodeA : pegium::AstNode {};
struct ValidationNodeB : pegium::AstNode {};
struct ValidationRefNode : pegium::AstNode {
  reference<ValidationNodeA> ref;
};

} // namespace

TEST(ValidationTest, RegistryFiltersChecksByTypeAndCategory) {
  pegium::validation::DefaultValidationRegistry registry;

  std::size_t nodeAFastCalls = 0;
  std::size_t nodeBFastCalls = 0;
  std::size_t astSlowCalls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&nodeAFastCalls](const ValidationNodeA &,
                        const pegium::validation::ValidationAcceptor &) {
        ++nodeAFastCalls;
      },
      "fast");
  registry.registerCheck<ValidationNodeB>(
      [&nodeBFastCalls](const ValidationNodeB &,
                        const pegium::validation::ValidationAcceptor &) {
        ++nodeBFastCalls;
      },
      "fast");
  registry.registerCheck<pegium::AstNode>(
      [&astSlowCalls](const pegium::AstNode &,
                      const pegium::validation::ValidationAcceptor &) {
        ++astSlowCalls;
      },
      "slow");

  const auto noopAcceptor =
      [](pegium::services::DiagnosticSeverity, std::string_view,
         pegium::TextOffset, pegium::TextOffset,
         std::optional<std::string_view>,
         const std::optional<::lsp::LSPAny> &) {};

  ValidationNodeA nodeA;

  const auto fastChecks = pegium::utils::collect(
      registry.getChecks(nodeA, std::vector<std::string>{"fast"}));
  ASSERT_EQ(fastChecks.size(), 1u);
  for (const auto &check : fastChecks) {
    check(nodeA, noopAcceptor);
  }

  EXPECT_EQ(nodeAFastCalls, 1u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 0u);

  const auto allChecks = pegium::utils::collect(registry.getChecks(nodeA));
  ASSERT_EQ(allChecks.size(), 2u);
  for (const auto &check : allChecks) {
    check(nodeA, noopAcceptor);
  }

  EXPECT_EQ(nodeAFastCalls, 2u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 1u);
}

TEST(ValidationTest, RegistryRejectsBuiltInCustomCategory) {
  pegium::validation::DefaultValidationRegistry registry;

  EXPECT_THROW(
      registry.registerCheck<ValidationNodeA>(
          [](const ValidationNodeA &,
             const pegium::validation::ValidationAcceptor &) {},
          std::string(pegium::validation::kBuiltInValidationCategory)),
      std::invalid_argument);
}

TEST(ValidationTest, RegistryCacheIsInvalidatedOnNewRegistration) {
  pegium::validation::DefaultValidationRegistry registry;

  ValidationNodeA nodeA;

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &,
         const pegium::validation::ValidationAcceptor &) {},
      "fast");

  const auto checksBefore = pegium::utils::collect(registry.getChecks(nodeA));
  ASSERT_EQ(checksBefore.size(), 1u);

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &,
         const pegium::validation::ValidationAcceptor &) {},
      "fast");

  const auto checksAfter = pegium::utils::collect(registry.getChecks(nodeA));
  EXPECT_EQ(checksAfter.size(), 2u);
}

TEST(ValidationTest, RegistryExposesChecksAsStream) {
  pegium::validation::DefaultValidationRegistry registry;
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &,
         const pegium::validation::ValidationAcceptor &) {},
      "fast");

  ValidationNodeA nodeA;
  auto checks = registry.getChecks(nodeA, std::vector<std::string>{"fast"});
  std::size_t count = 0;
  for (const auto &check : checks) {
    ASSERT_TRUE(static_cast<bool>(check));
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(ValidationTest, DefaultValidatorSupportsBuiltInAndCustomCategories) {
  auto registry =
      std::make_unique<pegium::validation::DefaultValidationRegistry>();
  registry->registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &,
         const pegium::validation::ValidationAcceptor &acceptor) {
        acceptor(pegium::services::DiagnosticSeverity::Warning,
                 "Custom warning", 12, 14, std::nullopt, std::nullopt);
      },
      "fast");

  pegium::services::SharedServices sharedServices;
  pegium::services::Services languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  pegium::validation::DefaultDocumentValidator validator(languageServices);

  pegium::workspace::Document document;
  document.uri = "file:///validation.pg";
  document.languageId = "mini";
  document.parseResult.value = std::make_unique<ValidationNodeA>();
  document.referenceDescriptions.push_back(
      {.sourceUri = document.uri,
       .sourcePath = "7",
       .sourceOffset = 7,
       .sourceLength = 13,
       .referenceType = "ValidationNodeA",
       .targetName = "UnknownSymbol",
       .targetUri = {},
       .targetPath = {}});

  pegium::validation::ValidationOptions builtInOnly;
  builtInOnly.categories = {"built-in"};
  const auto builtInDiagnostics =
      validator.validateDocument(document, builtInOnly);
  ASSERT_EQ(builtInDiagnostics.size(), 1u);
  EXPECT_EQ(builtInDiagnostics.front().message,
            "Unresolved reference: UnknownSymbol");
  EXPECT_EQ(builtInDiagnostics.front().source, "mini");
  EXPECT_EQ(builtInDiagnostics.front().begin, 7);
  EXPECT_EQ(builtInDiagnostics.front().end, 20);

  pegium::validation::ValidationOptions customOnly;
  customOnly.categories = {"fast"};
  const auto customDiagnostics = validator.validateDocument(document, customOnly);
  ASSERT_EQ(customDiagnostics.size(), 1u);
  EXPECT_EQ(customDiagnostics.front().message, "Custom warning");
  EXPECT_EQ(customDiagnostics.front().source, "mini");
  EXPECT_EQ(customDiagnostics.front().severity,
            pegium::services::DiagnosticSeverity::Warning);
  EXPECT_EQ(customDiagnostics.front().begin, 12);
  EXPECT_EQ(customDiagnostics.front().end, 14);

  pegium::validation::ValidationOptions disabled;
  disabled.enabled = false;
  EXPECT_TRUE(validator.validateDocument(document, disabled).empty());
}

TEST(ValidationTest, DefaultValidatorUsesReferenceNodeRangeWhenAvailable) {
  using namespace pegium::parser;

  auto registry =
      std::make_unique<pegium::validation::DefaultValidationRegistry>();
  pegium::services::SharedServices sharedServices;
  pegium::services::Services languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  pegium::validation::DefaultDocumentValidator validator(languageServices);

  ParserRule<ValidationRefNode> root{
      "Root",
      "prefix"_kw + ":"_kw + assign<&ValidationRefNode::ref>("UnknownSymbol"_kw)};

  pegium::workspace::Document document;
  document.uri = "file:///validation-ref.pg";
  document.languageId = "mini";
  document.setText("prefix:UnknownSymbol");
  root.parse(document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.value != nullptr);

  document.referenceDescriptions.push_back(
      {.sourceUri = document.uri,
       .sourcePath = "1",
       .sourceOffset = 1,
       .sourceLength = 13,
       .referenceType = "ValidationNodeA",
       .targetName = "UnknownSymbol",
       .targetUri = {},
       .targetPath = {}});

  pegium::validation::ValidationOptions builtInOnly;
  builtInOnly.categories = {"built-in"};
  const auto diagnostics = validator.validateDocument(document, builtInOnly);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().message, "Unresolved reference: UnknownSymbol");
  EXPECT_EQ(diagnostics.front().begin, 7u);
  EXPECT_EQ(diagnostics.front().end, 20u);
}
