#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/Document.hpp>

using namespace pegium::parser;

namespace {

struct FixedScopeProvider final : pegium::references::ScopeComputation {
  std::vector<pegium::workspace::AstNodeDescription> collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &) const override {
    if (!document.parseSucceeded()) {
      return {};
    }

    return {{.name = "Root",
             .type = "rule",
             .documentUri = document.uri,
             .offset = 0,
             .path = "0"}};
  }

  pegium::workspace::LocalSymbols collectLocalSymbols(
      const pegium::workspace::Document &,
      const pegium::utils::CancellationToken &) const override {
    return {};
  }
};

struct CountingLinker final : pegium::references::Linker {
  mutable std::size_t calls = 0;

  void link(pegium::workspace::Document &,
            const pegium::utils::CancellationToken &) const override {
    ++calls;
  }
};

struct ParseFailValidator final : pegium::validation::DocumentValidator {
  std::vector<pegium::services::Diagnostic>
  validateDocument(const pegium::workspace::Document &document,
                   const pegium::validation::ValidationOptions &) const override {
    if (document.parseSucceeded()) {
      return {};
    }

    return {{.severity = pegium::services::DiagnosticSeverity::Error,
             .message = "Parse failed",
             .source = "validator",
             .begin = 0,
             .end = 0}};
  }
};

struct CountingValidator final : pegium::validation::DocumentValidator {
  mutable std::size_t calls = 0;

  std::vector<pegium::services::Diagnostic>
  validateDocument(const pegium::workspace::Document &,
                   const pegium::validation::ValidationOptions &) const override {
    ++calls;
    return {};
  }
};

struct OptionsAwareValidator final : pegium::validation::DocumentValidator {
  mutable std::size_t optionsCalls = 0;
  mutable std::vector<std::string> lastCategories;

  std::vector<pegium::services::Diagnostic>
  validateDocument(const pegium::workspace::Document &,
                   const pegium::validation::ValidationOptions &options)
      const override {
    ++optionsCalls;
    lastCategories = options.categories;
    return {};
  }
};

struct TokenNode : pegium::AstNode {
  string token;
};

template <typename RuleType>
class TestRuleParser final : public Parser {
public:
  TestRuleParser(const RuleType &rule, Skipper skipper, ParseOptions options)
      : _rule(rule), _skipper(std::move(skipper)), _options(options) {}

  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken &cancelToken = {})
      const override {
    _rule.get().parse(document, _skipper, cancelToken, _options);
  }

private:
  std::reference_wrapper<const RuleType> _rule;
  Skipper _skipper;
  ParseOptions _options;
};

template <typename RuleType>
std::unique_ptr<const Parser>
make_test_rule_parser(const RuleType &rule, Skipper skipper,
                      ParseOptions options = {}) {
  return std::make_unique<TestRuleParser<RuleType>>(rule, std::move(skipper),
                                                    options);
}

} // namespace

TEST(ArchitectureTest, WorkspacePipelineRunsParseIndexLinkAndValidate) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);

  auto scopeComputation = std::make_unique<FixedScopeProvider>();
  auto linker = std::make_unique<CountingLinker>();
  auto documentValidator = std::make_unique<ParseFailValidator>();
  auto *linkerPtr = linker.get();
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = shared.workspace.workspaceManager->openOrUpdateAndRefresh(
      "file:///mini.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);
  EXPECT_TRUE(document->parseSucceeded());
  EXPECT_EQ(document->state, pegium::workspace::DocumentState::Validated);

  auto symbols = pegium::utils::collect(
      shared.workspace.indexManager->elementsForDocument(document->uri));
  ASSERT_EQ(symbols.size(), 1u);
  EXPECT_EQ(symbols[0].name, "Root");
  EXPECT_EQ(linkerPtr->calls, 1u);

  document = shared.workspace.workspaceManager->openOrUpdateAndRefresh("file:///mini.pg", "ko",
                                                     "mini");
  ASSERT_TRUE(document != nullptr);
  EXPECT_FALSE(document->parseSucceeded());
  EXPECT_EQ(linkerPtr->calls, 2u);

  auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(document->uri);
  EXPECT_FALSE(diagnostics.empty());
  EXPECT_EQ(diagnostics.back().message, "Parse failed");
}

TEST(ArchitectureTest, WorkspaceManagerConfigurationControlsBuildOptions) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);

  auto scopeComputation = std::make_unique<FixedScopeProvider>();
  auto linker = std::make_unique<CountingLinker>();
  auto documentValidator = std::make_unique<CountingValidator>();
  auto *linkerPtr = linker.get();
  auto *documentValidatorPtr = documentValidator.get();
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  pegium::workspace::WorkspaceConfiguration configuration;
  configuration.validation.enabled = false;
  shared.workspace.workspaceManager->setConfigurationProvider(
      std::make_shared<pegium::workspace::StaticWorkspaceConfigurationProvider>(
          configuration));

  auto document =
      shared.workspace.workspaceManager->openOrUpdateAndRefresh("file:///cfg.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  EXPECT_EQ(linkerPtr->calls, 1u);
  EXPECT_EQ(documentValidatorPtr->calls, 0u);
  EXPECT_EQ(document->state, pegium::workspace::DocumentState::Validated);
}

TEST(ArchitectureTest,
     WorkspaceManagerConfigurationCanPassValidationOptionsToBuilder) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);

  auto scopeComputation = std::make_unique<FixedScopeProvider>();
  auto linker = std::make_unique<CountingLinker>();
  auto documentValidator = std::make_unique<OptionsAwareValidator>();
  auto *documentValidatorPtr = documentValidator.get();
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  pegium::workspace::WorkspaceConfiguration configuration;
  configuration.validation.categories = {"built-in"};
  shared.workspace.workspaceManager->setConfigurationProvider(
      std::make_shared<pegium::workspace::StaticWorkspaceConfigurationProvider>(
          configuration));

  auto document = shared.workspace.workspaceManager->openOrUpdateAndRefresh(
      "file:///cfg-validation-options.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  EXPECT_EQ(documentValidatorPtr->optionsCalls, 1u);
  ASSERT_EQ(documentValidatorPtr->lastCategories.size(), 1u);
  EXPECT_EQ(documentValidatorPtr->lastCategories.front(), "built-in");
}
