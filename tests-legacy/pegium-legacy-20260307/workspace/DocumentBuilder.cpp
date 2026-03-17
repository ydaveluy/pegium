#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/IndexManager.hpp>
#include <pegium/workspace/Document.hpp>

using namespace pegium::parser;

namespace {

std::shared_ptr<pegium::workspace::Document>
open_or_update(pegium::services::SharedServices &shared, std::string uri,
               std::string text, std::string languageId) {
  return shared.workspace.documents->openOrUpdate(std::move(uri), std::move(text),
                                                  std::move(languageId));
}

std::shared_ptr<pegium::workspace::Document>
open_or_update_and_refresh(pegium::services::SharedServices &shared,
                           std::string uri, std::string text,
                           std::string languageId) {
  auto document = open_or_update(shared, std::move(uri), std::move(text),
                                 std::move(languageId));
  if (document != nullptr) {
    (void)shared.workspace.documentBuilder->build(std::vector{document});
  }
  return document;
}

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

struct ReferencingLinker final : pegium::references::Linker {
  void link(pegium::workspace::Document &document,
            const pegium::utils::CancellationToken &) const override {
    document.referenceDescriptions = {
        {.sourceUri = document.uri,
         .sourcePath = "0",
         .sourceOffset = 0,
         .sourceLength = 4,
         .referenceType = "rule",
         .targetName = "Root",
         .targetUri = {},
         .targetPath = {}}};
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

struct TextBackedScopeProvider final : pegium::references::ScopeComputation {
  std::vector<pegium::workspace::AstNodeDescription> collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &) const override {
    if (!document.parseSucceeded() ||
        document.uri.find("/def.") == std::string::npos) {
      return {};
    }

    return {{.name = document.text(),
             .type = "definition",
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

struct CountingReferenceLinker final : pegium::references::Linker {
  explicit CountingReferenceLinker(const pegium::workspace::IndexManager *index)
      : indexManager(index) {}

  mutable std::unordered_map<std::string, std::size_t> callsByUri;
  const pegium::workspace::IndexManager *indexManager = nullptr;

  void link(pegium::workspace::Document &document,
            const pegium::utils::CancellationToken &) const override {
    ++callsByUri[document.uri];
    document.referenceDescriptions.clear();

    if (document.uri.find("/use.") != std::string::npos) {
      pegium::workspace::ReferenceDescription reference{
          .sourceUri = document.uri,
          .sourcePath = "0",
          .sourceOffset = 0,
          .sourceLength = static_cast<pegium::TextOffset>(document.text().size()),
          .referenceType = "definition",
          .targetName = document.text(),
      };

      if (indexManager != nullptr) {
        for (const auto &entry : indexManager->findElementsByName(document.text())) {
          reference.targetUri = entry.documentUri;
          reference.targetPath = entry.path;
          break;
        }
      }

      document.referenceDescriptions.push_back(std::move(reference));
    }
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

TEST(ArchitectureTest, DocumentBuilderResetToComputedScopesClearsReferences) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<ReferencingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///mini-reset.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);
  EXPECT_EQ(document->state, pegium::workspace::DocumentState::Validated);

  auto references = pegium::utils::collect(
      shared.workspace.indexManager->referenceDescriptionsForDocument(
          document->uri));
  ASSERT_EQ(references.size(), 1u);
  EXPECT_EQ(references[0].targetName, "Root");

  shared.workspace.documentBuilder->resetToState(*document,
                              pegium::workspace::DocumentState::ComputedScopes);
  EXPECT_EQ(document->state, pegium::workspace::DocumentState::ComputedScopes);

  references = pegium::utils::collect(
      shared.workspace.indexManager->referenceDescriptionsForDocument(
          document->uri));
  EXPECT_TRUE(references.empty());
}

TEST(ArchitectureTest, DocumentBuilderUpdateRelinksAffectedDocuments) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);

  pegium::services::SharedServices shared;
  auto scopeComputation = std::make_unique<TextBackedScopeProvider>();
  auto linker = std::make_unique<CountingReferenceLinker>(
      shared.workspace.indexManager.get());
  auto documentValidator = std::make_unique<ParseFailValidator>();
  auto *linkerPtr = linker.get();


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto definition =
      open_or_update_and_refresh(shared, "file:///def.pg", "Root", "mini");
  ASSERT_TRUE(definition != nullptr);
  auto usage =
      open_or_update_and_refresh(shared, "file:///use.pg", "Root", "mini");
  ASSERT_TRUE(usage != nullptr);
  ASSERT_EQ(linkerPtr->callsByUri["file:///use.pg"], 1u);

  (void)open_or_update(shared, "file:///def.pg", "Catalogue", "mini");
  auto updateResult =
      shared.workspace.workspaceManager->update(std::vector<std::string>{"file:///def.pg"}, {});

  ASSERT_EQ(linkerPtr->callsByUri["file:///use.pg"], 2u);

  bool rebuiltUsage = false;
  for (const auto &document : updateResult.rebuiltDocuments) {
    if (document && document->uri == "file:///use.pg") {
      rebuiltUsage = true;
      break;
    }
  }
  EXPECT_TRUE(rebuiltUsage);
}

TEST(ArchitectureTest, DocumentBuilderUpdateRelinksUnresolvedReferences) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);

  pegium::services::SharedServices shared;
  auto scopeComputation = std::make_unique<TextBackedScopeProvider>();
  auto linker = std::make_unique<CountingReferenceLinker>(
      shared.workspace.indexManager.get());
  auto documentValidator = std::make_unique<ParseFailValidator>();
  auto *linkerPtr = linker.get();


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto usage =
      open_or_update_and_refresh(shared, "file:///use.pg", "Missing", "mini");
  ASSERT_TRUE(usage != nullptr);
  ASSERT_EQ(linkerPtr->callsByUri["file:///use.pg"], 1u);
  ASSERT_EQ(usage->referenceDescriptions.size(), 1u);
  EXPECT_FALSE(usage->referenceDescriptions.front().targetUri.has_value());

  const auto updateResult = shared.workspace.documentBuilder->update({}, {});

  ASSERT_EQ(linkerPtr->callsByUri["file:///use.pg"], 2u);
  ASSERT_EQ(updateResult.rebuiltDocuments.size(), 1u);
  ASSERT_TRUE(updateResult.rebuiltDocuments.front() != nullptr);
  EXPECT_EQ(updateResult.rebuiltDocuments.front()->uri, "file:///use.pg");
}

TEST(ArchitectureTest, DocumentBuilderBuildOptionsCanDisableValidation) {
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

  auto document = open_or_update(shared, "file:///opts.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::workspace::BuildOptions buildOptions;
  buildOptions.validation.enabled = false;
  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}, buildOptions));

