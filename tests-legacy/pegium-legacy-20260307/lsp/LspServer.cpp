#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <lsp/connection.h>
#include <lsp/io/stream.h>
#include <lsp/messagehandler.h>

#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/DefaultLanguageFeatures.hpp>
#include <pegium/lsp/FuzzyMatcher.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/IndexManager.hpp>

using namespace pegium::parser;

namespace {

std::shared_ptr<pegium::workspace::Document>
open_or_update_and_refresh(pegium::services::SharedServices &shared,
                           std::string uri, std::string text,
                           std::string languageId) {
  auto document = shared.workspace.documents->openOrUpdate(
      std::move(uri), std::move(text), std::move(languageId));
  if (document != nullptr) {
    (void)shared.workspace.documentBuilder->build(std::vector{document});
  }
  return document;
}

::lsp::Position position_from_offset(std::string_view text,
                                     pegium::TextOffset offset) {
  std::uint32_t line = 0;
  std::uint32_t character = 0;
  for (pegium::TextOffset index = 0; index < offset && index < text.size();
       ++index) {
    if (text[index] == '\n') {
      ++line;
      character = 0;
      continue;
    }
    ++character;
  }
  return {.line = line, .character = character};
}

::lsp::WorkspaceSymbolParams workspace_symbol_params(std::string_view query) {
  ::lsp::WorkspaceSymbolParams params{};
  params.query = std::string(query);
  return params;
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

struct LocalAndExportScopeProvider final : pegium::references::ScopeComputation {
  std::vector<pegium::workspace::AstNodeDescription> collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &) const override {
    if (!document.parseSucceeded()) {
      return {};
    }

    return {{.name = document.text(),
             .type = "symbol",
             .documentUri = document.uri,
             .offset = 0,
             .path = "0"}};
  }

  pegium::workspace::LocalSymbols collectLocalSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &cancelToken) const override {
    pegium::workspace::LocalSymbols symbols;
    const auto *root = document.parseResult.value.get();
    if (root == nullptr) {
      return symbols;
    }
    for (const auto &symbol : collectExportedSymbols(document, cancelToken)) {
      symbols.emplace(root, symbol);
    }
    return symbols;
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

struct FixedReferences final : pegium::references::References {
  std::optional<pegium::workspace::AstNodeDescription>
  findDeclarationAt(const pegium::workspace::Document &document,
                    pegium::TextOffset) const override {
    if (document.text() != "foo") {
      return std::nullopt;
    }

    return pegium::workspace::AstNodeDescription{
        .name = "foo",
        .type = "definition",
        .documentUri = "file:///decl.pg",
        .offset = 4,
        .path = "4",
    };
  }

  pegium::utils::stream<pegium::workspace::ReferenceDescriptionOrDeclaration>
  findReferencesAt(const pegium::workspace::Document &document,
                   pegium::TextOffset,
                   bool includeDeclaration) const override {
    if (document.text() != "foo") {
      return pegium::utils::make_stream<
          pegium::workspace::ReferenceDescriptionOrDeclaration>(
          std::views::empty<pegium::workspace::ReferenceDescriptionOrDeclaration>);
    }

    std::vector<pegium::workspace::ReferenceDescriptionOrDeclaration> references{
        pegium::workspace::ReferenceDescription{.sourceUri = "file:///use-a.pg",
         .sourcePath = "1",
         .sourceOffset = 1,
         .sourceLength = 3,
         .referenceType = "definition",
         .targetName = "foo",
         .targetUri = "file:///decl.pg",
         .targetPath = "4"},
        pegium::workspace::ReferenceDescription{.sourceUri = "file:///use-b.pg",
         .sourcePath = "3",
         .sourceOffset = 3,
         .sourceLength = 3,
         .referenceType = "definition",
         .targetName = "foo",
         .targetUri = "file:///decl.pg",
         .targetPath = "4"},
    };

    if (includeDeclaration) {
      references.push_back(pegium::workspace::AstNodeDescription{
          .name = "foo",
          .type = "definition",
          .documentUri = "file:///decl.pg",
          .offset = 4,
          .path = "4"});
    }
    return pegium::utils::make_stream<
        pegium::workspace::ReferenceDescriptionOrDeclaration>(
        std::move(references));
  }
};

struct CapturingStream final : lsp::io::Stream {
  std::string written;

  void read(char *buffer, std::size_t size) override {
    for (std::size_t index = 0; index < size; ++index) {
      buffer[index] = Eof;
    }
  }

  void write(const char *buffer, std::size_t size) override {
    written.append(buffer, size);
  }
};

struct CountingWorkspaceSymbolProvider final
    : pegium::services::WorkspaceSymbolProvider {
  mutable std::size_t calls = 0;
  std::vector<pegium::services::WorkspaceSymbol> symbols;

  std::vector<pegium::services::WorkspaceSymbol>
  getWorkspaceSymbols(const ::lsp::WorkspaceSymbolParams &params,
                      const pegium::utils::CancellationToken &cancelToken) const override {
    (void)cancelToken;
    const std::string_view query = params.query;
    ++calls;

    std::vector<pegium::services::WorkspaceSymbol> matches;
    for (const auto &symbol : symbols) {
      if (query.empty() ||
          symbol.name.find(std::string(query)) != std::string::npos) {
        matches.push_back(symbol);
      }
    }
    return matches;
  }
};

struct CountingScopeProvider final : pegium::references::ScopeProvider {
  mutable std::size_t calls = 0;
  std::vector<pegium::workspace::AstNodeDescription> elements;

  std::shared_ptr<const pegium::references::Scope>
  getScope(const pegium::ReferenceInfo &) const override {
    ++calls;
    return std::make_shared<const pegium::references::StreamScope>(elements);
  }
};

struct CountingFuzzyMatcher final : pegium::lsp::FuzzyMatcher {
  mutable std::size_t matchesCalls = 0;
  mutable std::size_t scoreCalls = 0;

  bool matches(std::string_view text,
               std::string_view query) const override {
    ++matchesCalls;
    if (query.empty()) {
      return true;
    }
    return text.find(query) != std::string_view::npos;
  }

  std::uint32_t score(std::string_view text,
                      std::string_view query) const override {
    ++scoreCalls;
    if (query.empty()) {
      return 1;
    }
    return text == query ? 100 : 10;
  }
};

struct DuplicateExportScopeProvider final : pegium::references::ScopeComputation {
  std::vector<pegium::workspace::AstNodeDescription> collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &) const override {
    if (!document.parseSucceeded()) {
      return {};
    }
    return {
        {.name = "Zoo",
         .type = "symbol",
         .documentUri = document.uri,
         .offset = 3,
         .path = "3"},
        {.name = "Apple",
         .type = "symbol",
         .documentUri = document.uri,
         .offset = 1,
         .path = "1"},
        {.name = "Apple",
         .type = "symbol",
         .documentUri = document.uri,
         .offset = 1,
         .path = "1"},
    };
  }

  pegium::workspace::LocalSymbols collectLocalSymbols(
      const pegium::workspace::Document &,
      const pegium::utils::CancellationToken &) const override {
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

TEST(ArchitectureTest, LanguageServerPublishesDiagnosticsFromWorkspacePipeline) {
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

  CapturingStream stream;
  lsp::Connection connection(stream);
  lsp::MessageHandler handler(connection);
  pegium::lsp::DefaultLanguageServer server(shared, handler);

  EXPECT_TRUE(server.didOpen("file:///doc.pg", "mini", "ok"));
  EXPECT_NE(stream.written.find("textDocument/publishDiagnostics"),
            std::string::npos);
  EXPECT_EQ(stream.written.find("Parse failed"), std::string::npos);
  const auto writtenAfterOpen = stream.written.size();

  EXPECT_FALSE(server.didChange("file:///doc.pg", "ko"));
  EXPECT_NE(stream.written.find("Parse failed", writtenAfterOpen),
            std::string::npos);

  EXPECT_TRUE(server.didClose("file:///doc.pg"));
  EXPECT_FALSE(server.didClose("file:///doc.pg"));
}

TEST(ArchitectureTest, LanguageServerPublishesDiagnosticsWithDocumentVersion) {
  ParserRule<TokenNode> root{"Root", assign<&TokenNode::token>("ok"_kw)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini",
                                                    std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  CapturingStream stream;
  lsp::Connection connection(stream);
  lsp::MessageHandler handler(connection);
  pegium::lsp::DefaultLanguageServer server(shared, handler);

  EXPECT_TRUE(server.didOpen("file:///doc.pg", "mini", "ok"));
  EXPECT_NE(stream.written.find("\"version\":1"), std::string::npos);

  const auto writtenAfterOpen = stream.written.size();
  EXPECT_FALSE(server.didChange("file:///doc.pg", "ko"));
  EXPECT_NE(stream.written.find("\"version\":2", writtenAfterOpen),
            std::string::npos);
}

TEST(ArchitectureTest, LanguageServerCanUseEmbeddedMessageHandler) {
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

  CapturingStream stream;
  lsp::Connection connection(stream);
  lsp::MessageHandler handler(connection);
  pegium::lsp::DefaultLanguageServer server(shared, handler);

  EXPECT_FALSE(server.didOpen("file:///msg.pg", "mini", "ko"));
  EXPECT_NE(stream.written.find("textDocument/publishDiagnostics"),
            std::string::npos);
  EXPECT_NE(stream.written.find("Parse failed"), std::string::npos);
}

TEST(ArchitectureTest, LanguageServerProvidesDefinitionReferencesAndRename) {
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


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::move(scopeComputation);
  languageServices->references.linker = std::move(linker);
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto definition =
      open_or_update_and_refresh(shared, "file:///def.pg", "Root", "mini");
  ASSERT_TRUE(definition != nullptr);
  auto usage =
      open_or_update_and_refresh(shared, "file:///use.pg", "Root", "mini");
  ASSERT_TRUE(usage != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);

  ::lsp::DefinitionParams definitionParams{};
  definitionParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  definitionParams.position = {.line = 0, .character = 1};
  auto definitionViaParams = server.getDefinition(definitionParams);
  ASSERT_TRUE(definitionViaParams.has_value());
  EXPECT_EQ(definitionViaParams->uri, "file:///def.pg");

  ::lsp::TypeDefinitionParams typeDefinitionParams{};
  typeDefinitionParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  typeDefinitionParams.position = {.line = 0, .character = 1};
  auto typeDefinitionViaParams = server.getTypeDefinition(typeDefinitionParams);
  ASSERT_TRUE(typeDefinitionViaParams.has_value());
  EXPECT_EQ(typeDefinitionViaParams->uri, "file:///def.pg");

  ::lsp::ImplementationParams implementationParams{};
  implementationParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  implementationParams.position = {.line = 0, .character = 1};
  auto implementationViaParams = server.getImplementation(implementationParams);
  ASSERT_TRUE(implementationViaParams.has_value());
  EXPECT_EQ(implementationViaParams->uri, "file:///def.pg");

  ::lsp::DeclarationParams declarationParams{};
  declarationParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  declarationParams.position = {.line = 0, .character = 1};
  auto declarationViaParams = server.getDeclaration(declarationParams);
  ASSERT_TRUE(declarationViaParams.has_value());
  EXPECT_EQ(declarationViaParams->uri, "file:///def.pg");

  ::lsp::DocumentHighlightParams highlightParams{};
  highlightParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  highlightParams.position = {.line = 0, .character = 1};
  const auto highlights = server.getDocumentHighlights(highlightParams);
  ASSERT_EQ(highlights.size(), 1u);
  EXPECT_EQ(highlights.front().begin, 0u);
  EXPECT_EQ(highlights.front().kind, ::lsp::DocumentHighlightKind::Read);
  const auto highlightsViaParams = server.getDocumentHighlights(highlightParams);
  ASSERT_EQ(highlightsViaParams.size(), 1u);
  EXPECT_EQ(highlightsViaParams.front().begin, 0u);

  ::lsp::ReferenceParams referencesParams{};
  referencesParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  referencesParams.position = {.line = 0, .character = 1};
  referencesParams.context.includeDeclaration = true;
  const auto referencesViaParams = server.getReferences(referencesParams);
  ASSERT_GE(referencesViaParams.size(), 2u);

  ::lsp::PrepareRenameParams prepareRenameParams{};
  prepareRenameParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  prepareRenameParams.position = {.line = 0, .character = 1};
  auto prepareRenameViaParams = server.prepareRename(prepareRenameParams);
  ASSERT_TRUE(prepareRenameViaParams.has_value());
  EXPECT_EQ(prepareRenameViaParams->begin, 0u);
  EXPECT_EQ(prepareRenameViaParams->placeholder, "Root");

  ::lsp::RenameParams renameParams{};
  renameParams.textDocument.uri = ::lsp::Uri::parse("file:///use.pg");
  renameParams.position = {.line = 0, .character = 1};
  renameParams.newName = "Catalogue";
  auto renameViaParams = server.rename(renameParams);
  ASSERT_TRUE(renameViaParams.has_value());
  EXPECT_TRUE(renameViaParams->changes.contains("file:///def.pg"));
  EXPECT_TRUE(renameViaParams->changes.contains("file:///use.pg"));

  auto workspaceSymbols = server.getWorkspaceSymbols(
      workspace_symbol_params("roo"));
  ASSERT_FALSE(workspaceSymbols.empty());
  EXPECT_EQ(workspaceSymbols.front().name, "Root");
}

TEST(ArchitectureTest, LanguageServerWorkspaceSymbolsUseSharedProviderOnly) {
  pegium::services::SharedServices shared;
  auto provider = std::make_unique<CountingWorkspaceSymbolProvider>();
  auto *providerPtr = provider.get();
  provider->symbols.push_back(
      {.name = "Root",
       .kind = "symbol",
       .uri = "file:///shared.pg",
       .begin = 0,
       .end = 4,
       .containerName = {}});
  shared.lsp.workspaceSymbolProvider = std::move(provider);

  pegium::lsp::DefaultLanguageServer server(shared);
  const auto symbols = server.getWorkspaceSymbols(workspace_symbol_params("Ro"));

  ASSERT_EQ(providerPtr->calls, 1u);
  ASSERT_EQ(symbols.size(), 1u);
  EXPECT_EQ(symbols.front().name, "Root");
}

TEST(ArchitectureTest, LanguageServerWorkspaceSymbolsAreGlobalAcrossLanguages) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserA = make_test_rule_parser(token, SkipperBuilder().build(), options);
  auto parserB = make_test_rule_parser(token, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageA =
      pegium::services::makeDefaultServices(shared, "miniA", std::move(parserA));
  languageA->references.scopeComputation = std::make_unique<LocalAndExportScopeProvider>();
  languageA->references.linker = std::make_unique<CountingLinker>();
  languageA->validation.documentValidator = std::make_unique<ParseFailValidator>();

  auto languageB =
      pegium::services::makeDefaultServices(shared, "miniB", std::move(parserB));
  languageB->references.scopeComputation = std::make_unique<LocalAndExportScopeProvider>();
  languageB->references.linker = std::make_unique<CountingLinker>();
  languageB->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageA)));
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageB)));

  auto alpha =
      open_or_update_and_refresh(shared, "file:///alpha.pg", "Alpha", "miniA");
  ASSERT_TRUE(alpha != nullptr);
  auto beta =
      open_or_update_and_refresh(shared, "file:///beta.pg", "Beta", "miniB");
  ASSERT_TRUE(beta != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  const auto symbols = server.getWorkspaceSymbols(workspace_symbol_params(""));

  bool hasAlpha = false;
  bool hasBeta = false;
  for (const auto &symbol : symbols) {
    hasAlpha = hasAlpha || symbol.name == "Alpha";
    hasBeta = hasBeta || symbol.name == "Beta";
  }
  EXPECT_TRUE(hasAlpha);
  EXPECT_TRUE(hasBeta);
}

