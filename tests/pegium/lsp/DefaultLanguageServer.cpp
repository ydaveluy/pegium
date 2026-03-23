#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/hierarchy/CallHierarchyProvider.hpp>
#include <pegium/lsp/code-actions/CodeLensProvider.hpp>
#include <pegium/lsp/navigation/DeclarationProvider.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/runtime/internal/LanguageClientFactory.hpp>
#include <pegium/lsp/navigation/DocumentLinkProvider.hpp>
#include <pegium/lsp/services/ExecuteCommandHandler.hpp>
#include <pegium/lsp/workspace/FileOperationHandler.hpp>
#include <pegium/lsp/navigation/ImplementationProvider.hpp>
#include <pegium/lsp/semantic/InlayHintProvider.hpp>
#include <pegium/lsp/ranges/SelectionRangeProvider.hpp>
#include <pegium/lsp/semantic/SemanticTokenProvider.hpp>
#include <pegium/lsp/semantic/SignatureHelpProvider.hpp>
#include <pegium/lsp/navigation/TypeDefinitionProvider.hpp>
#include <pegium/lsp/hierarchy/TypeHierarchyProvider.hpp>
#include <pegium/lsp/symbols/WorkspaceSymbolProvider.hpp>

namespace pegium {
namespace {

class TestSelectionRangeProvider final : public ::pegium::SelectionRangeProvider {
public:
  std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &,
                     const ::lsp::SelectionRangeParams &,
                     const utils::CancellationToken &) const override {
    return {};
  }
};

class TestCompletionProvider final : public ::pegium::CompletionProvider {
public:
  [[nodiscard]] std::optional<CompletionProviderOptions>
  completionOptions() const noexcept override {
    return CompletionProviderOptions{
        .triggerCharacters = {"."},
        .allCommitCharacters = {";"},
    };
  }

  std::optional<::lsp::CompletionList>
  getCompletion(const workspace::Document &, const ::lsp::CompletionParams &,
                const utils::CancellationToken &) const override {
    return ::lsp::CompletionList{};
  }
};

class TestSavingDocumentUpdateHandler final : public DocumentUpdateHandler {
public:
  [[nodiscard]] bool supportsDidSaveDocument() const noexcept override {
    return true;
  }
};

class TestFullSaveLifecycleDocumentUpdateHandler final
    : public DocumentUpdateHandler {
public:
  [[nodiscard]] bool supportsDidSaveDocument() const noexcept override {
    return true;
  }

  [[nodiscard]] bool supportsWillSaveDocument() const noexcept override {
    return true;
  }

  [[nodiscard]] bool
  supportsWillSaveDocumentWaitUntil() const noexcept override {
    return true;
  }
};

class RecordingConfigurationProvider final : public workspace::ConfigurationProvider {
public:
  void initialize(const workspace::InitializeParams &) override {
    order.push_back("configuration.initialize");
  }

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    initializedStarted = true;
    order.push_back("configuration.initialized");
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  [[nodiscard]] bool isReady() const noexcept override { return true; }

  void updateConfiguration(const workspace::ConfigurationChangeParams &) override {}

  std::optional<services::JsonValue>
  getConfiguration(std::string_view, std::string_view) const override {
    return std::nullopt;
  }

  utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener) override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(std::string_view) const override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view) const override {
    return {};
  }

  std::vector<std::string> order;
  std::atomic<bool> initializedStarted = false;
};

class RecordingWorkspaceManager final : public workspace::WorkspaceManager {
public:
  explicit RecordingWorkspaceManager(std::vector<std::string> &order)
      : order(order) {
    std::promise<void> promise;
    promise.set_value();
    readyFuture = promise.get_future().share();
  }

  workspace::BuildOptions &initialBuildOptions() override { return options; }

  const workspace::BuildOptions &initialBuildOptions() const override {
    return options;
  }

  std::shared_future<void> ready() const override { return readyFuture; }

