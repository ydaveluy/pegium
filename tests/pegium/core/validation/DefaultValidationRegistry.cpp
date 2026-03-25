#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/DefaultAstReflection.hpp>
#include <pegium/core/validation/DefaultValidationRegistry.hpp>
#include <pegium/core/validation/DiagnosticRanges.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/core/workspace/Document.hpp>

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

struct MoveOnlyValidator {
  explicit MoveOnlyValidator(std::unique_ptr<std::size_t> calls)
      : calls(std::move(calls)) {}

  MoveOnlyValidator(const MoveOnlyValidator &) = delete;
  MoveOnlyValidator &operator=(const MoveOnlyValidator &) = delete;
  MoveOnlyValidator(MoveOnlyValidator &&) noexcept = default;
  MoveOnlyValidator &operator=(MoveOnlyValidator &&) noexcept = default;

  void checkNodeA(const ValidationNodeA &, const ValidationAcceptor &) const {
    ++*calls;
  }

  std::unique_ptr<std::size_t> calls;
};

struct ValidationBaseNode : pegium::AstNode {};
struct ValidationDerivedNode : ValidationBaseNode {};

class ValidationSubtypeBootstrapParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return BaseRule;
  }

  Rule<ValidationDerivedNode> DerivedRule{"Derived", "derived"_kw};
  Rule<ValidationBaseNode> BaseRule{"Base", DerivedRule};
};

class ValidationNodeABootstrapParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Rule<ValidationNodeA> NodeRule{"Node", "node"_kw};
  Rule<ValidationRoot> RootRule{"Root", append<&ValidationRoot::nodes>(NodeRule)};
};

struct RegistryTestServices {
  RegistryTestServices() : core(shared) {
    shared.observabilitySink = observabilitySink;
    shared.astReflection = std::make_unique<pegium::DefaultAstReflection>();
  }

  std::shared_ptr<test::RecordingObservabilitySink> observabilitySink =
      std::make_shared<test::RecordingObservabilitySink>();
  pegium::SharedCoreServices shared;
  pegium::CoreServices core;
};

void run_checks(const ValidationRegistry &registry, const AstNode &node,
                std::span<const std::string> categories,
                const ValidationAcceptor &acceptor,
                const utils::CancellationToken &cancelToken = {}) {
  auto preparedChecks = registry.prepareChecks(categories);
  preparedChecks->run(node, acceptor, cancelToken);
}

TEST(DefaultValidationRegistryTest, FiltersChecksByTypeAndCategory) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  ValidationNodeABootstrapParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         *context.shared.astReflection);

  std::size_t nodeAFastCalls = 0;
  std::size_t nodeBFastCalls = 0;
  std::size_t astSlowCalls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&nodeAFastCalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++nodeAFastCalls;
      },
      "fast");
  registry.registerCheck<ValidationNodeB>(
      [&nodeBFastCalls](const ValidationNodeB &, const ValidationAcceptor &) {
        ++nodeBFastCalls;
      },
      "fast");
  registry.registerCheck<pegium::AstNode>(
      [&astSlowCalls](const pegium::AstNode &, const ValidationAcceptor &) {
        ++astSlowCalls;
      },
      "slow");

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;

  const std::vector<std::string> fastCategories{"fast"};
  const auto fastChecks = registry.prepareChecks(fastCategories);
  fastChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(nodeAFastCalls, 1u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 0u);

  const auto allChecks = registry.prepareChecks();
  allChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(nodeAFastCalls, 2u);
  EXPECT_EQ(nodeBFastCalls, 0u);
  EXPECT_EQ(astSlowCalls, 1u);
}

TEST(DefaultValidationRegistryTest, RejectsBuiltInCustomCategory) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);

  EXPECT_THROW(
      registry.registerCheck<ValidationNodeA>(
          [](const ValidationNodeA &, const ValidationAcceptor &) {},
          std::string(kBuiltInValidationCategory)),
      std::invalid_argument);
}

TEST(DefaultValidationRegistryTest,
     LateRegistrationsInvalidateFuturePreparedChecksOnly) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);

  std::size_t existingCalls = 0;
  std::size_t lateCalls = 0;
  registry.registerCheck<ValidationNodeA>(
      [&existingCalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++existingCalls;
      },
      "fast");

  auto initialChecks = registry.prepareChecks();

  registry.registerCheck<ValidationNodeA>(
      [&lateCalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++lateCalls;
      },
      "fast");

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  initialChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(existingCalls, 1u);
  EXPECT_EQ(lateCalls, 0u);

  auto refreshedChecks = registry.prepareChecks();
  refreshedChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(existingCalls, 2u);
  EXPECT_EQ(lateCalls, 1u);
}

