#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pegium/execution/TaskScheduler.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/validation/DefaultDocumentValidator.hpp>
#include <pegium/validation/DefaultValidationRegistry.hpp>
#include <pegium/validation/DocumentValidator.hpp>
#include <pegium/workspace/Document.hpp>

#include "ValidationTestUtils.hpp"

namespace pegium::validation {
namespace {

using namespace pegium::parser;
using test_support::ValidationNodeA;
using test_support::ValidationRefNode;

struct ValidationRootNode : pegium::AstNode {
  vector<pointer<ValidationNodeA>> nodes;
};

TEST(DefaultDocumentValidatorTest, SupportsBuiltInAndCustomCategories) {
  auto registry = std::make_unique<DefaultValidationRegistry>();
  registry->registerCheck<ValidationRefNode>(
      [](const ValidationRefNode &, const ValidationAcceptor &acceptor) {
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Warning;
        diagnostic.message = "Custom warning";
        diagnostic.begin = 12;
        diagnostic.end = 14;
        acceptor(std::move(diagnostic));
      },
      "fast");

  services::SharedCoreServices sharedServices;
  services::CoreServices languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  DefaultDocumentValidator validator(languageServices);

  ParserRule<ValidationRefNode> root{
      "Root",
      "prefix"_kw + ":"_kw + assign<&ValidationRefNode::ref>("UnknownSymbol"_kw)};

  workspace::Document document;
  document.uri = "file:///validation.pg";
  document.languageId = "mini";
  document.setText("prefix:UnknownSymbol");
  pegium::test::parse_rule(root, document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.value != nullptr);
  ASSERT_FALSE(document.references.empty());
  document.references.front().get()->setResolution(
      ReferenceResolution{.node = nullptr,
                          .description = nullptr,
                          .errorMessage =
                              "Could not resolve reference 'UnknownSymbol'."});

  ValidationOptions builtInOnly;
  builtInOnly.enabled = true;
  builtInOnly.categories = {"built-in"};
  const auto builtInDiagnostics = validator.validateDocument(document, builtInOnly);
  ASSERT_EQ(builtInDiagnostics.size(), 1u);
  EXPECT_EQ(builtInDiagnostics.front().message,
            "Unresolved reference: UnknownSymbol");
  EXPECT_EQ(builtInDiagnostics.front().source, "mini");
  EXPECT_EQ(builtInDiagnostics.front().begin, 7);
  EXPECT_EQ(builtInDiagnostics.front().end, 20);

  ValidationOptions customOnly;
  customOnly.enabled = true;
  customOnly.categories = {"fast"};
  const auto customDiagnostics = validator.validateDocument(document, customOnly);
  ASSERT_EQ(customDiagnostics.size(), 1u);
  EXPECT_EQ(customDiagnostics.front().message, "Custom warning");
  EXPECT_EQ(customDiagnostics.front().source, "mini");
  EXPECT_EQ(customDiagnostics.front().severity,
            services::DiagnosticSeverity::Warning);
  EXPECT_EQ(customDiagnostics.front().begin, 12);
  EXPECT_EQ(customDiagnostics.front().end, 14);

  ValidationOptions disabled;
  disabled.enabled = false;
  EXPECT_TRUE(validator.validateDocument(document, disabled).empty());
}

TEST(DefaultDocumentValidatorTest, RunsBeforeAndAfterDocumentHooks) {
  auto registry = std::make_unique<DefaultValidationRegistry>();
  std::vector<std::string> phases;
  registry->registerBeforeDocument(
      [&phases](const pegium::AstNode &, const ValidationAcceptor &acceptor,
                std::span<const std::string> categories,
                const utils::CancellationToken &) {
        phases.push_back("before");
        EXPECT_EQ(categories.size(), 1u);
        EXPECT_EQ(categories.front(), "fast");
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Information;
        diagnostic.message = "Before";
        diagnostic.begin = 0;
        diagnostic.end = 0;
        acceptor(std::move(diagnostic));
      });
  registry->registerCheck<ValidationNodeA>(
      [&phases](const ValidationNodeA &, const ValidationAcceptor &acceptor) {
        phases.push_back("node");
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Warning;
        diagnostic.message = "Node";
        diagnostic.begin = 1;
        diagnostic.end = 2;
        acceptor(std::move(diagnostic));
      },
      "fast");
  registry->registerAfterDocument(
      [&phases](const pegium::AstNode &, const ValidationAcceptor &acceptor,
                std::span<const std::string>,
                const utils::CancellationToken &) {
        phases.push_back("after");
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Hint;
        diagnostic.message = "After";
        diagnostic.begin = 3;
        diagnostic.end = 3;
        acceptor(std::move(diagnostic));
      });

  services::SharedCoreServices sharedServices;
  services::CoreServices languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  DefaultDocumentValidator validator(languageServices);

  workspace::Document document;
  document.uri = "file:///validation-hooks.pg";
  document.languageId = "mini";
  document.parseResult.value = std::make_unique<ValidationNodeA>();

  ValidationOptions options;
  options.enabled = true;
  options.categories = {"fast"};
  const auto diagnostics = validator.validateDocument(document, options);

  ASSERT_EQ(phases, (std::vector<std::string>{"before", "node", "after"}));
  ASSERT_EQ(diagnostics.size(), 3u);
  EXPECT_EQ(diagnostics[0].message, "Before");
  EXPECT_EQ(diagnostics[1].message, "Node");
  EXPECT_EQ(diagnostics[2].message, "After");
}

TEST(DefaultDocumentValidatorTest, UsesReferenceNodeRangeWhenAvailable) {
  auto registry = std::make_unique<DefaultValidationRegistry>();
  services::SharedCoreServices sharedServices;
  services::CoreServices languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  DefaultDocumentValidator validator(languageServices);

  ParserRule<ValidationRefNode> root{
      "Root",
      "prefix"_kw + ":"_kw + assign<&ValidationRefNode::ref>("UnknownSymbol"_kw)};

  workspace::Document document;
  document.uri = "file:///validation-ref.pg";
  document.languageId = "mini";
  document.setText("prefix:UnknownSymbol");
  pegium::test::parse_rule(root, document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.value != nullptr);
  ASSERT_FALSE(document.references.empty());
  document.references.front().get()->setResolution(
      ReferenceResolution{.node = nullptr,
                          .description = nullptr,
                          .errorMessage =
                              "Could not resolve reference 'UnknownSymbol'."});

  ValidationOptions builtInOnly;
  builtInOnly.enabled = true;
  builtInOnly.categories = {"built-in"};
  const auto diagnostics = validator.validateDocument(document, builtInOnly);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().message, "Unresolved reference: UnknownSymbol");
  EXPECT_EQ(diagnostics.front().begin, 7u);
  EXPECT_EQ(diagnostics.front().end, 20u);
}

TEST(DefaultDocumentValidatorTest,
     PreservesDiagnosticOrderWhenValidatingDescendantsInParallel) {
  auto registry = std::make_unique<DefaultValidationRegistry>();
  registry->registerCheck<ValidationRootNode>(
      [](const ValidationRootNode &, const ValidationAcceptor &acceptor) {
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Information;
        diagnostic.message = "root";
        diagnostic.begin = 0;
        diagnostic.end = 0;
        acceptor(std::move(diagnostic));
      },
      "fast");
  registry->registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &node, const ValidationAcceptor &acceptor) {
        services::Diagnostic diagnostic;
        diagnostic.severity = services::DiagnosticSeverity::Warning;
        diagnostic.message = std::to_string(node.getCstNode().getBegin());
        diagnostic.begin = node.getCstNode().getBegin();
        diagnostic.end = node.getCstNode().getEnd();
        acceptor(std::move(diagnostic));
      },
      "fast");

  services::SharedCoreServices sharedServices;
  sharedServices.execution.taskScheduler =
      std::make_shared<execution::TaskScheduler>(1);
  services::CoreServices languageServices(sharedServices);
  languageServices.languageId = "mini";
  languageServices.validation.validationRegistry = std::move(registry);
  DefaultDocumentValidator validator(languageServices);

  ParserRule<ValidationNodeA> nodeRule{"Node", ","_kw};
  ParserRule<ValidationRootNode> rootRule{
      "Root", some(append<&ValidationRootNode::nodes>(nodeRule))};

  workspace::Document document;
  document.uri = "file:///validation-order.pg";
  document.languageId = "mini";
  document.setText(std::string(260, ','));
  pegium::test::parse_rule(rootRule, document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.value != nullptr);
  const auto *rootNode =
      dynamic_cast<const ValidationRootNode *>(document.parseResult.value.get());
  ASSERT_NE(rootNode, nullptr);
  std::vector<std::string> expectedMessages;
  for (const auto *node : rootNode->getAllContent<ValidationNodeA>()) {
    expectedMessages.push_back(std::to_string(node->getCstNode().getBegin()));
  }

  ValidationOptions options;
  options.enabled = true;
  options.categories = {"fast"};
  const auto diagnostics = validator.validateDocument(document, options);

  ASSERT_EQ(diagnostics.size(), expectedMessages.size() + 1U);
  EXPECT_EQ(diagnostics.front().message, "root");
  for (std::size_t index = 1; index < diagnostics.size(); ++index) {
    EXPECT_EQ(diagnostics[index].message, expectedMessages[index - 1U]);
  }
}

} // namespace
} // namespace pegium::validation