TEST(ArchitectureTest, LanguageServerWorkspaceSymbolsAreSortedAndDeduplicated) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation =
      std::make_unique<DuplicateExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///dedup.pg", "Word", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  const auto symbols = server.getWorkspaceSymbols(workspace_symbol_params(""));

  ASSERT_EQ(symbols.size(), 2u);
  EXPECT_EQ(symbols[0].name, "Apple");
  EXPECT_EQ(symbols[1].name, "Zoo");
}

TEST(ArchitectureTest, LanguageServerCompletionUsesInjectedScopeAndSharedFuzzyMatcher) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);

  auto scopeProvider = std::make_unique<CountingScopeProvider>();
  auto *scopeProviderPtr = scopeProvider.get();
  scopeProviderPtr->elements.push_back(
      {.name = "RootFromScope",
       .type = "symbol",
       .documentUri = "file:///scope.pg",
       .offset = 0,
       .path = "0"});
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeProvider = std::move(scopeProvider);
  languageServices->references.scopeComputation =
      std::make_unique<LocalAndExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  auto fuzzyMatcher = std::make_unique<CountingFuzzyMatcher>();
  auto *fuzzyMatcherPtr = fuzzyMatcher.get();
  shared.lsp.fuzzyMatcher = std::move(fuzzyMatcher);
  pegium::lsp::installDefaultLspServices(*languageServices);
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///completion.pg", "Root", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  ::lsp::CompletionParams completionParams{};
  completionParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  completionParams.position = {.line = 0, .character = 2};
  const auto completion = server.getCompletion(completionParams);

  ASSERT_EQ(scopeProviderPtr->calls, 1u);
  EXPECT_GT(fuzzyMatcherPtr->matchesCalls, 0u);
  EXPECT_GT(fuzzyMatcherPtr->scoreCalls, 0u);

  ASSERT_EQ(completion.size(), 1u);
  EXPECT_EQ(completion.front().label, "RootFromScope");
}

