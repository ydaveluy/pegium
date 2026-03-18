#include <gtest/gtest.h>

#include <type_traits>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/CallHierarchyProvider.hpp>
#include <pegium/lsp/CodeLensProvider.hpp>
#include <pegium/lsp/DeclarationProvider.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/DocumentLinkProvider.hpp>
#include <pegium/lsp/ExecuteCommandHandler.hpp>
#include <pegium/lsp/FileOperationHandler.hpp>
#include <pegium/lsp/ImplementationProvider.hpp>
#include <pegium/lsp/InlayHintProvider.hpp>
#include <pegium/lsp/SelectionRangeProvider.hpp>
#include <pegium/lsp/SemanticTokenProvider.hpp>
#include <pegium/lsp/SignatureHelpProvider.hpp>
#include <pegium/lsp/TypeDefinitionProvider.hpp>
#include <pegium/lsp/TypeHierarchyProvider.hpp>
#include <pegium/lsp/WorkspaceSymbolProvider.hpp>

namespace pegium::lsp {
namespace {

class TestSelectionRangeProvider final : public services::SelectionRangeProvider {
public:
  std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &,
                     const ::lsp::SelectionRangeParams &,
                     const utils::CancellationToken &) const override {
    return {};
  }
};

class TestSavingDocumentUpdateHandler final : public DocumentUpdateHandler {
public:
  [[nodiscard]] bool supportsDidSaveDocument() const noexcept override {
    return true;
  }
};

class TestExecuteCommandHandler final : public ExecuteCommandHandler {
public:
  std::vector<std::string> commands() const override {
    return {"test.command"};
  }

  std::optional<::lsp::LSPAny>
  executeCommand(std::string_view, const ::lsp::LSPArray &,
                 const utils::CancellationToken &) const override {
    return std::nullopt;
  }
};

class TestFileOperationHandler final : public FileOperationHandler {
public:
  [[nodiscard]] bool supportsDidCreateFiles() const noexcept override {
    return true;
  }

  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override {
    return options;
  }

private:
  ::lsp::FileOperationOptions options = [] {
    ::lsp::FileOperationFilter fileFilter{};
    fileFilter.pattern.glob = "**/*";
    fileFilter.scheme = "file";

    ::lsp::FileOperationRegistrationOptions registration{};
    registration.filters.push_back(std::move(fileFilter));

    ::lsp::FileOperationOptions out{};
    out.didCreate = registration;
    return out;
  }();
};

class DeclaredButUnsupportedFileOperationHandler final
    : public FileOperationHandler {
public:
  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override {
    return options;
  }

private:
  ::lsp::FileOperationOptions options = [] {
    ::lsp::FileOperationFilter fileFilter{};
    fileFilter.pattern.glob = "**/*";
    fileFilter.scheme = "file";

    ::lsp::FileOperationRegistrationOptions registration{};
    registration.filters.push_back(std::move(fileFilter));

    ::lsp::FileOperationOptions out{};
    out.didCreate = registration;
    return out;
  }();
};

class TestWorkspaceSymbolProvider final : public services::WorkspaceSymbolProvider {
public:
  explicit TestWorkspaceSymbolProvider(bool resolveProvider)
      : resolveProvider(resolveProvider) {}

  std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &,
             const utils::CancellationToken &) const override {
    return {};
  }

  [[nodiscard]] bool supportsResolveSymbol() const noexcept override {
    return resolveProvider;
  }

  std::optional<::lsp::WorkspaceSymbol>
  resolveSymbol(const ::lsp::WorkspaceSymbol &symbol,
                const utils::CancellationToken &) const override {
    if (!resolveProvider) {
      return std::nullopt;
    }
    return symbol;
  }

private:
  bool resolveProvider = false;
};