  EXPECT_EQ(linkerPtr->calls, 1u);
  EXPECT_EQ(documentValidatorPtr->calls, 0u);
  EXPECT_EQ(document->state, pegium::workspace::DocumentState::Validated);
  EXPECT_TRUE(document->references.empty());
  EXPECT_TRUE(
      pegium::utils::collect(
          shared.workspace.indexManager->referenceDescriptionsForDocument(
              document->uri))
          .empty());
}

TEST(ArchitectureTest, DocumentBuilderBuildOptionsCanPassValidationOptions) {
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

  auto document =
      open_or_update(shared, "file:///opts-categories.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::workspace::BuildOptions buildOptions;
  buildOptions.validation.categories = {"built-in", "fast"};

  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}, buildOptions));
  EXPECT_EQ(documentValidatorPtr->optionsCalls, 1u);
  ASSERT_EQ(documentValidatorPtr->lastCategories.size(), 2u);
  EXPECT_EQ(documentValidatorPtr->lastCategories[0], "built-in");
  EXPECT_EQ(documentValidatorPtr->lastCategories[1], "fast");
}

TEST(ArchitectureTest, DocumentBuilderUpdateUsesUpdateBuildOptions) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);

  auto scopeComputation = std::make_unique<FixedScopeProvider>();
  auto linker = std::make_unique<CountingLinker>();
  auto documentValidator = std::make_unique<CountingValidator>();
  auto *documentValidatorPtr = documentValidator.get();
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::move(documentValidator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document =
      open_or_update_and_refresh(shared, "file:///upd-opts.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);
  ASSERT_EQ(documentValidatorPtr->calls, 1u);

  (void)open_or_update(shared, "file:///upd-opts.pg", "ok", "mini");

  shared.workspace.documentBuilder->updateBuildOptions().validation.enabled = false;
  const auto updateResult =
      shared.workspace.documentBuilder->update(std::vector<std::string>{"file:///upd-opts.pg"}, {});

  ASSERT_EQ(updateResult.rebuiltDocuments.size(), 1u);
  EXPECT_EQ(documentValidatorPtr->calls, 1u);
  EXPECT_TRUE(updateResult.rebuiltDocuments.front()->diagnostics.empty());
}

TEST(ArchitectureTest,
     DocumentBuilderUpdateRebuildsWhenValidationCategoriesExpand) {
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

  auto document = open_or_update(shared, "file:///upd-categories.pg", "ok",
                                                "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::workspace::BuildOptions initialBuildOptions;
  initialBuildOptions.validation.categories = {"built-in"};
  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}, initialBuildOptions));
  ASSERT_EQ(documentValidatorPtr->optionsCalls, 1u);
  ASSERT_EQ(documentValidatorPtr->lastCategories.size(), 1u);
  EXPECT_EQ(documentValidatorPtr->lastCategories.front(), "built-in");

  shared.workspace.documentBuilder->updateBuildOptions().validation.categories = {"built-in", "slow"};
  const auto updateResult = shared.workspace.documentBuilder->update({}, {});

  ASSERT_EQ(updateResult.rebuiltDocuments.size(), 1u);
  EXPECT_EQ(updateResult.rebuiltDocuments.front()->uri, document->uri);
  ASSERT_EQ(documentValidatorPtr->optionsCalls, 2u);
  ASSERT_EQ(documentValidatorPtr->lastCategories.size(), 2u);
  EXPECT_EQ(documentValidatorPtr->lastCategories[0], "built-in");
  EXPECT_EQ(documentValidatorPtr->lastCategories[1], "slow");
}

