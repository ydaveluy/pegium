#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/validation/DefaultValidationRegistry.hpp>
#include <pegium/validation/DiagnosticRanges.hpp>
#include <pegium/validation/ValidationRegistry.hpp>
#include <pegium/workspace/Document.hpp>

#include "ValidationTestUtils.hpp"

namespace pegium::validation {
namespace {

using namespace pegium::parser;
using test_support::ValidationNodeA;
using test_support::ValidationNodeB;

struct ValidationRoot : pegium::AstNode {
  vector<pointer<ValidationNodeA>> nodes;
};

struct BoundValidator {
  std::shared_ptr<std::size_t> nodeACalls = std::make_shared<std::size_t>(0);
  std::shared_ptr<std::size_t> nodeBCalls = std::make_shared<std::size_t>(0);

  void checkNodeA(const ValidationNodeA &, const ValidationAcceptor &) const {
    ++*nodeACalls;
  }

  void checkNodeB(const ValidationNodeB &, const ValidationAcceptor &) const {
    ++*nodeBCalls;
  }
};

TEST(DefaultValidationRegistryTest, FiltersChecksByTypeAndCategory) {
  DefaultValidationRegistry registry;

  std::size_t nodeAFastCalls = 0;
  std::size_t nodeBFastCalls = 0;
  std::size_t astSlowCalls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&nodeAFastCalls](const ValidationNodeA &,
                        const ValidationAcceptor &) { ++nodeAFastCalls; },
      "fast");
  registry.registerCheck<ValidationNodeB>(
      [&nodeBFastCalls](const ValidationNodeB &,
                        const ValidationAcceptor &) { ++nodeBFastCalls; },
      "fast");
  registry.registerCheck<pegium::AstNode>(
      [&astSlowCalls](const pegium::AstNode &, const ValidationAcceptor &) {
        ++astSlowCalls;
      },
      "slow");

  const ValidationAcceptor noopAcceptor = [](services::Diagnostic) {};

  ValidationNodeA nodeA;

  const auto fastChecks =
      utils::collect(registry.getChecks(nodeA, std::vector<std::string>{"fast"}));
  ASSERT_EQ(fastChecks.size(), 1u);
  for (const auto &check : fastChecks) {
    check(nodeA, noopAcceptor);
  }

  EXPECT_EQ(nodeAFastCalls, 1u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 0u);

  const auto allChecks = utils::collect(registry.getChecks(nodeA));
  ASSERT_EQ(allChecks.size(), 2u);
  for (const auto &check : allChecks) {
    check(nodeA, noopAcceptor);
  }

  EXPECT_EQ(nodeAFastCalls, 2u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 1u);
}

TEST(DefaultValidationRegistryTest, RejectsBuiltInCustomCategory) {
  DefaultValidationRegistry registry;

  EXPECT_THROW(
      registry.registerCheck<ValidationNodeA>(
          [](const ValidationNodeA &, const ValidationAcceptor &) {},
          std::string(kBuiltInValidationCategory)),
      std::invalid_argument);
}

TEST(DefaultValidationRegistryTest, CacheIsInvalidatedOnNewRegistration) {
  DefaultValidationRegistry registry;

  ValidationNodeA nodeA;

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {}, "fast");

  const auto checksBefore = utils::collect(registry.getChecks(nodeA));
  ASSERT_EQ(checksBefore.size(), 1u);

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {}, "fast");

  const auto checksAfter = utils::collect(registry.getChecks(nodeA));
  EXPECT_EQ(checksAfter.size(), 2u);
}