TEST(ArchitectureTest, LanguageServerWorkspaceSymbolsUseSharedFuzzyMatcher) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation =
      std::make_unique<LocalAndExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  auto fuzzyMatcher = std::make_unique<CountingFuzzyMatcher>();
  auto *fuzzyMatcherPtr = fuzzyMatcher.get();
  shared.lsp.fuzzyMatcher = std::move(fuzzyMatcher);
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///workspace-symbol.pg", "Alpha", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  const auto symbols = server.getWorkspaceSymbols(workspace_symbol_params("ph"));

  EXPECT_GT(fuzzyMatcherPtr->matchesCalls, 0u);
  EXPECT_GT(fuzzyMatcherPtr->scoreCalls, 0u);
  ASSERT_FALSE(symbols.empty());
  EXPECT_EQ(symbols.front().name, "Alpha");
}

TEST(ArchitectureTest, LanguageServerUsesReferencesServiceForSymbolQueries) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.references = std::make_unique<FixedReferences>();
  pegium::lsp::installDefaultLspServices(*languageServices);

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  pegium::lsp::DefaultLanguageServer server(shared);
  ASSERT_TRUE(server.didOpen("file:///input.pg", "mini", "foo"));

  ::lsp::DefinitionParams definitionParams{};
  definitionParams.textDocument.uri = ::lsp::Uri::parse("file:///input.pg");
  definitionParams.position = {.line = 0, .character = 0};
  const auto definition = server.getDefinition(definitionParams);
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->uri, "file:///decl.pg");
  EXPECT_EQ(definition->begin, 4u);
  EXPECT_EQ(definition->end, 7u);

  ::lsp::ReferenceParams referencesParams{};
  referencesParams.textDocument.uri = ::lsp::Uri::parse("file:///input.pg");
  referencesParams.position = {.line = 0, .character = 0};
  referencesParams.context.includeDeclaration = true;
  const auto references = server.getReferences(referencesParams);
  ASSERT_EQ(references.size(), 3u);

  ::lsp::RenameParams renameParams{};
  renameParams.textDocument.uri = ::lsp::Uri::parse("file:///input.pg");
  renameParams.position = {.line = 0, .character = 0};
  renameParams.newName = "bar";
  const auto rename = server.rename(renameParams);
  ASSERT_TRUE(rename.has_value());
  EXPECT_EQ(rename->changes.size(), 3u);
  for (const auto &[uri, edits] : rename->changes) {
    (void)uri;
    ASSERT_EQ(edits.size(), 1u);
    EXPECT_EQ(edits.front().newText, "bar");
  }
}

