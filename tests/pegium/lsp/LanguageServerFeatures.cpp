#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/code-actions/CodeLensProvider.hpp>
#include <pegium/lsp/code-actions/CodeActionProvider.hpp>
#include <pegium/lsp/navigation/DeclarationProvider.hpp>
#include <pegium/lsp/code-actions/DefaultCodeActionProvider.hpp>
#include <pegium/lsp/support/JsonValue.hpp>
#include <pegium/lsp/navigation/DocumentLinkProvider.hpp>
#include <pegium/lsp/formatting/Formatter.hpp>
#include <pegium/lsp/runtime/internal/LanguageServerFeatureDispatch.hpp>
#include <pegium/lsp/symbols/WorkspaceSymbolProvider.hpp>

namespace pegium {
namespace {

constexpr std::string_view kDefaultCodeActionsKey = "pegiumDefaultCodeActions";

::lsp::Diagnostic make_default_code_action_diagnostic(
    std::string title, std::uint32_t begin, std::uint32_t end,
    std::string newText) {
  services::JsonValue::Object action;
  action.try_emplace("kind", "quickfix");
  action.try_emplace("editKind", end > begin ? "replace" : "insert");
  action.try_emplace("title", std::move(title));
  action.try_emplace("begin", static_cast<std::int64_t>(begin));
  action.try_emplace("end", static_cast<std::int64_t>(end));
  action.try_emplace("newText", std::move(newText));

  services::JsonValue::Array actions;
  actions.emplace_back(std::move(action));

  services::JsonValue::Object data;
  data.try_emplace(std::string(kDefaultCodeActionsKey), std::move(actions));

  ::lsp::Diagnostic diagnostic{};
  diagnostic.message = "recovery";
  diagnostic.data = to_lsp_any(services::JsonValue(std::move(data)));
  return diagnostic;
}

class RecordingDeclarationProvider final : public ::pegium::DeclarationProvider {
public:
  mutable std::string lastUri;

  std::optional<std::vector<::lsp::LocationLink>>
  getDeclaration(const workspace::Document &document,
                 const ::lsp::DeclarationParams &,
                 const utils::CancellationToken &) const override {
    lastUri = document.uri;

    ::lsp::LocationLink link{};
    link.targetUri = ::lsp::DocumentUri(::lsp::Uri::parse(document.uri));
    return std::vector<::lsp::LocationLink>{std::move(link)};
  }
};

class RecordingCodeActionProvider final : public ::pegium::CodeActionProvider {
public:
  mutable std::string lastUri;

  std::optional<
      std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &,
                 const utils::CancellationToken &) const override {
    lastUri = document.uri;

    ::lsp::CodeAction action{};
    action.title = "Fix value";
    action.kind = ::lsp::CodeActionKind::QuickFix;
    return std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>{
        std::move(action)};
  }
};

class HookAppendingCodeActionProvider final
    : public DefaultCodeActionProvider {
public:
  mutable std::string lastUri;

protected:
  void appendCodeActions(const workspace::Document &document,
                         const ::lsp::CodeActionParams &,
                         CodeActionResult &actions,
                         const utils::CancellationToken &) const override {
    lastUri = document.uri;

    ::lsp::CodeAction action{};
    action.title = "Custom hook fix";
    action.kind = ::lsp::CodeActionKind::QuickFix;
    actions.push_back(std::move(action));
  }
};

class OverrideCallingBaseCodeActionProvider final
    : public DefaultCodeActionProvider {
public:
  mutable std::string lastUri;

  std::optional<std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &params,
                 const utils::CancellationToken &cancelToken) const override {
    lastUri = document.uri;

    auto actions =
        DefaultCodeActionProvider::getCodeActions(document, params, cancelToken)
            .value_or(CodeActionResult{});

    ::lsp::CodeAction action{};
    action.title = "Custom override fix";
    action.kind = ::lsp::CodeActionKind::QuickFix;
    actions.push_back(std::move(action));
    if (actions.empty()) {
      return std::nullopt;
    }
    return std::optional<
        std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>(
        std::move(actions));
  }
};

class RecordingCodeLensProvider final : public ::pegium::CodeLensProvider {
public:
  bool resolveSupported = false;
  mutable std::string lastUri;
  mutable std::optional<services::JsonValue> lastResolveData;