TEST(DefaultValidationRegistryTest,
     LateRegistrationsUpdateValidationCategories) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {}, "fast");

  EXPECT_EQ(registry.getAllValidationCategories(),
            (std::vector<std::string>{"fast", "slow", "built-in"}));

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {}, "project");

  EXPECT_EQ(
      registry.getAllValidationCategories(),
      (std::vector<std::string>{"fast", "slow", "built-in", "project"}));
}

TEST(DefaultValidationRegistryTest, StoresBeforeAndAfterDocumentHooks) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);

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

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA root;
  const std::vector<std::string> categories{"fast"};

  registry.checksBefore().front()(root, noopAcceptor, categories, {});
  registry.checksAfter().front()(root, noopAcceptor, categories, {});

  EXPECT_EQ(beforeCalls, 1u);
  EXPECT_EQ(afterCalls, 1u);
}

TEST(DefaultValidationRegistryTest, LateRegistrationsUpdateHookReads) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  std::vector<std::string> order;

  registry.registerBeforeDocument(
      [&order](const pegium::AstNode &, const ValidationAcceptor &,
               std::span<const std::string>,
               const utils::CancellationToken &) { order.push_back("before-1"); });
  registry.registerAfterDocument(
      [&order](const pegium::AstNode &, const ValidationAcceptor &,
               std::span<const std::string>,
               const utils::CancellationToken &) { order.push_back("after-1"); });

  ASSERT_EQ(registry.checksBefore().size(), 1u);
  ASSERT_EQ(registry.checksAfter().size(), 1u);

  registry.registerBeforeDocument(
      [&order](const pegium::AstNode &, const ValidationAcceptor &,
               std::span<const std::string>,
               const utils::CancellationToken &) { order.push_back("before-2"); });
  registry.registerAfterDocument(
      [&order](const pegium::AstNode &, const ValidationAcceptor &,
               std::span<const std::string>,
               const utils::CancellationToken &) { order.push_back("after-2"); });

  ASSERT_EQ(registry.checksBefore().size(), 2u);
  ASSERT_EQ(registry.checksAfter().size(), 2u);

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA root;
  const std::vector<std::string> categories{"fast"};

  for (const auto &checkBefore : registry.checksBefore()) {
    checkBefore(root, noopAcceptor, categories, {});
  }
  for (const auto &checkAfter : registry.checksAfter()) {
    checkAfter(root, noopAcceptor, categories, {});
  }

  EXPECT_EQ(order, (std::vector<std::string>{
                       "before-1",
                       "before-2",
                       "after-1",
                       "after-2",
                   }));
}

TEST(DefaultValidationRegistryTest, RegisterChecksAddsGroupedTypedChecks) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
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

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  ValidationNodeB nodeB;

  run_checks(registry, nodeA, {}, noopAcceptor);
  run_checks(registry, nodeB, {}, noopAcceptor);

  EXPECT_EQ(nodeACalls, 1u);
  EXPECT_EQ(nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest, RegisterChecksBindsValidatorMethods) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  const BoundValidator validator;

  registry.registerChecks(
      {ValidationRegistry::makeValidationCheck<&BoundValidator::checkNodeA>(
           validator),
       ValidationRegistry::makeValidationCheck<&BoundValidator::checkNodeB>(
           validator)},
      "fast");

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  ValidationNodeB nodeB;

  run_checks(registry, nodeA, {}, noopAcceptor);
  run_checks(registry, nodeB, {}, noopAcceptor);

  EXPECT_EQ(*validator.nodeACalls, 1u);
  EXPECT_EQ(*validator.nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest,
     RegisterChecksBindsMoveOnlyValidatorMethods) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  auto calls = std::make_unique<std::size_t>(0);
  auto *callCounter = calls.get();

  registry.registerChecks(
      {ValidationRegistry::makeValidationCheck<&MoveOnlyValidator::checkNodeA>(
          MoveOnlyValidator(std::move(calls)))},
      "fast");

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  run_checks(registry, nodeA, {}, noopAcceptor);

  ASSERT_NE(callCounter, nullptr);
  EXPECT_EQ(*callCounter, 1u);
}