class TestFormatter : public services::Formatter {
public:
  std::vector<::lsp::TextEdit>
  formatDocument(const workspace::Document &, const ::lsp::DocumentFormattingParams &,
                 const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::TextEdit>
  formatDocumentRange(
      const workspace::Document &, const ::lsp::DocumentRangeFormattingParams &,
      const utils::CancellationToken &) const override {
    return {};
  }
};

class TestOnTypeFormatter final : public TestFormatter {
public:
  std::optional<::lsp::DocumentOnTypeFormattingOptions>
  formatOnTypeOptions() const noexcept override {
    ::lsp::DocumentOnTypeFormattingOptions options{};
    options.firstTriggerCharacter = ";";
    options.moreTriggerCharacter = ::lsp::Array<::lsp::String>{","};
    return options;
  }
};

class TestSignatureHelpProvider final : public services::SignatureHelpProvider {
public:
  ::lsp::SignatureHelpOptions signatureHelpOptions() const override {
    ::lsp::SignatureHelpOptions options{};
    options.triggerCharacters = ::lsp::Array<::lsp::String>{"("};
    return options;
  }

  std::optional<::lsp::SignatureHelp>
  provideSignatureHelp(const workspace::Document &,
                       const ::lsp::SignatureHelpParams &,
                       const utils::CancellationToken &) const override {
    return ::lsp::SignatureHelp{};
  }
};

class TestDeclarationProvider final : public services::DeclarationProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getDeclaration(const workspace::Document &, const ::lsp::DeclarationParams &,
                 const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestTypeDefinitionProvider final : public services::TypeDefinitionProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &,
                    const ::lsp::TypeDefinitionParams &,
                    const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestImplementationProvider final : public services::ImplementationProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &,
                    const ::lsp::ImplementationParams &,
                    const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestCodeLensProvider final : public services::CodeLensProvider {
public:
  std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &, const ::lsp::CodeLensParams &,
                  const utils::CancellationToken &) const override {
    return {};
  }
};

class TestDocumentLinkProvider final : public services::DocumentLinkProvider {
public:
  std::vector<::lsp::DocumentLink>
  getDocumentLinks(const workspace::Document &,
                   const ::lsp::DocumentLinkParams &,
                   const utils::CancellationToken &) const override {
    return {};
  }
};

class TestCallHierarchyProvider final : public services::CallHierarchyProvider {
public:
  std::vector<::lsp::CallHierarchyItem>
  prepareCallHierarchy(const workspace::Document &,
                       const ::lsp::CallHierarchyPrepareParams &,
                       const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::CallHierarchyIncomingCall>
  incomingCalls(const ::lsp::CallHierarchyIncomingCallsParams &,
                const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::CallHierarchyOutgoingCall>
  outgoingCalls(const ::lsp::CallHierarchyOutgoingCallsParams &,
                const utils::CancellationToken &) const override {
    return {};
  }
};

class TestTypeHierarchyProvider final : public services::TypeHierarchyProvider {
public:
  std::vector<::lsp::TypeHierarchyItem>
  prepareTypeHierarchy(const workspace::Document &,
                       const ::lsp::TypeHierarchyPrepareParams &,
                       const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::TypeHierarchyItem>
  supertypes(const ::lsp::TypeHierarchySupertypesParams &,
             const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::TypeHierarchyItem>
  subtypes(const ::lsp::TypeHierarchySubtypesParams &,
           const utils::CancellationToken &) const override {
    return {};
  }
};

class TestInlayHintProvider final : public services::InlayHintProvider {
public:
  std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &, const ::lsp::InlayHintParams &,
                const utils::CancellationToken &) const override {
    return {};
  }
};

class TestSemanticTokenProvider final : public services::SemanticTokenProvider {
public:
  explicit TestSemanticTokenProvider(std::string tokenType = "type",
                                     std::string tokenModifier = "declaration")
      : tokenType(std::move(tokenType)),
        tokenModifier(std::move(tokenModifier)) {}

  StringIndexMap tokenTypes() const override {
    return {{tokenType, 0}};
  }

  StringIndexMap tokenModifiers() const override {
    return {{tokenModifier, 1}};
  }

  ::lsp::SemanticTokensOptions semanticTokensOptions() const override {
    ::lsp::SemanticTokensOptions options{};
    options.legend.tokenTypes.push_back(tokenType);
    options.legend.tokenModifiers.push_back(tokenModifier);
    options.range = true;
    ::lsp::SemanticTokensOptionsFull full{};
    full.delta = false;
    options.full = full;
    return options;
  }

  std::optional<::lsp::SemanticTokens>
  semanticHighlight(const workspace::Document &,
                    const ::lsp::SemanticTokensParams &,
                    const utils::CancellationToken &) const override {
    return ::lsp::SemanticTokens{};
  }

  std::optional<::lsp::SemanticTokens>
  semanticHighlightRange(const workspace::Document &,
                         const ::lsp::SemanticTokensRangeParams &,
                         const utils::CancellationToken &) const override {
    return ::lsp::SemanticTokens{};
  }

private:
  std::string tokenType;
  std::string tokenModifier;
};

TEST(DefaultLanguageServerTest, InitializeAdvertisesCapabilitiesFromRegisteredServices) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "test", {".test"})));

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.textDocumentSync.has_value());
  const auto &textSync =
      std::get<::lsp::TextDocumentSyncOptions>(*result.capabilities.textDocumentSync);
  EXPECT_TRUE(textSync.openClose.value_or(false));
  EXPECT_EQ(textSync.change, ::lsp::TextDocumentSyncKind::Incremental);
  EXPECT_FALSE(std::visit(
      [](const auto &save) {
        using Save = std::decay_t<decltype(save)>;
        if constexpr (std::is_same_v<Save, bool>) {
          return save;
        } else {
          return true;
        }
      },
      *textSync.save));

  EXPECT_TRUE(result.capabilities.completionProvider.has_value());
  ASSERT_TRUE(result.capabilities.hoverProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.hoverProvider));
  ASSERT_TRUE(result.capabilities.definitionProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.definitionProvider));
  ASSERT_TRUE(result.capabilities.renameProvider.has_value());
  EXPECT_TRUE(
      std::get<::lsp::RenameOptions>(*result.capabilities.renameProvider)
          .prepareProvider.value_or(false));
  EXPECT_FALSE(result.capabilities.selectionRangeProvider.has_value());
  EXPECT_FALSE(result.capabilities.signatureHelpProvider.has_value());
  ASSERT_TRUE(result.capabilities.codeActionProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.codeActionProvider));
  EXPECT_FALSE(result.capabilities.executeCommandProvider.has_value());
  ASSERT_TRUE(result.capabilities.workspaceSymbolProvider.has_value());
  EXPECT_FALSE(std::visit(
      [](const auto &options) {
        using Options = std::decay_t<decltype(options)>;
        if constexpr (std::is_same_v<Options, bool>) {
          return options;
        } else {
          return options.resolveProvider.value_or(false);
        }
      },
      *result.capabilities.workspaceSymbolProvider));
  ASSERT_TRUE(result.serverInfo.has_value());
  EXPECT_EQ(result.serverInfo->name, "pegium");
}