  std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &document,
                  const ::lsp::CodeLensParams &,
                  const utils::CancellationToken &) const override {
    lastUri = document.uri;

    ::lsp::CodeLens codeLens{};
    codeLens.range.start = document.textDocument().positionAt(0);
    codeLens.range.end = document.textDocument().positionAt(0);
    codeLens.data = to_lsp_any(services::JsonValue(services::JsonValue::Object{
        {"seed", "alpha"},
    }));
    return {std::move(codeLens)};
  }

  [[nodiscard]] bool supportsResolveCodeLens() const noexcept override {
    return resolveSupported;
  }

  std::optional<::lsp::CodeLens>
  resolveCodeLens(const ::lsp::CodeLens &codeLens,
                  const utils::CancellationToken &) const override {
    if (!resolveSupported) {
      return std::nullopt;
    }

    if (codeLens.data.has_value()) {
      lastResolveData = from_lsp_any(*codeLens.data);
    } else {
      lastResolveData = std::nullopt;
    }

    ::lsp::CodeLens resolved = codeLens;
    ::lsp::Command command{};
    command.title = "Resolved lens";
    command.command = "test.resolveCodeLens";
    resolved.command = std::move(command);
    resolved.data = std::nullopt;
    return resolved;
  }
};

class RecordingDocumentLinkProvider final
    : public ::pegium::DocumentLinkProvider {
public:
  mutable std::string lastUri;

  std::vector<::lsp::DocumentLink>
  getDocumentLinks(const workspace::Document &document,
                   const ::lsp::DocumentLinkParams &,
                   const utils::CancellationToken &) const override {
    lastUri = document.uri;

    ::lsp::DocumentLink link{};
    link.target = ::lsp::DocumentUri(::lsp::Uri::parse(document.uri));
    return {std::move(link)};
  }
};

class RecordingWorkspaceSymbolProvider final
    : public ::pegium::WorkspaceSymbolProvider {
public:
  [[nodiscard]] bool supportsResolveSymbol() const noexcept override {
    return true;
  }

  std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &,
             const utils::CancellationToken &) const override {
    return {};
  }

  std::optional<::lsp::WorkspaceSymbol>
  resolveSymbol(const ::lsp::WorkspaceSymbol &symbol,
                const utils::CancellationToken &) const override {
    lastName = symbol.name;

    ::lsp::WorkspaceSymbol resolved = symbol;
    ::lsp::Location location{};
    location.uri = ::lsp::DocumentUri(::lsp::Uri::parse(test::make_file_uri(
        "workspace-symbol-resolve.test")));
    location.range.start = text::Position(1, 2);
    location.range.end = text::Position(1, 7);
    resolved.location = std::move(location);
    return resolved;
  }

  mutable std::string lastName;
};

class RecordingFormatter final : public ::pegium::Formatter {
public:
  mutable std::string lastDocumentUri;
  mutable std::optional<::lsp::Position> lastOnTypePosition;

  std::vector<::lsp::TextEdit>
  formatDocument(const workspace::Document &document,
                 const ::lsp::DocumentFormattingParams &,
                 const utils::CancellationToken &) const override {
    lastDocumentUri = document.uri;

    ::lsp::TextEdit edit{};
    edit.range.start = document.textDocument().positionAt(0);
    edit.range.end = document.textDocument().positionAt(0);
    edit.newText = "full";
    return {std::move(edit)};
  }

  std::vector<::lsp::TextEdit>
  formatDocumentRange(const workspace::Document &document,
                      const ::lsp::DocumentRangeFormattingParams &,
                      const utils::CancellationToken &) const override {
    lastDocumentUri = document.uri;

    ::lsp::TextEdit edit{};
    edit.range.start = document.textDocument().positionAt(0);
    edit.range.end = document.textDocument().positionAt(0);
    edit.newText = "range";
    return {std::move(edit)};
  }

  std::vector<::lsp::TextEdit>
  formatDocumentOnType(const workspace::Document &document,
                       const ::lsp::DocumentOnTypeFormattingParams &params,
                       const utils::CancellationToken &) const override {
    lastDocumentUri = document.uri;
    lastOnTypePosition = params.position;

    ::lsp::TextEdit edit{};
    edit.range.start = document.textDocument().positionAt(0);
    edit.range.end = document.textDocument().positionAt(0);
    edit.newText = "on-type";
    return {std::move(edit)};
  }
};

TEST(LanguageServerFeaturesTest, DispatchesDeclarationRequestsToLanguageService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingDeclarationProvider>();
  auto *recording = provider.get();
  services->lsp.declarationProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("declaration-dispatch.test"), "test",
      "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::DeclarationParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto links = getDeclaration(*shared, params, utils::default_cancel_token);
  ASSERT_TRUE(links.has_value());
  ASSERT_EQ(links->size(), 1u);
  EXPECT_EQ(recording->lastUri, document->uri);
  EXPECT_EQ((*links)[0].targetUri.toString(), document->uri);
}