TEST(ArchitectureTest, LanguageServerProvidesCodeActionsFromDiagnostics) {
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

  auto document = open_or_update_and_refresh(
      shared,
      "file:///actions.pg", "ok", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  ::lsp::CodeActionParams codeActionParams{};
  codeActionParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  codeActionParams.range = {
      .start = {.line = 0, .character = 0},
      .end = {.line = 0, .character = 0}};
  ::lsp::Diagnostic lspDiagnostic{};
  lspDiagnostic.severity = ::lsp::DiagnosticSeverity::Error;
  lspDiagnostic.message = "synthetic";
  lspDiagnostic.source = "test";
  lspDiagnostic.range = codeActionParams.range;
  codeActionParams.context.diagnostics.push_back(std::move(lspDiagnostic));
  auto actions = server.getCodeActions(codeActionParams);

  ASSERT_FALSE(actions.empty());
  EXPECT_EQ(actions.front().title, "Remove unexpected character");
  EXPECT_TRUE(actions.front().edit.changes.contains(document->uri));
}

TEST(ArchitectureTest, LanguageServerProvidesCompletionHoverAndDocumentSymbols) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> token{"Token", assign<&TokenNode::token>(tokenText)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(token, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation =
      std::make_unique<LocalAndExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///symbols.pg", "Root", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);

  ::lsp::CompletionParams completionParams{};
  completionParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  completionParams.position = {.line = 0, .character = 4};
  auto completionViaParams = server.getCompletion(completionParams);
  auto completion = completionViaParams;
  ASSERT_FALSE(completionViaParams.empty());
  EXPECT_EQ(completionViaParams.front().label, "Root");

  ::lsp::HoverParams hoverParams{};
  hoverParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  hoverParams.position = {.line = 0, .character = 1};
  auto hoverViaParams = server.getHoverContent(hoverParams);
  auto hover = hoverViaParams;
  ASSERT_TRUE(hoverViaParams.has_value());
  EXPECT_NE(hoverViaParams->contents.find("Symbol: Root"), std::string::npos);

  ::lsp::SignatureHelpParams signatureHelpParams{};
  signatureHelpParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  signatureHelpParams.position = {.line = 0, .character = 1};
  auto signatureHelpViaParams = server.getSignatureHelp(signatureHelpParams);
  auto signatureHelp = signatureHelpViaParams;
  ASSERT_TRUE(signatureHelpViaParams.has_value());
  ASSERT_FALSE(signatureHelpViaParams->signatures.empty());
  EXPECT_EQ(signatureHelp->signatures.front().label, "Root(...)");

  ::lsp::CodeLensParams codeLensParams{};
  codeLensParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  const auto codeLens = server.getCodeLens(codeLensParams);
  EXPECT_TRUE(codeLens.empty());

  lsp::FormattingOptions formatting{};
  formatting.tabSize = 2;
  formatting.insertSpaces = true;
  ::lsp::DocumentFormattingParams formattingParams{};
  formattingParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  formattingParams.options = formatting;
  EXPECT_TRUE(server.formatDocument(formattingParams).empty());
  ::lsp::DocumentRangeFormattingParams rangeFormattingParams{};
  rangeFormattingParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  rangeFormattingParams.options = formatting;
  rangeFormattingParams.range = {
      .start = {.line = 0, .character = 0},
      .end = {.line = 0, .character = 4}};
  EXPECT_TRUE(server.formatDocumentRange(rangeFormattingParams).empty());

  ::lsp::InlayHintParams inlayHintParams{};
  inlayHintParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  inlayHintParams.range = {
      .start = {.line = 0, .character = 0},
      .end = {.line = 0, .character = 4}};
  EXPECT_TRUE(server.getInlayHints(inlayHintParams).empty());

  ::lsp::SemanticTokensParams semanticTokensParams{};
  semanticTokensParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  auto semanticTokensViaParams = server.getSemanticTokensFull(semanticTokensParams);
  auto semanticTokens = semanticTokensViaParams;
  ASSERT_TRUE(semanticTokensViaParams.has_value());
  EXPECT_TRUE(semanticTokensViaParams->data.empty());
  ::lsp::SemanticTokensRangeParams semanticTokensRangeParams{};
  semanticTokensRangeParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  semanticTokensRangeParams.range = {
      .start = {.line = 0, .character = 0},
      .end = {.line = 0, .character = 4}};
  auto semanticTokensRangeViaParams =
      server.getSemanticTokensRange(semanticTokensRangeParams);
  auto semanticTokensRange = semanticTokensRangeViaParams;
  ASSERT_TRUE(semanticTokensRangeViaParams.has_value());
  EXPECT_TRUE(semanticTokensRangeViaParams->data.empty());

  EXPECT_TRUE(server.getExecuteCommands().empty());
  lsp::LSPArray commandArguments{};
  ::lsp::ExecuteCommandParams executeCommandParams{};
  executeCommandParams.command = "noop";
  executeCommandParams.arguments = commandArguments;
  EXPECT_FALSE(server.executeCommand(executeCommandParams).has_value());

  ::lsp::CallHierarchyPrepareParams callHierarchyPrepareParams{};
  callHierarchyPrepareParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  callHierarchyPrepareParams.position = {.line = 0, .character = 1};
  EXPECT_TRUE(server.prepareCallHierarchy(callHierarchyPrepareParams).empty());
  ::lsp::TypeHierarchyPrepareParams typeHierarchyPrepareParams{};
  typeHierarchyPrepareParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  typeHierarchyPrepareParams.position = {.line = 0, .character = 1};
  EXPECT_TRUE(server.prepareTypeHierarchy(typeHierarchyPrepareParams).empty());

  ::lsp::DocumentSymbolParams symbolParams{};
  symbolParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  auto symbolsViaParams = server.getDocumentSymbols(symbolParams);
  auto symbols = symbolsViaParams;
  ASSERT_FALSE(symbolsViaParams.empty());
  EXPECT_EQ(symbolsViaParams.front().label, "Root");
}

TEST(ArchitectureTest, LanguageServerHoverDoesNotCrashOnRecoveredDeletedToken) {
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<TokenNode> root{"Root",
                             assign<&TokenNode::token>(tokenText) + ";"_kw};

  ParseOptions options;
  options.recoveryEnabled = true;

  auto parserService =
      make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices = pegium::services::makeDefaultServices(
      shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation =
      std::make_unique<LocalAndExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator =
      std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///hover-recovered.pg", "Root+;", "mini");
  ASSERT_TRUE(document != nullptr);
  ASSERT_TRUE(document->parseResult.value != nullptr);
  ASSERT_FALSE(document->parseResult.parseDiagnostics.empty());

  const auto plusPos = document->text().find('+');
  ASSERT_NE(plusPos, std::string::npos);
  const auto plusOffset = static_cast<pegium::TextOffset>(plusPos);

  pegium::lsp::DefaultLanguageServer server(shared);
  ::lsp::HoverParams hoverParams{};
  hoverParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  hoverParams.position = position_from_offset(document->text(), plusOffset);
  EXPECT_NO_THROW(
      (void)server.getHoverContent(hoverParams));
}

TEST(ArchitectureTest, LanguageServerHoverDoesNotCrashOnRecoveredDanglingOperator) {
  DataTypeRule<std::string> identifier{"Identifier", some(w)};
  const auto spaces = many(s);

  ParserRule<TokenNode> term{"Term", assign<&TokenNode::token>(identifier)};
  ParserRule<TokenNode> expression{"Expression",
                                   term + many(spaces + "+"_kw + spaces + term)};
  ParserRule<TokenNode> statement{"Statement", expression + spaces + ";"_kw};
  ParserRule<TokenNode> root{"Root", statement + many(statement)};

  ParseOptions options;
  options.recoveryEnabled = true;

  auto parserService = make_test_rule_parser(root, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;

  auto languageServices = pegium::services::makeDefaultServices(
      shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation =
      std::make_unique<LocalAndExportScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator =
      std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///hover-dangling-operator.pg", "abc     +   ;", "mini");
  ASSERT_TRUE(document != nullptr);
  ASSERT_TRUE(document->parseResult.value != nullptr);
  ASSERT_FALSE(document->parseResult.parseDiagnostics.empty());

  const auto plusPos = document->text().find('+');
  ASSERT_NE(plusPos, std::string::npos);
  const auto plusOffset = static_cast<pegium::TextOffset>(plusPos);

  pegium::lsp::DefaultLanguageServer server(shared);
  ::lsp::HoverParams hoverParams{};
  hoverParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  hoverParams.position = position_from_offset(document->text(), plusOffset);
  EXPECT_NO_THROW(
      (void)server.getHoverContent(hoverParams));
}

TEST(ArchitectureTest, LanguageServerProvidesFoldingRanges) {
  DataTypeRule<std::string> text{"Text", some(dot)};
  ParserRule<TokenNode> file{"File", assign<&TokenNode::token>(text)};

  ParseOptions options;
  options.recoveryEnabled = false;

  auto parserService =
      make_test_rule_parser(file, SkipperBuilder().build(), options);
  pegium::services::SharedServices shared;


  auto languageServices =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parserService));
  languageServices->references.scopeComputation = std::make_unique<FixedScopeProvider>();
  languageServices->references.linker = std::make_unique<CountingLinker>();
  languageServices->validation.documentValidator = std::make_unique<ParseFailValidator>();

  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(languageServices)));

  auto document = open_or_update_and_refresh(
      shared,
      "file:///fold.pg", "section {\n  item https://example.com\n}\n", "mini");
  ASSERT_TRUE(document != nullptr);

  pegium::lsp::DefaultLanguageServer server(shared);
  ::lsp::FoldingRangeParams foldingParams{};
  foldingParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  const auto ranges = server.getFoldingRanges(foldingParams);

  ASSERT_FALSE(ranges.empty());
  EXPECT_GT(ranges.front().end, ranges.front().begin);

  ::lsp::DocumentLinkParams linkParams{};
  linkParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  const auto links = server.getDocumentLinks(linkParams);
  ASSERT_FALSE(links.empty());
  EXPECT_EQ(links.front().targetUri, "https://example.com");

  ::lsp::SelectionRangeParams selectionParams{};
  selectionParams.textDocument.uri = ::lsp::Uri::parse(document->uri);
  selectionParams.positions.push_back(position_from_offset(document->text(), 12));
  const auto selectionRanges = server.getSelectionRanges(selectionParams);
  ASSERT_FALSE(selectionRanges.empty());
  ASSERT_FALSE(selectionRanges.front().empty());
  EXPECT_LE(selectionRanges.front().front().begin,
            selectionRanges.front().front().end);
}