TEST(ArchitectureTest, DocumentBuilderEmitsUpdateAndPhaseEvents) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update(shared, "file:///events.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  std::size_t updateCount = 0;
  std::vector<std::string> lastChanged;
  std::vector<std::string> lastDeleted;
  auto updateSubscription = shared.workspace.documentBuilder->onUpdate(
      [&updateCount, &lastChanged, &lastDeleted](
          std::span<const std::string> changedUris,
          std::span<const std::string> deletedUris) {
        ++updateCount;
        lastChanged.assign(changedUris.begin(), changedUris.end());
        lastDeleted.assign(deletedUris.begin(), deletedUris.end());
      });

  std::size_t validatedBuildPhaseCount = 0;
  auto buildPhaseSubscription = shared.workspace.documentBuilder->onBuildPhase(
      pegium::workspace::DocumentState::Validated,
      [&validatedBuildPhaseCount, &document](
          std::span<const std::shared_ptr<pegium::workspace::Document>>
              builtDocuments) {
        ++validatedBuildPhaseCount;
        ASSERT_EQ(builtDocuments.size(), 1u);
        ASSERT_TRUE(builtDocuments.front() != nullptr);
        EXPECT_EQ(builtDocuments.front()->uri, document->uri);
      });

  std::size_t validatedDocumentPhaseCount = 0;
  auto documentPhaseSubscription = shared.workspace.documentBuilder->onDocumentPhase(
      pegium::workspace::DocumentState::Validated,
      [&validatedDocumentPhaseCount, &document](
          const std::shared_ptr<pegium::workspace::Document> &builtDocument) {
        ++validatedDocumentPhaseCount;
        ASSERT_TRUE(builtDocument != nullptr);
        EXPECT_EQ(builtDocument->uri, document->uri);
      });

  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}));
  EXPECT_EQ(updateCount, 1u);
  ASSERT_EQ(lastChanged.size(), 1u);
  EXPECT_EQ(lastChanged.front(), document->uri);
  EXPECT_TRUE(lastDeleted.empty());
  EXPECT_EQ(validatedBuildPhaseCount, 1u);
  EXPECT_EQ(validatedDocumentPhaseCount, 1u);

  (void)open_or_update(shared, document->uri, "ok", "mini");
  const auto updateResult =
      shared.workspace.documentBuilder->update(std::vector<std::string>{document->uri}, {});
  ASSERT_EQ(updateResult.rebuiltDocuments.size(), 1u);
  EXPECT_EQ(updateCount, 2u);
  EXPECT_EQ(validatedBuildPhaseCount, 2u);
  EXPECT_EQ(validatedDocumentPhaseCount, 2u);

  (void)updateSubscription;
  (void)buildPhaseSubscription;
  (void)documentPhaseSubscription;
}