TEST(LanguageServerFeaturesTest, DispatchesCodeLensRequestsToLanguageService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingCodeLensProvider>();
  auto *recording = provider.get();
  services->lsp.codeLensProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("codelens-dispatch.test"), "test", "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeLensParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto codeLens = getCodeLens(*shared, params, utils::default_cancel_token);
  ASSERT_EQ(codeLens.size(), 1u);
  EXPECT_EQ(recording->lastUri, document->uri);
}

TEST(LanguageServerFeaturesTest,
     ResolvesCodeLensRequestsAndPreservesOriginalProviderData) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingCodeLensProvider>();
  provider->resolveSupported = true;
  auto *recording = provider.get();
  services->lsp.codeLensProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("codelens-resolve.test"), "test", "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeLensParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto codeLens = getCodeLens(*shared, params, utils::default_cancel_token);
  ASSERT_EQ(codeLens.size(), 1u);
  ASSERT_TRUE(codeLens.front().data.has_value());

  const auto resolved =
      resolveCodeLens(*shared, codeLens.front(), utils::default_cancel_token);
  ASSERT_TRUE(resolved.has_value());
  ASSERT_TRUE(resolved->command.has_value());
  EXPECT_EQ(resolved->command->title, "Resolved lens");

  ASSERT_TRUE(recording->lastResolveData.has_value());
  ASSERT_TRUE(recording->lastResolveData->isObject());
  EXPECT_EQ(recording->lastResolveData->object().at("seed").string(), "alpha");

  const auto resolvedAgain =
      resolveCodeLens(*shared, *resolved, utils::default_cancel_token);
  ASSERT_TRUE(resolvedAgain.has_value());
  ASSERT_TRUE(resolvedAgain->command.has_value());
  ASSERT_TRUE(recording->lastResolveData.has_value());
  EXPECT_EQ(recording->lastResolveData->object().at("seed").string(), "alpha");
}

TEST(LanguageServerFeaturesTest,
     DispatchesCodeActionRequestsToLanguageService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingCodeActionProvider>();
  auto *recording = provider.get();
  services->lsp.codeActionProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("code-action-dispatch.test"), "test",
      "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto actions =
      getCodeActions(*shared, params, utils::default_cancel_token);
  ASSERT_TRUE(actions.has_value());
  ASSERT_EQ(actions->size(), 1u);
  EXPECT_EQ(recording->lastUri, document->uri);
  ASSERT_TRUE(std::holds_alternative<::lsp::CodeAction>((*actions)[0]));
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[0]).title, "Fix value");
}

TEST(LanguageServerFeaturesTest,
     DefaultCodeActionProviderBuildsRecoveryQuickFixes) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("code-action-default.test"), "test", "oops");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.context.diagnostics.push_back(
      make_default_code_action_diagnostic("Delete unexpected text", 0, 4, ""));

  const auto actions =
      getCodeActions(*shared, params, utils::default_cancel_token);
  ASSERT_TRUE(actions.has_value());
  ASSERT_EQ(actions->size(), 1u);
  ASSERT_TRUE(std::holds_alternative<::lsp::CodeAction>((*actions)[0]));
  const auto &action = std::get<::lsp::CodeAction>((*actions)[0]);
  EXPECT_EQ(action.title, "Delete unexpected text");
  ASSERT_TRUE(action.edit.has_value());
}

TEST(LanguageServerFeaturesTest,
     DefaultCodeActionProviderCanBeExtendedViaHook) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<HookAppendingCodeActionProvider>();
  auto *recording = provider.get();
  services->lsp.codeActionProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("code-action-hook.test"), "test", "oops");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.context.diagnostics.push_back(
      make_default_code_action_diagnostic("Delete unexpected text", 0, 4, ""));

  const auto actions =
      getCodeActions(*shared, params, utils::default_cancel_token);
  ASSERT_TRUE(actions.has_value());
  ASSERT_EQ(actions->size(), 2u);
  EXPECT_EQ(recording->lastUri, document->uri);
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[0]).title,
            "Delete unexpected text");
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[1]).title, "Custom hook fix");
}

TEST(LanguageServerFeaturesTest,
     DefaultCodeActionProviderCanBeOverriddenWhileCallingBase) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<OverrideCallingBaseCodeActionProvider>();
  auto *recording = provider.get();
  services->lsp.codeActionProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("code-action-override.test"), "test", "oops");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.context.diagnostics.push_back(
      make_default_code_action_diagnostic("Delete unexpected text", 0, 4, ""));

  const auto actions =
      getCodeActions(*shared, params, utils::default_cancel_token);
  ASSERT_TRUE(actions.has_value());
  ASSERT_EQ(actions->size(), 2u);
  EXPECT_EQ(recording->lastUri, document->uri);
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[0]).title,
            "Delete unexpected text");
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[1]).title,
            "Custom override fix");
}

