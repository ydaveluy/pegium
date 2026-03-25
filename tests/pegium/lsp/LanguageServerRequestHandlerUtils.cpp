#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <variant>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/core/workspace/Configuration.hpp>

namespace pegium {
namespace {

void set_validation_configuration(
    workspace::ConfigurationProvider &configurationProvider,
    std::string_view languageId, bool enabled,
    std::vector<std::string> categories) {
  pegium::JsonValue::Array categoryValues;
  categoryValues.reserve(categories.size());
  for (auto &category : categories) {
    categoryValues.emplace_back(std::move(category));
  }

  workspace::ConfigurationChangeParams params;
  params.settings = pegium::JsonValue(pegium::JsonValue::Object{
      {std::string(languageId),
       pegium::JsonValue(pegium::JsonValue::Object{
           {"validation",
            pegium::JsonValue(pegium::JsonValue::Object{
                {"enabled", pegium::JsonValue(enabled)},
                {"categories", pegium::JsonValue(std::move(categoryValues))},
            })},
       })},
  });
  configurationProvider.updateConfiguration(params);
}

class BlockingBuildDocumentBuilder final : public workspace::DocumentBuilder {
public:
  [[nodiscard]] workspace::BuildOptions &
  updateBuildOptions() noexcept override {
    return _options;
  }

  [[nodiscard]] const workspace::BuildOptions &
  updateBuildOptions() const noexcept override {
    return _options;
  }

  void build(std::span<const std::shared_ptr<workspace::Document>>,
             const workspace::BuildOptions & = {},
             utils::CancellationToken cancelToken = {}) const override {
    {
      std::scoped_lock lock(_mutex);
      _started = true;
    }
    _cv.notify_all();

    try {
      while (!cancelToken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      _observedCancellation = true;
      utils::throw_if_cancelled(cancelToken);
    } catch (...) {
      {
        std::scoped_lock lock(_mutex);
        _finished = true;
      }
      _cv.notify_all();
      throw;
    }
  }

  void update(std::span<const workspace::DocumentId>,
              std::span<const workspace::DocumentId>,
              utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
  }

  utils::ScopedDisposable
  onUpdate(std::function<void(std::span<const workspace::DocumentId>,
                              std::span<const workspace::DocumentId>)>)
      const override {
    return {};
  }

  utils::ScopedDisposable onBuildPhase(
      workspace::DocumentState,
      std::function<void(std::span<const std::shared_ptr<workspace::Document>>,
                         utils::CancellationToken)>) const override {
    return {};
  }

  utils::ScopedDisposable onDocumentPhase(
      workspace::DocumentState,
      std::function<void(const std::shared_ptr<workspace::Document> &,
                         utils::CancellationToken)>) const override {
    return {};
  }

  void waitUntil(workspace::DocumentState,
                 utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
  }

  [[nodiscard]] workspace::DocumentId
  waitUntil(workspace::DocumentState, workspace::DocumentId documentId,
            utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    return documentId;
  }

  void resetToState(workspace::Document &, workspace::DocumentState) const override {}

  [[nodiscard]] bool
  waitUntilStarted(std::chrono::milliseconds timeout =
                       std::chrono::milliseconds(1000)) const {
    std::unique_lock lock(_mutex);
    return _cv.wait_for(lock, timeout, [this]() { return _started; });
  }

  [[nodiscard]] bool
  waitUntilFinished(std::chrono::milliseconds timeout =
                        std::chrono::milliseconds(1000)) const {
    std::unique_lock lock(_mutex);
    return _cv.wait_for(lock, timeout, [this]() { return _finished; });
  }

  [[nodiscard]] bool observedCancellation() const noexcept {
    return _observedCancellation.load();
  }

private:
  mutable workspace::BuildOptions _options;
  mutable std::mutex _mutex;
  mutable std::condition_variable _cv;
  mutable bool _started = false;
  mutable bool _finished = false;
  mutable std::atomic<bool> _observedCancellation = false;
};

class LanguageServerRequestHandlerUtilsTest : public ::testing::Test {
protected:
  std::unique_ptr<pegium::SharedServices> shared =
      test::make_empty_shared_services();
  DefaultLanguageServer server{*shared};
  LanguageServerRuntimeState runtimeState;
  LanguageServerHandlerContext context{server, *shared, runtimeState};
  test::RecordingDocumentBuilder *builder = nullptr;