TEST(DefaultValidationRegistryTest, ValidationCheckExceptionsBecomeDiagnostics) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {
        throw std::runtime_error("boom");
      });

  std::vector<std::string> messages;
  const ValidationAcceptor acceptor{
      [&messages](pegium::Diagnostic diagnostic) {
        EXPECT_EQ(diagnostic.severity, pegium::DiagnosticSeverity::Error);
        messages.push_back(std::move(diagnostic.message));
      }};

  ValidationNodeA node;
  run_checks(registry, node, {}, acceptor);

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_NE(messages.front().find("An error occurred during validation: boom"),
            std::string::npos);
  ASSERT_TRUE(context.observabilitySink->waitForCount(1));
  const auto observation = context.observabilitySink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::ValidationCheckThrew);
  EXPECT_NE(observation->message.find("boom"), std::string::npos);
}

TEST(DefaultValidationRegistryTest,
     ValidationCheckExceptionsUseTestedNodeRange) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &) {
        throw std::runtime_error("boom");
      });

  ParserRule<ValidationNodeA> nodeRule{"Node", "node"_kw};
  ParserRule<ValidationRoot> rootRule{
      "Root", "prefix"_kw + "-"_kw + append<&ValidationRoot::nodes>(nodeRule)};

  workspace::Document document(
      test::make_text_document("file:///validation-range.pg", "mini",
                               "prefix-node"));
  pegium::test::parse_rule(rootRule, document, SkipperBuilder().build());

  auto *root = pegium::ast_ptr_cast<ValidationRoot>(document.parseResult.value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->nodes.size(), 1u);
  auto *node = root->nodes.front().get();
  ASSERT_NE(node, nullptr);

  std::optional<pegium::Diagnostic> captured;
  const ValidationAcceptor acceptor{
      [&captured](pegium::Diagnostic diagnostic) {
        captured = std::move(diagnostic);
      }};

  run_checks(registry, *node, {}, acceptor);

  ASSERT_TRUE(captured.has_value());
  const auto [begin, end] = range_of(*node);
  EXPECT_EQ(captured->begin, begin);
  EXPECT_EQ(captured->end, end);
  EXPECT_NE(captured->message.find("An error occurred during validation: boom"),
            std::string::npos);
}

TEST(DefaultValidationRegistryTest,
     RunChecksKeepsTypeSpecificMatchesAcrossAlternatingNodes) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
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

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  ValidationNodeB nodeB;

  run_checks(registry, nodeA, {}, noopAcceptor);
  run_checks(registry, nodeB, {}, noopAcceptor);
  run_checks(registry, nodeA, {}, noopAcceptor);

  EXPECT_EQ(nodeACalls, 2u);
  EXPECT_EQ(nodeBCalls, 1u);
}

TEST(DefaultValidationRegistryTest,
     PreparedChecksWithDifferentCategoriesStayIndependent) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  std::size_t fastCalls = 0;
  std::size_t slowCalls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&fastCalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++fastCalls;
      },
      "fast");
  registry.registerCheck<ValidationNodeA>(
      [&slowCalls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++slowCalls;
      },
      "slow");

  const std::vector<std::string> fastCategories{"fast"};
  const std::vector<std::string> slowCategories{"slow"};
  const auto fastChecks = registry.prepareChecks(fastCategories);
  const auto slowChecks = registry.prepareChecks(slowCategories);

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  fastChecks->run(nodeA, noopAcceptor, {});
  slowChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(fastCalls, 1u);
  EXPECT_EQ(slowCalls, 1u);
}

TEST(DefaultValidationRegistryTest, UnknownCategoriesExecuteNothing) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  std::size_t calls = 0;

  registry.registerCheck<ValidationNodeA>(
      [&calls](const ValidationNodeA &, const ValidationAcceptor &) {
        ++calls;
      },
      "fast");

  const std::vector<std::string> unknownCategories{"unknown"};
  auto preparedChecks = registry.prepareChecks(unknownCategories);

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  preparedChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(calls, 0u);
}

TEST(DefaultValidationRegistryTest, ChecksExecuteInRegistrationOrder) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  ValidationNodeABootstrapParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         *context.shared.astReflection);

  std::vector<std::string> order;
  registry.registerCheck<ValidationNodeA>(
      [&order](const ValidationNodeA &, const ValidationAcceptor &) {
        order.push_back("node-fast-1");
      },
      "fast");
  registry.registerCheck<pegium::AstNode>(
      [&order](const pegium::AstNode &, const ValidationAcceptor &) {
        order.push_back("ast-fast");
      },
      "fast");
  registry.registerCheck<ValidationNodeA>(
      [&order](const ValidationNodeA &, const ValidationAcceptor &) {
        order.push_back("node-fast-2");
      },
      "fast");

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  run_checks(registry, nodeA, {}, noopAcceptor);

  EXPECT_EQ(order, (std::vector<std::string>{
                       "node-fast-1",
                       "ast-fast",
                       "node-fast-2",
                   }));
}