TEST(LanguageServerFeaturesTest,
     DefaultCodeActionProviderRespectsRequestedKinds) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("code-action-kinds.test"), "test", "oops");
  ASSERT_NE(document, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.context.diagnostics.push_back(
      make_default_code_action_diagnostic("Delete unexpected text", 0, 4, ""));
  params.context.only = ::lsp::Array<::lsp::CodeActionKindEnum>{
      ::lsp::CodeActionKind::Refactor};

  const auto actions =
      getCodeActions(*shared, params, utils::default_cancel_token);
  EXPECT_FALSE(actions.has_value());
}

TEST(LanguageServerFeaturesTest,
     DispatchesDocumentLinkRequestsToLanguageService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingDocumentLinkProvider>();
  auto *recording = provider.get();
  services->lsp.documentLinkProvider = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("document-link-dispatch.test"), "test",
      "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::DocumentLinkParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto links =
      getDocumentLinks(*shared, params, utils::default_cancel_token);
  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(recording->lastUri, document->uri);
  EXPECT_EQ(links[0].target->toString(), document->uri);
}

TEST(LanguageServerFeaturesTest,
     DispatchesWorkspaceSymbolResolveRequestsToSharedService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto provider = std::make_unique<RecordingWorkspaceSymbolProvider>();
  auto *recording = provider.get();
  shared->lsp.workspaceSymbolProvider = std::move(provider);

  ::lsp::WorkspaceSymbol symbol{};
  symbol.name = "value";
  symbol.kind = ::lsp::SymbolKind::Variable;
  symbol.location = ::lsp::WorkspaceSymbolLocation_Uri{
      .uri =
          ::lsp::DocumentUri(::lsp::Uri::parse(test::make_file_uri("value.test")))};

  const auto resolved =
      resolveWorkspaceSymbol(*shared, symbol, utils::default_cancel_token);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(recording->lastName, "value");
  ASSERT_TRUE(std::holds_alternative<::lsp::Location>(resolved->location));
  const auto &location = std::get<::lsp::Location>(resolved->location);
  EXPECT_EQ(location.uri.toString(),
            test::make_file_uri("workspace-symbol-resolve.test"));
  EXPECT_EQ(location.range.start.line, 1u);
  EXPECT_EQ(location.range.start.character, 2u);
  EXPECT_EQ(location.range.end.character, 7u);
}

TEST(LanguageServerFeaturesTest, DispatchesFormattingRequestsToLanguageService) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  auto provider = std::make_unique<RecordingFormatter>();
  auto *recording = provider.get();
  services->lsp.formatter = std::move(provider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("formatting-dispatch.test"), "test",
      "alpha");
  ASSERT_NE(document, nullptr);

  ::lsp::DocumentFormattingParams formattingParams{};
  formattingParams.textDocument.uri =
      ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  const auto formatting =
      formatDocument(*shared, formattingParams, utils::default_cancel_token);
  ASSERT_EQ(formatting.size(), 1u);
  EXPECT_EQ(recording->lastDocumentUri, document->uri);
  EXPECT_EQ(formatting[0].newText, "full");

  ::lsp::DocumentRangeFormattingParams rangeParams{};
  rangeParams.textDocument.uri =
      ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  rangeParams.range.start = text::Position(0, 0);
  rangeParams.range.end = text::Position(0, 5);
  const auto rangeFormatting =
      formatDocumentRange(*shared, rangeParams, utils::default_cancel_token);
  ASSERT_EQ(rangeFormatting.size(), 1u);
  EXPECT_EQ(recording->lastDocumentUri, document->uri);
  EXPECT_EQ(rangeFormatting[0].newText, "range");

  ::lsp::DocumentOnTypeFormattingParams onTypeParams{};
  onTypeParams.textDocument.uri =
      ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  onTypeParams.position = text::Position(0, 3);
  onTypeParams.ch = ";";
  const auto onTypeFormatting =
      formatDocumentOnType(*shared, onTypeParams, utils::default_cancel_token);
  ASSERT_EQ(onTypeFormatting.size(), 1u);
  EXPECT_EQ(recording->lastDocumentUri, document->uri);
  ASSERT_TRUE(recording->lastOnTypePosition.has_value());
  EXPECT_EQ(recording->lastOnTypePosition->line, 0u);
  EXPECT_EQ(recording->lastOnTypePosition->character, 3u);
  EXPECT_EQ(onTypeFormatting[0].newText, "on-type");
}

} // namespace
} // namespace pegium