  std::optional<std::vector<workspace::WorkspaceFolder>>
  workspaceFolders() const override {
    return std::vector<workspace::WorkspaceFolder>{};
  }

  void initialize(const workspace::InitializeParams &) override {
    order.push_back("workspace.initialize");
  }

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    initializedStarted = true;
    order.push_back("workspace.initialized");
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  void initializeWorkspace(std::span<const workspace::WorkspaceFolder>,
                           utils::CancellationToken cancelToken = {}) override {
    utils::throw_if_cancelled(cancelToken);
  }

  std::vector<std::string> searchFolder(std::string_view) const override {
    return {};
  }

  bool shouldIncludeEntry(const workspace::FileSystemNode &) const override {
    return true;
  }

  std::vector<std::string> &order;
  mutable workspace::BuildOptions options{};
  std::shared_future<void> readyFuture;
  std::atomic<bool> initializedStarted = false;
};

class BlockingConfigurationProvider final : public workspace::ConfigurationProvider {
public:
  explicit BlockingConfigurationProvider(std::shared_future<void> gate)
      : gate(std::move(gate)) {}

  void initialize(const workspace::InitializeParams &) override {}

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    initializedStarted = true;
    auto waitGate = gate;
    return std::async(std::launch::async,
                      [waitGate]() mutable { waitGate.wait(); });
  }

  [[nodiscard]] bool isReady() const noexcept override { return false; }

  void updateConfiguration(const workspace::ConfigurationChangeParams &) override {}

  std::optional<services::JsonValue>
  getConfiguration(std::string_view, std::string_view) const override {
    return std::nullopt;
  }

  utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener) override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(std::string_view) const override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view) const override {
    return {};
  }

  std::shared_future<void> gate;
  std::atomic<bool> initializedStarted = false;
};

class BlockingWorkspaceManager final : public workspace::WorkspaceManager {
public:
  explicit BlockingWorkspaceManager(std::shared_future<void> gate)
      : gate(std::move(gate)) {
    std::promise<void> promise;
    promise.set_value();
    readyFuture = promise.get_future().share();
  }

  workspace::BuildOptions &initialBuildOptions() override { return options; }

  const workspace::BuildOptions &initialBuildOptions() const override {
    return options;
  }

  std::shared_future<void> ready() const override { return readyFuture; }

  std::optional<std::vector<workspace::WorkspaceFolder>>
  workspaceFolders() const override {
    return std::vector<workspace::WorkspaceFolder>{};
  }

  void initialize(const workspace::InitializeParams &) override {}

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    initializedStarted = true;
    auto waitGate = gate;
    return std::async(std::launch::async,
                      [waitGate]() mutable { waitGate.wait(); });
  }

  void initializeWorkspace(std::span<const workspace::WorkspaceFolder>,
                           utils::CancellationToken cancelToken = {}) override {
    utils::throw_if_cancelled(cancelToken);
  }

  std::vector<std::string> searchFolder(std::string_view) const override {
    return {};
  }

  bool shouldIncludeEntry(const workspace::FileSystemNode &) const override {
    return true;
  }

  std::shared_future<void> gate;
  std::shared_future<void> readyFuture;
  mutable workspace::BuildOptions options{};
  std::atomic<bool> initializedStarted = false;
};

class FailingConfigurationProvider final : public workspace::ConfigurationProvider {
public:
  void initialize(const workspace::InitializeParams &) override {}

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    return std::async(std::launch::async, []() -> void {
      throw std::runtime_error("configuration init failed");
    });
  }

  [[nodiscard]] bool isReady() const noexcept override { return false; }

  void updateConfiguration(const workspace::ConfigurationChangeParams &) override {}

  std::optional<services::JsonValue>
  getConfiguration(std::string_view, std::string_view) const override {
    return std::nullopt;
  }

  utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener) override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(std::string_view) const override {
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view) const override {
    return {};
  }
};

class FailingWorkspaceManager final : public workspace::WorkspaceManager {
public:
  FailingWorkspaceManager() {
    std::promise<void> promise;
    promise.set_value();
    readyFuture = promise.get_future().share();
  }