  LanguageServerRequestHandlerUtilsTest() {
    pegium::installDefaultSharedCoreServices(*shared);
    pegium::installDefaultSharedLspServices(*shared);
    pegium::test::initialize_shared_workspace_for_tests(*shared);
  }

  void installRecordingBuilder() {
    builder = new test::RecordingDocumentBuilder();
    shared->workspace.documentBuilder.reset(builder);
  }

  void registerTestLanguage() {
    {
      auto registeredServices = 
        test::make_uninstalled_services(*shared, "test", {".test"});
      pegium::installDefaultCoreServices(*registeredServices);
      pegium::installDefaultLspServices(*registeredServices);
      shared->serviceRegistry->registerServices(std::move(registeredServices));
    }
  }

  std::string installFile(std::string_view fileName, std::string content) {
    auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
    const auto filePath =
        std::string("/tmp/pegium-tests/") + std::string(fileName);
    const auto fileUri = utils::path_to_file_uri(filePath);
    fileSystem->files[filePath] = std::move(content);
    shared->workspace.fileSystemProvider = fileSystem;
    return fileUri;
  }

  void setCapabilities(const workspace::InitializeCapabilities &capabilities) {
    context.setInitializeCapabilities(capabilities);
  }
};

TEST_F(LanguageServerRequestHandlerUtilsTest,
       EnsureDocumentLoadedKeepsValidationDisabledByDefaultForParsedRequests) {
  installRecordingBuilder();
  registerTestLanguage();
  const auto fileUri = installFile("lsp-loaded.test", "content");

  auto document = ensure_document_loaded(*shared, fileUri,
                                         workspace::DocumentState::Parsed);

  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(builder->waitForBuildCalls(1));
  const auto call = builder->lastBuildCall();
  EXPECT_EQ(call.uris, std::vector<std::string>({fileUri}));
  EXPECT_FALSE(validation::is_validation_enabled(call.options.validation));
  EXPECT_TRUE(std::holds_alternative<std::monostate>(call.options.validation));
}

TEST_F(
    LanguageServerRequestHandlerUtilsTest,
    EnsureDocumentLoadedForValidatedRequestsForcesValidationAndKeepsCategories) {
  installRecordingBuilder();
  registerTestLanguage();
  set_validation_configuration(*shared->workspace.configurationProvider, "test",
                               true, {"slow"});
  const auto fileUri = installFile("lsp-loaded-validated.test", "content");

  auto document = ensure_document_loaded(*shared, fileUri,
                                         workspace::DocumentState::Validated);

  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(builder->waitForBuildCalls(1));
  const auto call = builder->lastBuildCall();
  EXPECT_EQ(call.uris, std::vector<std::string>({fileUri}));
  ASSERT_TRUE(validation::is_validation_enabled(call.options.validation));
  const auto *validationOptions =
      validation::get_validation_options(call.options.validation);
  ASSERT_NE(validationOptions, nullptr);
  EXPECT_EQ(validationOptions->categories, (std::vector<std::string>{"slow"}));
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WaitUntilPhaseTurnsDocumentLoadErrorsIntoRequestFailed) {
  registerTestLanguage();

  EXPECT_THROW(
      wait_until_phase(*shared, utils::default_cancel_token,
                       test::make_file_uri("missing-lsp-loaded.test"),
                       workspace::DocumentState::Parsed),
      ::lsp::RequestError);
}

TEST_F(
    LanguageServerRequestHandlerUtilsTest,
    EnsureDocumentLoadedPropagatesRequestCancellationToBuild) {
  auto blockingBuilder = std::make_unique<BlockingBuildDocumentBuilder>();
  auto *blockingBuilderPtr = blockingBuilder.get();
  shared->workspace.documentBuilder = std::move(blockingBuilder);
  registerTestLanguage();
  const auto fileUri = installFile("lsp-cancel-build.test", "content");
  utils::CancellationTokenSource cancellation;

  auto future = std::async(std::launch::async, [&]() {
    return ensure_document_loaded(*shared, fileUri, workspace::DocumentState::Parsed,
                                  cancellation.get_token());
  });

  ASSERT_TRUE(blockingBuilderPtr->waitUntilStarted());
  cancellation.request_stop();

  EXPECT_THROW(future.get(), utils::OperationCancelled);
  EXPECT_TRUE(test::wait_until(
      [blockingBuilderPtr]() { return blockingBuilderPtr->observedCancellation(); }));
  EXPECT_TRUE(blockingBuilderPtr->waitUntilFinished());
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapOptionalPayloadMapsOptionalToNullableResult) {
  const auto empty = wrap_optional_payload<::lsp::TextDocument_HoverResult>{}(
      context, std::optional<::lsp::Hover>{}, utils::default_cancel_token);
  EXPECT_TRUE(empty.isNull());

  ::lsp::Hover hover{};
  hover.range.emplace();
  hover.range->start = text::Position(1, 2);
  hover.range->end = text::Position(1, 6);
  const auto value = wrap_optional_payload<::lsp::TextDocument_HoverResult>{}(
      context, std::optional<::lsp::Hover>{hover}, utils::default_cancel_token);
  ASSERT_FALSE(value.isNull());
  EXPECT_EQ(value.value().range->start.line, 1u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapVectorPayloadMapsVectorsWithoutSpecialCases) {
  ::lsp::DocumentLink link{};
  link.tooltip = "doc";
  const auto result =
      wrap_vector_payload<::lsp::TextDocument_DocumentLinkResult>{}(
          context, std::vector<::lsp::DocumentLink>{std::move(link)},
          utils::default_cancel_token);

  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(result.value().size(), 1u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapEmptyVectorAsNullReturnsNullForEmptyCollections) {
  const auto empty = wrap_empty_vector_as_null<
      ::lsp::TextDocument_PrepareCallHierarchyResult>{}(
      context, std::vector<::lsp::CallHierarchyItem>{},
      utils::default_cancel_token);
  EXPECT_TRUE(empty.isNull());

  ::lsp::CallHierarchyItem item{};
  item.name = "Item";
  item.kind = ::lsp::SymbolKind::Class;
  item.uri = ::lsp::DocumentUri(
      ::lsp::Uri::parse(test::make_file_uri("call-hierarchy.test")));
  const auto value = wrap_empty_vector_as_null<
      ::lsp::TextDocument_PrepareCallHierarchyResult>{}(
      context, std::vector<::lsp::CallHierarchyItem>{std::move(item)},
      utils::default_cancel_token);
  ASSERT_FALSE(value.isNull());
  EXPECT_EQ(value.value().size(), 1u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapOptionalLinksPreservesLinksWhenClientSupportsThem) {
  workspace::InitializeCapabilities capabilities;
  capabilities.definitionLinkSupport = true;
  capabilities.typeDefinitionLinkSupport = true;
  setCapabilities(capabilities);

  ::lsp::LocationLink link{};
  link.targetUri = ::lsp::DocumentUri(
      ::lsp::Uri::parse(test::make_file_uri("definition-link.test")));
  link.targetRange.start = text::Position(4, 0);
  link.targetRange.end = text::Position(4, 8);
  link.targetSelectionRange.start = text::Position(4, 2);
  link.targetSelectionRange.end = text::Position(4, 7);
  link.originSelectionRange =
      ::lsp::Range{.start = text::Position(1, 1), .end = text::Position(1, 6)};

  const auto result =
      wrap_optional_links<::lsp::TextDocument_DefinitionResult,
                          ::lsp::Definition>{GotoLinkKind::Definition}(
          context, std::vector<::lsp::LocationLink>{link},
          utils::default_cancel_token);

  ASSERT_FALSE(result.isNull());
  ASSERT_TRUE(std::holds_alternative<::lsp::Array<::lsp::DefinitionLink>>(
      result.value()));
  const auto &links = result.get<::lsp::Array<::lsp::DefinitionLink>>();
  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(links.front().targetSelectionRange.start.character, 2u);
  ASSERT_TRUE(links.front().originSelectionRange.has_value());
  EXPECT_EQ(links.front().originSelectionRange->start.line, 1u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapOptionalLinksFallsBackToLocationsWhenClientDoesNotSupportLinks) {
  ::lsp::LocationLink link{};
  link.targetUri = ::lsp::DocumentUri(
      ::lsp::Uri::parse(test::make_file_uri("definition-location.test")));
  link.targetRange.start = text::Position(3, 0);
  link.targetRange.end = text::Position(3, 10);
  link.targetSelectionRange.start = text::Position(3, 4);
  link.targetSelectionRange.end = text::Position(3, 9);

  const auto result =
      wrap_optional_links<::lsp::TextDocument_DefinitionResult,
                          ::lsp::Definition>{GotoLinkKind::Definition}(
          context, std::vector<::lsp::LocationLink>{link},
          utils::default_cancel_token);

  ASSERT_FALSE(result.isNull());
  ASSERT_TRUE(std::holds_alternative<::lsp::Definition>(result.value()));
  const auto &definition = result.get<::lsp::Definition>();
  ASSERT_TRUE(
      std::holds_alternative<::lsp::Array<::lsp::Location>>(definition));
  const auto &locations = std::get<::lsp::Array<::lsp::Location>>(definition);
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_EQ(locations.front().range.start.character, 4u);
  EXPECT_EQ(locations.front().range.end.character, 9u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapOptionalLinksFallsBackToLocationsWhenClientExplicitlyDisablesLinks) {
  workspace::InitializeCapabilities capabilities;
  capabilities.definitionLinkSupport = false;
  setCapabilities(capabilities);

  ::lsp::LocationLink link{};
  link.targetUri = ::lsp::DocumentUri(::lsp::Uri::parse(
      test::make_file_uri("definition-location-disabled.test")));
  link.targetRange.start = text::Position(6, 0);
  link.targetRange.end = text::Position(6, 10);
  link.targetSelectionRange.start = text::Position(6, 5);
  link.targetSelectionRange.end = text::Position(6, 9);

  const auto result =
      wrap_optional_links<::lsp::TextDocument_DefinitionResult,
                          ::lsp::Definition>{GotoLinkKind::Definition}(
          context, std::vector<::lsp::LocationLink>{link},
          utils::default_cancel_token);

  ASSERT_FALSE(result.isNull());
  ASSERT_TRUE(std::holds_alternative<::lsp::Definition>(result.value()));
  const auto &definition = result.get<::lsp::Definition>();
  ASSERT_TRUE(
      std::holds_alternative<::lsp::Array<::lsp::Location>>(definition));
  const auto &locations = std::get<::lsp::Array<::lsp::Location>>(definition);
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_EQ(locations.front().range.start.character, 5u);
  EXPECT_EQ(locations.front().range.end.character, 9u);
}

TEST_F(LanguageServerRequestHandlerUtilsTest,
       WrapResolvedOrOriginalPrefersResolvedValueAndFallsBackToOriginal) {
  ::lsp::WorkspaceSymbol original{};
  original.name = "original";

  auto fallback =
      wrap_resolved_or_original<::lsp::WorkspaceSymbol>{.original = original}(
          context, std::optional<::lsp::WorkspaceSymbol>{},
          utils::default_cancel_token);
  EXPECT_EQ(fallback.name, "original");

  ::lsp::WorkspaceSymbol resolved{};
  resolved.name = "resolved";
  auto value =
      wrap_resolved_or_original<::lsp::WorkspaceSymbol>{.original = original}(
          context, std::optional<::lsp::WorkspaceSymbol>{resolved},
          utils::default_cancel_token);
  EXPECT_EQ(value.name, "resolved");
}

} // namespace
} // namespace pegium