TEST(ArchitectureTest, DocumentBuilderCancellationEmitsDocumentPhaseOnly) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto documentA = open_or_update(shared, "file:///a.pg", "ok", "mini");
  auto documentB = open_or_update(shared, "file:///b.pg", "ok", "mini");
  ASSERT_TRUE(documentA != nullptr);
  ASSERT_TRUE(documentB != nullptr);

  std::size_t parsedDocumentPhaseCount = 0;
  auto cancellationSource = std::stop_source{};
  auto documentPhaseSubscription = shared.workspace.documentBuilder->onDocumentPhase(
      pegium::workspace::DocumentState::Parsed,
      [&parsedDocumentPhaseCount, &cancellationSource](
          const std::shared_ptr<pegium::workspace::Document> &) {
        ++parsedDocumentPhaseCount;
        if (parsedDocumentPhaseCount == 1u) {
          cancellationSource.request_stop();
        }
      });

  std::size_t parsedBuildPhaseCount = 0;
  auto buildPhaseSubscription = shared.workspace.documentBuilder->onBuildPhase(
      pegium::workspace::DocumentState::Parsed,
      [&parsedBuildPhaseCount](
          std::span<const std::shared_ptr<pegium::workspace::Document>>) {
        ++parsedBuildPhaseCount;
      });

  EXPECT_THROW(
      (void)shared.workspace.documentBuilder->build(
          std::vector{documentA, documentB}, {},
          cancellationSource.get_token()),
      pegium::utils::OperationCancelled);
  EXPECT_EQ(parsedDocumentPhaseCount, 1u);
  EXPECT_EQ(parsedBuildPhaseCount, 0u);

  std::size_t parsedDocuments = 0;
  if (documentA->state >= pegium::workspace::DocumentState::Parsed) {
    ++parsedDocuments;
  }
  if (documentB->state >= pegium::workspace::DocumentState::Parsed) {
    ++parsedDocuments;
  }
  EXPECT_EQ(parsedDocuments, 1u);

  (void)documentPhaseSubscription;
  (void)buildPhaseSubscription;
}

TEST(ArchitectureTest,
     DocumentBuilderUpdateRebuildsDocumentsFromIncompleteCancelledBuild) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  auto validator = std::make_unique<CountingValidator>();
  auto *validatorPtr = validator.get();
  languageServices->validation.documentValidator = std::move(validator);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto documentA =
      open_or_update(shared, "file:///incomplete-a.pg", "ok", "mini");
  auto documentB =
      open_or_update(shared, "file:///incomplete-b.pg", "ok", "mini");
  ASSERT_TRUE(documentA != nullptr);
  ASSERT_TRUE(documentB != nullptr);

  std::size_t parsedDocumentPhaseCount = 0;
  auto cancellationSource = std::stop_source{};
  auto documentPhaseSubscription = shared.workspace.documentBuilder->onDocumentPhase(
      pegium::workspace::DocumentState::Parsed,
      [&parsedDocumentPhaseCount, &cancellationSource](
          const std::shared_ptr<pegium::workspace::Document> &) {
        ++parsedDocumentPhaseCount;
        if (parsedDocumentPhaseCount == 1u) {
          cancellationSource.request_stop();
        }
      });

  EXPECT_THROW(
      (void)shared.workspace.documentBuilder->build(
          std::vector{documentA, documentB}, {},
          cancellationSource.get_token()),
      pegium::utils::OperationCancelled);
  EXPECT_EQ(validatorPtr->calls, 0u);

  const auto updateResult = shared.workspace.documentBuilder->update({}, {});
  ASSERT_EQ(updateResult.rebuiltDocuments.size(), 2u);
  EXPECT_EQ(validatorPtr->calls, 2u);
  EXPECT_EQ(documentA->state, pegium::workspace::DocumentState::Validated);
  EXPECT_EQ(documentB->state, pegium::workspace::DocumentState::Validated);

  (void)documentPhaseSubscription;
}

TEST(ArchitectureTest, DocumentBuilderWaitUntilStateUnblocksAfterBuild) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update(shared, "file:///wait.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  auto waiter = std::async(std::launch::async, [&shared]() {
    return shared.workspace.documentBuilder->waitUntil(pegium::workspace::DocumentState::Validated);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}));
  EXPECT_TRUE(waiter.get());
}

TEST(ArchitectureTest, DocumentBuilderWaitUntilDocumentStateUnblocksAfterBuild) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document =
      open_or_update(shared, "file:///wait-doc.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  auto waiter = std::async(std::launch::async, [&shared, &document]() {
    return shared.workspace.documentBuilder->waitUntil(pegium::workspace::DocumentState::Validated,
                                    document->uri);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_TRUE(shared.workspace.documentBuilder->build(std::vector{document}));
  EXPECT_TRUE(waiter.get());
}

TEST(ArchitectureTest, DocumentBuilderWaitUntilDocumentStateHandlesInvalidCases) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  EXPECT_FALSE(shared.workspace.documentBuilder->waitUntil(pegium::workspace::DocumentState::Parsed,
                                        "file:///missing.pg"));

  auto documentBuilt = open_or_update_and_refresh(
      shared,
      "file:///wait-built.pg", "ok", "mini");
  ASSERT_TRUE(documentBuilt != nullptr);
  auto documentNotBuilt =
      open_or_update(shared, "file:///wait-not-built.pg", "ok", "mini");
  ASSERT_TRUE(documentNotBuilt != nullptr);

  EXPECT_FALSE(shared.workspace.documentBuilder->waitUntil(pegium::workspace::DocumentState::Validated,
                                        documentNotBuilt->uri));
}