  workspace::BuildOptions &initialBuildOptions() override { return options; }

  const workspace::BuildOptions &initialBuildOptions() const override {
    return options;
  }

  std::shared_future<void> ready() const override { return readyFuture; }

  std::optional<std::vector<workspace::WorkspaceFolder>>
  workspaceFolders() const override {
    return std::vector<workspace::WorkspaceFolder>{};
  }

  void initialize(const workspace::InitializeParams &) override {}

  std::future<void>
  initialized(const workspace::InitializedParams &) override {
    return std::async(std::launch::async, []() -> void {
      throw std::runtime_error("workspace init failed");
    });
  }

  void initializeWorkspace(std::span<const workspace::WorkspaceFolder>,
                           utils::CancellationToken cancelToken = {}) override {
    utils::throw_if_cancelled(cancelToken);
  }

  std::vector<std::string> searchFolder(std::string_view) const override {
    return {};
  }

  bool shouldIncludeEntry(const workspace::FileSystemNode &) const override {
    return true;
  }

  workspace::BuildOptions options{};
  std::shared_future<void> readyFuture;
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

::lsp::FileOperationOptions make_test_file_operation_options() {
  ::lsp::FileOperationFilter fileFilter{};
  fileFilter.pattern.glob = "**/*";
  fileFilter.scheme = "file";

  ::lsp::FileOperationRegistrationOptions registration{};
  registration.filters.push_back(std::move(fileFilter));

  ::lsp::FileOperationOptions options{};
  options.didCreate = registration;
  return options;
}

class TestFileOperationHandler final : public FileOperationHandler {
public:
  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override {
    return options;
  }

private:
  ::lsp::FileOperationOptions options = make_test_file_operation_options();
};

class EmptyFileOperationHandler final : public FileOperationHandler {
public:
  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override {
    return options;
  }

private:
  ::lsp::FileOperationOptions options{};
};

class TestWorkspaceSymbolProvider final : public ::pegium::WorkspaceSymbolProvider {
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

class TestFormatter : public ::pegium::Formatter {
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

class TestSignatureHelpProvider final : public ::pegium::SignatureHelpProvider {
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

class TestDeclarationProvider final : public ::pegium::DeclarationProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getDeclaration(const workspace::Document &, const ::lsp::DeclarationParams &,
                 const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestTypeDefinitionProvider final : public ::pegium::TypeDefinitionProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &,
                    const ::lsp::TypeDefinitionParams &,
                    const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestImplementationProvider final : public ::pegium::ImplementationProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &,
                    const ::lsp::ImplementationParams &,
                    const utils::CancellationToken &) const override {
    return std::vector<::lsp::LocationLink>{};
  }
};

class TestCodeLensProvider final : public ::pegium::CodeLensProvider {
public:
  std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &, const ::lsp::CodeLensParams &,
                  const utils::CancellationToken &) const override {
    return {};
  }
};

class TestDocumentLinkProvider final : public ::pegium::DocumentLinkProvider {
public:
  std::vector<::lsp::DocumentLink>
  getDocumentLinks(const workspace::Document &,
                   const ::lsp::DocumentLinkParams &,
                   const utils::CancellationToken &) const override {
    return {};
  }
};

class TestCallHierarchyProvider final : public ::pegium::CallHierarchyProvider {
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

class TestTypeHierarchyProvider final : public ::pegium::TypeHierarchyProvider {
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

class TestInlayHintProvider final : public ::pegium::InlayHintProvider {
public:
  std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &, const ::lsp::InlayHintParams &,
                const utils::CancellationToken &) const override {
    return {};
  }
};

class TestSemanticTokenProvider final : public ::pegium::SemanticTokenProvider {
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
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  EXPECT_FALSE(result.capabilities.completionProvider->completionItem.has_value());
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
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  services->lsp.selectionRangeProvider =
      std::make_unique<TestSelectionRangeProvider>();
  services->lsp.completionProvider = std::make_unique<TestCompletionProvider>();
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