TEST(DefaultLanguageServerTest, OptInServicesAdvertiseCapabilitiesWhenInstalledExplicitly) {
  auto shared = test::make_shared_services();
  auto services = test::make_services(*shared, "test", {".test"});
  services->lsp.selectionRangeProvider =
      std::make_unique<TestSelectionRangeProvider>();
  services->lsp.signatureHelp = std::make_unique<TestSignatureHelpProvider>();
  services->lsp.declarationProvider =
      std::make_unique<TestDeclarationProvider>();
  services->lsp.typeProvider = std::make_unique<TestTypeDefinitionProvider>();
  services->lsp.implementationProvider =
      std::make_unique<TestImplementationProvider>();
  services->lsp.codeLensProvider = std::make_unique<TestCodeLensProvider>();
  services->lsp.documentLinkProvider =
      std::make_unique<TestDocumentLinkProvider>();
  services->lsp.formatter = std::make_unique<TestFormatter>();
  services->lsp.callHierarchyProvider =
      std::make_unique<TestCallHierarchyProvider>();
  services->lsp.typeHierarchyProvider =
      std::make_unique<TestTypeHierarchyProvider>();
  services->lsp.inlayHintProvider = std::make_unique<TestInlayHintProvider>();
  services->lsp.semanticTokenProvider =
      std::make_unique<TestSemanticTokenProvider>();

  shared->lsp.executeCommandHandler =
      std::make_unique<TestExecuteCommandHandler>();
  shared->lsp.fileOperationHandler =
      std::make_unique<TestFileOperationHandler>();
  shared->lsp.workspaceSymbolProvider =
      std::make_unique<TestWorkspaceSymbolProvider>(true);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.selectionRangeProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.selectionRangeProvider));

  ASSERT_TRUE(result.capabilities.signatureHelpProvider.has_value());
  ASSERT_TRUE(
      result.capabilities.signatureHelpProvider->triggerCharacters.has_value());
  EXPECT_EQ((*result.capabilities.signatureHelpProvider->triggerCharacters)[0],
            "(");

  ASSERT_TRUE(result.capabilities.declarationProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.declarationProvider));
  ASSERT_TRUE(result.capabilities.typeDefinitionProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.typeDefinitionProvider));
  ASSERT_TRUE(result.capabilities.implementationProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.implementationProvider));

  ASSERT_TRUE(result.capabilities.executeCommandProvider.has_value());
  EXPECT_EQ(result.capabilities.executeCommandProvider->commands,
            std::vector<std::string>{"test.command"});

  ASSERT_TRUE(result.capabilities.workspace.has_value());
  ASSERT_TRUE(result.capabilities.workspace->fileOperations.has_value());
  EXPECT_TRUE(result.capabilities.workspace->fileOperations->didCreate.has_value());
  ASSERT_TRUE(result.capabilities.workspaceSymbolProvider.has_value());
  EXPECT_TRUE(std::visit(
      [](const auto &options) {
        using Options = std::decay_t<decltype(options)>;
        if constexpr (std::is_same_v<Options, bool>) {
          return options;
        } else {
          return options.resolveProvider.value_or(false);
        }
      },
      *result.capabilities.workspaceSymbolProvider));
  ASSERT_TRUE(result.capabilities.codeLensProvider.has_value());
  EXPECT_FALSE(result.capabilities.codeLensProvider->resolveProvider.value_or(false));
  ASSERT_TRUE(result.capabilities.documentLinkProvider.has_value());
  EXPECT_FALSE(
      result.capabilities.documentLinkProvider->resolveProvider.value_or(false));
  ASSERT_TRUE(result.capabilities.documentFormattingProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.documentFormattingProvider));
  ASSERT_TRUE(result.capabilities.documentRangeFormattingProvider.has_value());
  EXPECT_TRUE(
      std::get<bool>(*result.capabilities.documentRangeFormattingProvider));
  EXPECT_FALSE(result.capabilities.documentOnTypeFormattingProvider.has_value());
  ASSERT_TRUE(result.capabilities.callHierarchyProvider.has_value());
  ASSERT_TRUE(result.capabilities.typeHierarchyProvider.has_value());
  ASSERT_TRUE(result.capabilities.inlayHintProvider.has_value());
  EXPECT_FALSE(std::visit(
      [](const auto &options) {
        using Options = std::decay_t<decltype(options)>;
        if constexpr (std::is_same_v<Options, bool>) {
          return options;
        } else {
          return options.resolveProvider.value_or(false);
        }
      },
      *result.capabilities.inlayHintProvider));
  ASSERT_TRUE(result.capabilities.semanticTokensProvider.has_value());
  const auto &semanticTokensProvider =
      *result.capabilities.semanticTokensProvider;
  EXPECT_EQ(
      std::visit(
          [](const auto &options) { return options.legend.tokenTypes.size(); },
          semanticTokensProvider),
      1u);
}