TEST(DefaultValidationRegistryTest, ExposesChecksAsStream) {
  DefaultValidationRegistry registry;
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {}, "fast");

  ValidationNodeA nodeA;
  auto checks = registry.getChecks(nodeA, std::vector<std::string>{"fast"});
  std::size_t count = 0;
  for (const auto &check : checks) {
    ASSERT_TRUE(static_cast<bool>(check));
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(DefaultValidationRegistryTest, StoresBeforeAndAfterDocumentHooks) {
  DefaultValidationRegistry registry;

  std::size_t beforeCalls = 0;
  std::size_t afterCalls = 0;

  registry.registerBeforeDocument(
      [&beforeCalls](const pegium::AstNode &, const ValidationAcceptor &,
                     std::span<const std::string>,
                     const utils::CancellationToken &) { ++beforeCalls; });
  registry.registerAfterDocument(
      [&afterCalls](const pegium::AstNode &, const ValidationAcceptor &,
                    std::span<const std::string>,
                    const utils::CancellationToken &) { ++afterCalls; });

  ASSERT_EQ(registry.checksBefore().size(), 1u);
  ASSERT_EQ(registry.checksAfter().size(), 1u);

  const ValidationAcceptor noopAcceptor = [](services::Diagnostic) {};
  ValidationNodeA root;
  const std::vector<std::string> categories{"fast"};

  registry.checksBefore().front()(root, noopAcceptor, categories, {});
  registry.checksAfter().front()(root, noopAcceptor, categories, {});

  EXPECT_EQ(beforeCalls, 1u);
  EXPECT_EQ(afterCalls, 1u);
}

TEST(DefaultValidationRegistryTest, RegisterChecksAddsGroupedTypedChecks) {
  DefaultValidationRegistry registry;
  std::size_t nodeACalls = 0;
  std::size_t nodeBCalls = 0;

  registry.registerChecks(
      {ValidationRegistry::makeValidationCheck<ValidationNodeA>(
           [&nodeACalls](const ValidationNodeA &, const ValidationAcceptor &) {
             ++nodeACalls;
           }),
       ValidationRegistry::makeValidationCheck<ValidationNodeB>(
           [&nodeBCalls](const ValidationNodeB &, const ValidationAcceptor &) {
             ++nodeBCalls;
           })},
      "fast");

  const ValidationAcceptor noopAcceptor = [](services::Diagnostic) {};

  ValidationNodeA nodeA;
  ValidationNodeB nodeB;
  for (const auto &check : utils::collect(registry.getChecks(nodeA))) {
    check(nodeA, noopAcceptor);
  }
  for (const auto &check : utils::collect(registry.getChecks(nodeB))) {
    check(nodeB, noopAcceptor);
  }

  EXPECT_EQ(nodeACalls, 1u);
  EXPECT_EQ(nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest, RegisterChecksBindsValidatorMethods) {
  DefaultValidationRegistry registry;
  const BoundValidator validator;

  registry.registerChecks(
      {ValidationRegistry::makeValidationCheck<&BoundValidator::checkNodeA>(
           validator),
       ValidationRegistry::makeValidationCheck<&BoundValidator::checkNodeB>(
           validator)},
      "fast");

  const ValidationAcceptor noopAcceptor = [](services::Diagnostic) {};

  ValidationNodeA nodeA;
  ValidationNodeB nodeB;
  for (const auto &check : utils::collect(registry.getChecks(nodeA))) {
    check(nodeA, noopAcceptor);
  }
  for (const auto &check : utils::collect(registry.getChecks(nodeB))) {
    check(nodeB, noopAcceptor);
  }

  EXPECT_EQ(*validator.nodeACalls, 1u);
  EXPECT_EQ(*validator.nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest, ValidationCheckExceptionsBecomeDiagnostics) {
  DefaultValidationRegistry registry;
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {
        throw std::runtime_error("boom");
      });

  std::vector<std::string> messages;
  const ValidationAcceptor acceptor =
      [&messages](services::Diagnostic diagnostic) {
        EXPECT_EQ(diagnostic.severity, services::DiagnosticSeverity::Error);
        messages.push_back(std::move(diagnostic.message));
      };

  ValidationNodeA node;
  const auto checks = utils::collect(registry.getChecks(node));
  ASSERT_EQ(checks.size(), 1u);
  checks.front()(node, acceptor);

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_NE(messages.front().find("An error occurred during validation: boom"),
            std::string::npos);
}

TEST(DefaultValidationRegistryTest,
     ValidationCheckExceptionsUseTestedNodeRange) {
  DefaultValidationRegistry registry;
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {
        throw std::runtime_error("boom");
      });

  ParserRule<ValidationNodeA> nodeRule{"Node", "node"_kw};
  ParserRule<ValidationRoot> rootRule{
      "Root", "prefix"_kw + "-"_kw + append<&ValidationRoot::nodes>(nodeRule)};

  workspace::Document document;
  document.uri = "file:///validation-range.pg";
  document.languageId = "mini";
  document.setText("prefix-node");
  pegium::test::parse_rule(rootRule, document, SkipperBuilder().build());

  auto *root = pegium::ast_ptr_cast<ValidationRoot>(document.parseResult.value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->nodes.size(), 1u);
  auto *node = root->nodes.front().get();
  ASSERT_NE(node, nullptr);

  std::optional<services::Diagnostic> captured;
  const ValidationAcceptor acceptor =
      [&captured](services::Diagnostic diagnostic) {
        captured = std::move(diagnostic);
      };

  registry.runChecks(*node, {}, acceptor);

  ASSERT_TRUE(captured.has_value());
  const auto [begin, end] = range_of(*node);
  EXPECT_EQ(captured->begin, begin);
  EXPECT_EQ(captured->end, end);
  EXPECT_NE(captured->message.find("An error occurred during validation: boom"),
            std::string::npos);
}

TEST(DefaultValidationRegistryTest,
     RunChecksKeepsTypeSpecificMatchesAcrossAlternatingNodes) {
  DefaultValidationRegistry registry;
  std::size_t nodeACalls = 0;
  std::size_t nodeBCalls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&nodeACalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++nodeACalls;
      },
      "fast");
  registry.registerCheck<ValidationNodeB>(
      [&nodeBCalls](const ValidationNodeB &, const ValidationAcceptor &) {
        ++nodeBCalls;
      },
      "fast");

  const ValidationAcceptor noopAcceptor = [](services::Diagnostic) {};
  ValidationNodeA nodeA;
  ValidationNodeB nodeB;

  registry.runChecks(nodeA, {}, noopAcceptor);
  registry.runChecks(nodeB, {}, noopAcceptor);
  registry.runChecks(nodeA, {}, noopAcceptor);

  EXPECT_EQ(nodeACalls, 2u);
  EXPECT_EQ(nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest,
     PreparationExceptionsBecomeDiagnostics) {
  DefaultValidationRegistry registry;
  registry.registerBeforeDocument(
      [](const pegium::AstNode &, const ValidationAcceptor &,
         std::span<const std::string>, const utils::CancellationToken &) {
        throw std::runtime_error("setup failed");
      });

  std::vector<std::string> messages;
  const ValidationAcceptor acceptor =
      [&messages](services::Diagnostic diagnostic) {
        EXPECT_EQ(diagnostic.severity, services::DiagnosticSeverity::Error);
        messages.push_back(std::move(diagnostic.message));
      };

  ValidationNodeA root;
  registry.checksBefore().front()(root, acceptor, {}, {});

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_NE(messages.front().find(
                "An error occurred during set-up of the validation: setup failed"),
            std::string::npos);
}

} // namespace
} // namespace pegium::validation