  shared->serviceRegistry->registerServices(std::move(services));

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.selectionRangeProvider.has_value());
  EXPECT_TRUE(std::get<bool>(*result.capabilities.selectionRangeProvider));
  ASSERT_TRUE(result.capabilities.completionProvider.has_value());
  ASSERT_TRUE(result.capabilities.completionProvider->triggerCharacters.has_value());
  EXPECT_EQ((*result.capabilities.completionProvider->triggerCharacters)[0], ".");
  ASSERT_TRUE(
      result.capabilities.completionProvider->allCommitCharacters.has_value());
  EXPECT_EQ(
      (*result.capabilities.completionProvider->allCommitCharacters)[0], ";");
  EXPECT_FALSE(result.capabilities.completionProvider->completionItem.has_value());

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
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
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
     InitializeCallsDefaultServicesBeforeOnInitialize) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);

  auto configurationProvider = std::make_shared<RecordingConfigurationProvider>();
  auto workspaceManager =
      std::make_unique<RecordingWorkspaceManager>(configurationProvider->order);
  shared->workspace.configurationProvider = configurationProvider;
  shared->workspace.workspaceManager = std::move(workspaceManager);

  DefaultLanguageServer server(*shared);
  auto onInitialize = server.onInitialize(
      [order = &configurationProvider->order](const ::lsp::InitializeParams &) {
        order->push_back("event.initialize");
      });
  (void)onInitialize;

  (void)server.initialize(::lsp::InitializeParams{});

  EXPECT_EQ(configurationProvider->order,
            (std::vector<std::string>{"configuration.initialize",
                                      "workspace.initialize",
                                      "event.initialize"}));
}

TEST(DefaultLanguageServerTest,
     InitializedStartsDefaultServicesBeforeEventAndDoesNotBlock) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);

  std::promise<void> gatePromise;
  auto gate = gatePromise.get_future().share();
  auto configurationProvider =
      std::make_shared<BlockingConfigurationProvider>(gate);
  auto workspaceManager = std::make_unique<BlockingWorkspaceManager>(gate);
  auto *workspaceManagerPtr = workspaceManager.get();
  shared->workspace.configurationProvider = configurationProvider;
  shared->workspace.workspaceManager = std::move(workspaceManager);

  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);
  shared->lsp.languageClient =
      make_message_handler_language_client(handler, *shared->observabilitySink);

  DefaultLanguageServer server(*shared);
  bool sawInitialized = false;
  auto onInitialized = server.onInitialized(
      [&](const ::lsp::InitializedParams &) {
        sawInitialized = true;
        EXPECT_TRUE(configurationProvider->initializedStarted.load());
        EXPECT_TRUE(workspaceManagerPtr->initializedStarted.load());
      });
  (void)onInitialized;

  auto future = std::async(std::launch::async, [&]() {
    server.initialized(::lsp::InitializedParams{});
  });

  EXPECT_EQ(future.wait_for(std::chrono::milliseconds(200)),
            std::future_status::ready);
  future.get();

  EXPECT_TRUE(sawInitialized);
  EXPECT_TRUE(configurationProvider->initializedStarted.load());
  EXPECT_TRUE(workspaceManagerPtr->initializedStarted.load());

  gatePromise.set_value();
  shared->lsp.languageClient.reset();
}

TEST(DefaultLanguageServerTest, LifecycleEventsAreOneShot) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);

  DefaultLanguageServer server(*shared);
  std::size_t initializeCount = 0;
  std::size_t initializedCount = 0;

  auto onInitialize = server.onInitialize(
      [&](const ::lsp::InitializeParams &) { ++initializeCount; });
  auto onInitialized = server.onInitialized(
      [&](const ::lsp::InitializedParams &) { ++initializedCount; });
  (void)onInitialize;
  (void)onInitialized;

  (void)server.initialize(::lsp::InitializeParams{});
  (void)server.initialize(::lsp::InitializeParams{});
  server.initialized(::lsp::InitializedParams{});
  server.initialized(::lsp::InitializedParams{});

  EXPECT_EQ(initializeCount, 1u);
  EXPECT_EQ(initializedCount, 1u);
}