TEST(DefaultValidationRegistryTest,
     PreparedChecksRemainUsableAfterRegistryDestruction) {
  std::size_t calls = 0;
  std::unique_ptr<const ValidationRegistry::PreparedChecks> preparedChecks;

  {
    RegistryTestServices context;
    DefaultValidationRegistry registry(context.core);
    registry.registerCheck<ValidationNodeA>(
        [&calls](const ValidationNodeA &, const ValidationAcceptor &) {
          ++calls;
        },
        "fast");
    preparedChecks = registry.prepareChecks();
  }

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA nodeA;
  preparedChecks->run(nodeA, noopAcceptor, {});

  EXPECT_EQ(calls, 1u);
}

TEST(DefaultValidationRegistryTest,
     PreparationExceptionsBecomeDiagnostics) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  registry.registerBeforeDocument(
      [](const pegium::AstNode &, const ValidationAcceptor &,
         std::span<const std::string>, const utils::CancellationToken &) {
        throw std::runtime_error("setup failed");
      });

  std::vector<std::string> messages;
  const ValidationAcceptor acceptor{
      [&messages](pegium::Diagnostic diagnostic) {
        EXPECT_EQ(diagnostic.severity, pegium::DiagnosticSeverity::Error);
        messages.push_back(std::move(diagnostic.message));
      }};

  ValidationNodeA root;
  registry.checksBefore().front()(root, acceptor, {}, {});

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_NE(
      messages.front().find(
          "An error occurred during set-up of the validation: setup failed"),
      std::string::npos);
  ASSERT_TRUE(context.observabilitySink->waitForCount(1));
  const auto observation = context.observabilitySink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::ValidationPreparationThrew);
  EXPECT_NE(observation->message.find("setup failed"), std::string::npos);
}

TEST(DefaultValidationRegistryTest,
     FinalizationExceptionsBecomeDiagnosticsAndObservations) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  registry.registerAfterDocument(
      [](const pegium::AstNode &, const ValidationAcceptor &,
         std::span<const std::string>, const utils::CancellationToken &) {
        throw std::runtime_error("tear-down failed");
      });

  std::vector<std::string> messages;
  const ValidationAcceptor acceptor{
      [&messages](pegium::Diagnostic diagnostic) {
        messages.push_back(std::move(diagnostic.message));
      }};

  ValidationNodeA root;
  registry.checksAfter().front()(root, acceptor, {}, {});

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_NE(
      messages.front().find(
          "An error occurred during tear-down of the validation: tear-down failed"),
      std::string::npos);
  ASSERT_TRUE(context.observabilitySink->waitForCount(1));
  const auto observation = context.observabilitySink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::ValidationFinalizationThrew);
  EXPECT_NE(observation->message.find("tear-down failed"), std::string::npos);
}

TEST(DefaultValidationRegistryTest, BaseTypeChecksApplyToDerivedNodes) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);
  ASSERT_NE(context.shared.astReflection, nullptr);
  ValidationSubtypeBootstrapParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         *context.shared.astReflection);

  std::size_t baseCalls = 0;
  registry.registerCheck<ValidationBaseNode>(
      [&baseCalls](const ValidationBaseNode &, const ValidationAcceptor &) {
        ++baseCalls;
      });

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationDerivedNode derivedNode;

  run_checks(registry, derivedNode, {}, noopAcceptor);
  run_checks(registry, derivedNode, {}, noopAcceptor);

  EXPECT_EQ(baseCalls, 2u);
  EXPECT_TRUE(context.shared.astReflection->isSubtype(
      std::type_index(typeid(ValidationDerivedNode)),
      std::type_index(typeid(ValidationBaseNode))));
}

TEST(DefaultValidationRegistryTest,
     CancellationAwareChecksReceiveTokenAndPropagateCancellation) {
  RegistryTestServices context;
  DefaultValidationRegistry registry(context.core);

  registry.registerCheck<ValidationNodeA>(
      [](const ValidationNodeA &, const ValidationAcceptor &,
         const utils::CancellationToken &cancelToken) {
        utils::throw_if_cancelled(cancelToken);
      });

  utils::CancellationTokenSource source;
  source.request_stop();

  const ValidationAcceptor noopAcceptor{[](pegium::Diagnostic) {}};
  ValidationNodeA node;

  auto preparedChecks = registry.prepareChecks();
  EXPECT_THROW(preparedChecks->run(node, noopAcceptor, source.get_token()),
               utils::OperationCancelled);
}

} // namespace
} // namespace pegium::validation