TEST(DefaultLanguageServerTest, InitializeAndInitializedEmitCallbacks) {
  auto shared = test::make_shared_services();
  DefaultLanguageServer server(*shared);

  bool sawInitialize = false;
  bool sawInitialized = false;
  auto onInitialize = server.onInitialize(
      [&sawInitialize](const ::lsp::InitializeParams &) { sawInitialize = true; });
  auto onInitialized = server.onInitialized(
      [&sawInitialized](const ::lsp::InitializedParams &) {
        sawInitialized = true;
      });

  (void)onInitialize;
  (void)onInitialized;

  (void)server.initialize(::lsp::InitializeParams{});
  server.initialized(::lsp::InitializedParams{});

  EXPECT_TRUE(sawInitialize);
  EXPECT_TRUE(sawInitialized);
}

TEST(DefaultLanguageServerTest,
     InitializeAdvertisesOnTypeFormattingWhenFormatterProvidesOptions) {
  auto shared = test::make_shared_services();
  auto services = test::make_services(*shared, "test", {".test"});
  services->lsp.formatter = std::make_unique<TestOnTypeFormatter>();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.documentOnTypeFormattingProvider.has_value());
  EXPECT_EQ(
      result.capabilities.documentOnTypeFormattingProvider->firstTriggerCharacter,
      ";");
  ASSERT_TRUE(
      result.capabilities.documentOnTypeFormattingProvider->moreTriggerCharacter
          .has_value());
  ASSERT_EQ(
      result.capabilities.documentOnTypeFormattingProvider->moreTriggerCharacter
          ->size(),
      1u);
  EXPECT_EQ(
      (*result.capabilities.documentOnTypeFormattingProvider
            ->moreTriggerCharacter)[0],
      ",");
}