TEST(DefaultLanguageServerTest,
     InitializedPublishesObservationsForBackgroundFailuresAndDoesNotBlock) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);

  shared->workspace.configurationProvider =
      std::make_shared<FailingConfigurationProvider>();
  shared->workspace.workspaceManager =
      std::make_unique<FailingWorkspaceManager>();

  DefaultLanguageServer server(*shared);
  auto future = std::async(std::launch::async, [&]() {
    server.initialized(::lsp::InitializedParams{});
  });

  EXPECT_EQ(future.wait_for(std::chrono::milliseconds(200)),
            std::future_status::ready);
  future.get();

  ASSERT_TRUE(recordingSink->waitForCount(2));
  const auto observations = recordingSink->observations();

  bool sawConfigurationFailure = false;
  bool sawWorkspaceFailure = false;
  for (const auto &observation : observations) {
    if (observation.code !=
        observability::ObservationCode::LspRuntimeBackgroundTaskFailed) {
      continue;
    }
    if (observation.category == "ConfigurationProvider.initialized") {
      sawConfigurationFailure =
          observation.message.find("configuration init failed") !=
          std::string::npos;
    }
    if (observation.category == "WorkspaceManager.initialized") {
      sawWorkspaceFailure =
          observation.message.find("workspace init failed") !=
          std::string::npos;
    }
  }

  EXPECT_TRUE(sawConfigurationFailure);
  EXPECT_TRUE(sawWorkspaceFailure);
}

TEST(DefaultLanguageServerTest,
     InitializeAdvertisesOnTypeFormattingWhenFormatterProvidesOptions) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  services->lsp.formatter = std::make_unique<TestOnTypeFormatter>();
  shared->serviceRegistry->registerServices(std::move(services));

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
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto first = test::make_uninstalled_services(*shared, "first", {".first"});
  pegium::services::installDefaultCoreServices(*first);
  pegium::installDefaultLspServices(*first);
  auto second = test::make_uninstalled_services(*shared, "second", {".second"});
  pegium::services::installDefaultCoreServices(*second);
  pegium::installDefaultLspServices(*second);
  first->lsp.semanticTokenProvider =
      std::make_unique<TestSemanticTokenProvider>("type");
  second->lsp.semanticTokenProvider =
      std::make_unique<TestSemanticTokenProvider>("class");
  shared->serviceRegistry->registerServices(std::move(first));
  shared->serviceRegistry->registerServices(std::move(second));

  DefaultLanguageServer server(*shared);
  EXPECT_THROW((void)server.initialize(::lsp::InitializeParams{}),
               std::runtime_error);
}

TEST(DefaultLanguageServerTest,
     DoesNotAdvertiseConcreteFileOperationsWhenNoOperationsAreDeclared) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  shared->lsp.fileOperationHandler =
      std::make_unique<EmptyFileOperationHandler>();

  DefaultLanguageServer server(*shared);
  const auto result = server.initialize(::lsp::InitializeParams{});

  ASSERT_TRUE(result.capabilities.workspace.has_value());
  EXPECT_FALSE(result.capabilities.workspace->fileOperations.has_value());
}

TEST(DefaultLanguageServerTest,
     TextDocumentSyncSaveDependsOnDocumentUpdateHandlerSupport) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
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

TEST(DefaultLanguageServerTest,
     TextDocumentSyncReflectsExplicitDocumentSaveLifecycleSupport) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  shared->lsp.documentUpdateHandler =
      std::make_unique<TestFullSaveLifecycleDocumentUpdateHandler>();

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
  EXPECT_TRUE(textSync.willSave.value_or(false));
  EXPECT_TRUE(textSync.willSaveWaitUntil.value_or(false));
}

} // namespace
} // namespace pegium