TEST(DefaultLanguageServerTest,
     InitializeRejectsConflictingSemanticTokenLegendIndexes) {
  auto shared = test::make_shared_services();
  auto first = test::make_services(*shared, "first", {".first"});
  auto second = test::make_services(*shared, "second", {".second"});
  first->lsp.semanticTokenProvider =
      std::make_unique<TestSemanticTokenProvider>("type");
  second->lsp.semanticTokenProvider =
      std::make_unique<TestSemanticTokenProvider>("class");
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(first)));
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(second)));

  DefaultLanguageServer server(*shared);
  EXPECT_THROW((void)server.initialize(::lsp::InitializeParams{}),
               std::runtime_error);
}

TEST(DefaultLanguageServerTest,
     DoesNotAdvertiseUnsupportedFileOperationsEvenWhenDeclaredInOptions) {
  auto shared = test::make_shared_services();
  shared->lsp.fileOperationHandler =
      std::make_unique<DeclaredButUnsupportedFileOperationHandler>();

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.workspace.has_value());
  EXPECT_FALSE(result.capabilities.workspace->fileOperations.has_value());
}

TEST(DefaultLanguageServerTest,
     TextDocumentSyncSaveDependsOnDocumentUpdateHandlerSupport) {
  auto shared = test::make_shared_services();
  shared->lsp.documentUpdateHandler =
      std::make_unique<TestSavingDocumentUpdateHandler>();

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.textDocumentSync.has_value());
  const auto &textSync =
      std::get<::lsp::TextDocumentSyncOptions>(*result.capabilities.textDocumentSync);
  ASSERT_TRUE(textSync.save.has_value());
  EXPECT_TRUE(std::visit(
      [](const auto &save) {
        using Save = std::decay_t<decltype(save)>;
        if constexpr (std::is_same_v<Save, bool>) {
          return save;
        } else {
          return true;
        }
      },
      *textSync.save));
}

} // namespace
} // namespace pegium::lsp
