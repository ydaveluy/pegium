#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <span>
#include <thread>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/runtime/LanguageClient.hpp>
#include <pegium/lsp/runtime/internal/LanguageClientFactory.hpp>
#include <pegium/lsp/workspace/DefaultDocumentUpdateHandler.hpp>
#include <pegium/core/workspace/Configuration.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>

namespace pegium {
namespace {

void set_validation_configuration(
    workspace::ConfigurationProvider &configurationProvider,
    std::string_view languageId, bool enabled,
    std::vector<std::string> categories) {
  services::JsonValue::Array categoryValues;
  categoryValues.reserve(categories.size());
  for (auto &category : categories) {
    categoryValues.emplace_back(std::move(category));
  }

  workspace::ConfigurationChangeParams params;
  params.settings = services::JsonValue(services::JsonValue::Object{
      {std::string(languageId),
       services::JsonValue(services::JsonValue::Object{
           {"validation",
            services::JsonValue(services::JsonValue::Object{
                {"enabled", services::JsonValue(enabled)},
                {"categories", services::JsonValue(std::move(categoryValues))},
            })},
       })},
  });
  configurationProvider.updateConfiguration(params);
}

void clear_language_configuration(
    workspace::ConfigurationProvider &configurationProvider,
    std::string_view languageId) {
  workspace::ConfigurationChangeParams params;
  params.settings = services::JsonValue(services::JsonValue::Object{
      {std::string(languageId),
       services::JsonValue(services::JsonValue::Object{})},
  });
  configurationProvider.updateConfiguration(params);
}

class BlockingUpdateDocumentBuilder final : public workspace::DocumentBuilder {
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
    utils::throw_if_cancelled(cancelToken);
  }

  void update(std::span<const workspace::DocumentId>,
              std::span<const workspace::DocumentId>,
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

class DefaultDocumentUpdateHandlerTest : public ::testing::Test {
protected:
  std::unique_ptr<pegium::SharedServices> shared =
      test::make_empty_shared_services();
  test::RecordingDocumentBuilder *builder = nullptr;
  std::shared_ptr<test::RecordingObservabilitySink> recordingSink =
      std::make_shared<test::RecordingObservabilitySink>();
  std::unique_ptr<DefaultDocumentUpdateHandler> handler;

  DefaultDocumentUpdateHandlerTest() {
    pegium::services::installDefaultSharedCoreServices(*shared);
    shared->observabilitySink = recordingSink;
    pegium::installDefaultSharedLspServices(*shared);
    pegium::test::initialize_shared_workspace_for_tests(*shared);
  }

  void SetUp() override {
    builder = new test::RecordingDocumentBuilder();
    shared->workspace.documentBuilder.reset(builder);
    handler = std::make_unique<DefaultDocumentUpdateHandler>(*shared);
  }

  void TearDown() override {
    if (shared != nullptr && shared->workspace.workspaceLock != nullptr) {
      auto drain = shared->workspace.workspaceLock->write(
          [](const utils::CancellationToken &) {});
      if (drain.valid()) {
        drain.get();
      }
    }
    handler.reset();
  }

  std::shared_ptr<workspace::TextDocument>
  makeTextDocument(std::string_view fileName, std::string_view text,
                   std::string languageId = {},
                   std::optional<std::int64_t> version = std::nullopt) {
    return test::make_text_document(test::make_file_uri(fileName),
                                    std::move(languageId), text,
                                    version);
  }

  std::shared_ptr<workspace::Document>
  addExistingDocument(std::string_view fileName, std::string text,
                      std::string languageId = "test") {
    auto existing = std::make_shared<workspace::Document>(
        test::make_text_document(test::make_file_uri(fileName),
                                 std::move(languageId), std::move(text)));
    shared->workspace.documents->addDocument(existing);
    return existing;
  }
};

class ControlledReadyWorkspaceManager final : public workspace::WorkspaceManager {
public:
  explicit ControlledReadyWorkspaceManager(std::shared_future<void> readyFuture)
      : _readyFuture(std::move(readyFuture)) {}

  [[nodiscard]] workspace::BuildOptions &initialBuildOptions() override {
    return _options;
  }

  [[nodiscard]] const workspace::BuildOptions &initialBuildOptions()
      const override {
    return _options;
  }

  [[nodiscard]] std::shared_future<void> ready() const override {
    return _readyFuture;
  }

  [[nodiscard]] std::optional<std::vector<workspace::WorkspaceFolder>>
  workspaceFolders() const override {
    return std::vector<workspace::WorkspaceFolder>{};
  }

  void initialize(const workspace::InitializeParams &params) override {
    (void)params;
  }

  [[nodiscard]] std::future<void>
  initialized(const workspace::InitializedParams &params) override {
    (void)params;
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  void initializeWorkspace(
      std::span<const workspace::WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) override {
    (void)workspaceFolders;
    utils::throw_if_cancelled(cancelToken);
  }

  [[nodiscard]] std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const override {
    (void)workspaceUri;
    return {};
  }

  [[nodiscard]] bool
  shouldIncludeEntry(const workspace::FileSystemNode &entry) const override {
    (void)entry;
    return true;
  }

private:
  mutable workspace::BuildOptions _options;
  std::shared_future<void> _readyFuture;
};

template <typename T> std::future<T> make_ready_future(T value) {
  std::promise<T> promise;
  promise.set_value(std::move(value));
  return promise.get_future();
}

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

class RecordingLanguageClient final : public LanguageClient {
public:
  std::future<void> registerCapability(::lsp::RegistrationParams params) override {
    registrations.push_back(std::move(params));
    return make_ready_future();
  }

  std::future<std::vector<services::JsonValue>>
  fetchConfiguration(::lsp::ConfigurationParams params) override {
    configurationRequests.push_back(std::move(params));
    return make_ready_future(std::vector<services::JsonValue>{});
  }

  std::vector<::lsp::RegistrationParams> registrations;
  std::vector<::lsp::ConfigurationParams> configurationRequests;
};

class ThrowingRegisterLanguageClient final : public LanguageClient {
public:
  std::future<void> registerCapability(::lsp::RegistrationParams) override {
    return std::async(std::launch::deferred, []() -> void {
      throw std::runtime_error("register failed");
    });
  }

  std::future<std::vector<services::JsonValue>>
  fetchConfiguration(::lsp::ConfigurationParams) override {
    return make_ready_future(std::vector<services::JsonValue>{});
  }
};

class EmptyWatcherDocumentUpdateHandler final : public DefaultDocumentUpdateHandler {
public:
  using DefaultDocumentUpdateHandler::DefaultDocumentUpdateHandler;

protected:
  std::vector<::lsp::FileSystemWatcher> getWatchers() const override {
    return {};
  }
};

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidOpenDocumentIsNoOpForDefaultHandler) {
  auto document = makeTextDocument("opened.test", "content");

  handler->didOpenDocument({.document = document});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(50)));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSchedulesDocumentRebuild) {
  auto document = makeTextDocument("changed.test", "content");

  handler->didChangeContent({.document = document});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  const auto documentId =
      shared->workspace.documents->getDocumentId(document->uri());
  ASSERT_NE(documentId, workspace::InvalidDocumentId);
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{documentId});
  EXPECT_TRUE(call.deletedDocumentIds.empty());
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeWatchedFilesDeduplicatesUris) {
  const auto changedUri = test::make_file_uri("one.test");
  const auto deletedUri = test::make_file_uri("gone.test");
  const auto changedDocumentId =
      shared->workspace.documents->getOrCreateDocumentId(changedUri);
  const auto deletedDocumentId =
      shared->workspace.documents->getOrCreateDocumentId(deletedUri);

  ::lsp::DidChangeWatchedFilesParams params{};
  params.changes = {
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(changedUri)),
          .type = ::lsp::FileChangeType::Changed,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(changedUri)),
          .type = ::lsp::FileChangeType::Changed,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(deletedUri)),
          .type = ::lsp::FileChangeType::Deleted,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(deletedUri)),
          .type = ::lsp::FileChangeType::Deleted,
      },
  };

  handler->didChangeWatchedFiles(params);

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{changedDocumentId});
  EXPECT_EQ(call.deletedDocumentIds,
            std::vector<workspace::DocumentId>{deletedDocumentId});
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       RegistersWatchedFilesWhenClientSupportsDynamicRegistration) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler messageHandler(connection);
  shared->lsp.languageClient = make_message_handler_language_client(
      messageHandler, *shared->observabilitySink);

  ::lsp::InitializeParams params{};
  params.capabilities.workspace.emplace();
  params.capabilities.workspace->didChangeWatchedFiles.emplace();
  params.capabilities.workspace->didChangeWatchedFiles->dynamicRegistration =
      true;

  (void)shared->lsp.languageServer->initialize(params);
  shared->lsp.languageServer->initialized(::lsp::InitializedParams{});

  EXPECT_TRUE(test::wait_until([&]() {
    return stream.written().find(
               ::lsp::requests::Client_RegisterCapability::Method) !=
           std::string::npos;
  }));

  const auto request = test::parse_last_written_message(stream.written()).object();
  EXPECT_EQ(request.get("method").string(),
            ::lsp::requests::Client_RegisterCapability::Method);
  shared->lsp.languageClient.reset();
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DoesNotRegisterWatchedFilesWithoutDynamicRegistrationSupport) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler messageHandler(connection);
  shared->lsp.languageClient = make_message_handler_language_client(
      messageHandler, *shared->observabilitySink);

  (void)shared->lsp.languageServer->initialize(::lsp::InitializeParams{});
  shared->lsp.languageServer->initialized(::lsp::InitializedParams{});

  EXPECT_TRUE(stream.written().empty());
  shared->lsp.languageClient.reset();
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DoesNotRegisterWatchedFilesWhenWatcherListIsEmpty) {
  shared->lsp.languageClient = std::make_unique<RecordingLanguageClient>();
  handler.reset();
  shared->lsp.documentUpdateHandler =
      std::make_unique<EmptyWatcherDocumentUpdateHandler>(*shared);

  ::lsp::InitializeParams params{};
  params.capabilities.workspace.emplace();
  params.capabilities.workspace->didChangeWatchedFiles.emplace();
  params.capabilities.workspace->didChangeWatchedFiles->dynamicRegistration =
      true;

  (void)shared->lsp.languageServer->initialize(params);
  shared->lsp.languageServer->initialized(::lsp::InitializedParams{});

  auto *client =
      static_cast<RecordingLanguageClient *>(shared->lsp.languageClient.get());
  ASSERT_NE(client, nullptr);
  const auto hasWatchedFilesRegistration = std::ranges::any_of(
      client->registrations, [](const ::lsp::RegistrationParams &params) {
        return std::ranges::any_of(
            params.registrations, [](const ::lsp::Registration &registration) {
              return registration.id ==
                         "pegium.workspace.didChangeWatchedFiles" ||
                     registration.method ==
                         ::lsp::notifications::Workspace_DidChangeWatchedFiles::
                             Method;
            });
      });
  EXPECT_FALSE(hasWatchedFilesRegistration);
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       PublishesObservationWhenWatchedFileRegistrationFails) {
  shared->lsp.languageClient = std::make_unique<ThrowingRegisterLanguageClient>();

  ::lsp::InitializeParams params{};
  params.capabilities.workspace.emplace();
  params.capabilities.workspace->didChangeWatchedFiles.emplace();
  params.capabilities.workspace->didChangeWatchedFiles->dynamicRegistration =
      true;

  (void)shared->lsp.languageServer->initialize(params);
  shared->lsp.languageServer->initialized(::lsp::InitializedParams{});

  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::LspRuntimeBackgroundTaskFailed);
  EXPECT_EQ(observation->category, "workspace/didChangeWatchedFiles.register");
  EXPECT_NE(observation->message.find("register failed"), std::string::npos);
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSkipsRedundantSnapshotForExistingDocument) {
  auto existing = addExistingDocument("redundant.test", "content");
  auto textDocument =
      makeTextDocument("redundant.test", existing->textDocument().getText(),
                                       existing->textDocument().languageId(), 7);

  handler->didChangeContent({.document = textDocument});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(50)));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSkipsRedundantSnapshotWhenOnlyLanguageIdChanged) {
  addExistingDocument("redundant-language.test", "content", "test");
  auto textDocument =
      makeTextDocument("redundant-language.test", "content", "other", 7);

  handler->didChangeContent({.document = textDocument});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(50)));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentPreservesBuilderDefaultsWithoutValidationConfiguration) {
  builder->updateBuildOptions().validation =
      validation::ValidationOptions{.categories = {"built-in", "fast"}};

  auto existing = addExistingDocument("default-options.test", "old");
  auto textDocument =
      makeTextDocument("default-options.test", "new",
                       existing->textDocument().languageId());

  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  ASSERT_TRUE(validation::is_validation_enabled(call.options.validation));
  const auto *callValidationOptions =
      validation::get_validation_options(call.options.validation);
  ASSERT_NE(callValidationOptions, nullptr);
  EXPECT_EQ(callValidationOptions->categories,
            (std::vector<std::string>{"built-in", "fast"}));
  ASSERT_TRUE(validation::is_validation_enabled(
      builder->updateBuildOptions().validation));
  const auto *builderValidationOptions = validation::get_validation_options(
      builder->updateBuildOptions().validation);
  ASSERT_NE(builderValidationOptions, nullptr);
  EXPECT_EQ(builderValidationOptions->categories,
            (std::vector<std::string>{"built-in", "fast"}));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentUsesExplicitValidationConfigurationTemporarily) {
  builder->updateBuildOptions().validation =
      validation::ValidationOptions{.categories = {"built-in", "fast"}};

  set_validation_configuration(*shared->workspace.configurationProvider, "test",
                               true, {"slow"});

  auto existing = addExistingDocument("configured-options.test", "old");
  auto textDocument =
      makeTextDocument("configured-options.test", "new",
                       existing->textDocument().languageId());

  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  ASSERT_TRUE(validation::is_validation_enabled(call.options.validation));
  const auto *callValidationOptions =
      validation::get_validation_options(call.options.validation);
  ASSERT_NE(callValidationOptions, nullptr);
  EXPECT_EQ(callValidationOptions->categories,
            (std::vector<std::string>{"slow"}));

  clear_language_configuration(*shared->workspace.configurationProvider,
                               "test");

  const workspace::TextDocumentContentChangeEvent change{.text = "newer"};
  (void)workspace::TextDocument::update(*textDocument,
                                        std::span(&change, std::size_t{1}),
                                        textDocument->version() + 1);
  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(2));
  const auto restoredCall = builder->lastCall();
  ASSERT_TRUE(
      validation::is_validation_enabled(restoredCall.options.validation));
  const auto *restoredValidationOptions =
      validation::get_validation_options(restoredCall.options.validation);
  ASSERT_NE(restoredValidationOptions, nullptr);
  EXPECT_EQ(restoredValidationOptions->categories,
            (std::vector<std::string>{"built-in", "fast"}));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentPropagatesWorkspaceLockCancellationToBuilderUpdate) {
  auto blockingBuilder = std::make_unique<BlockingUpdateDocumentBuilder>();
  auto *blockingBuilderPtr = blockingBuilder.get();
  shared->workspace.documentBuilder = std::move(blockingBuilder);
  handler = std::make_unique<DefaultDocumentUpdateHandler>(*shared);

  auto document = makeTextDocument("cancelled-open.test", "content");
  handler->didChangeContent({.document = document});

  ASSERT_TRUE(blockingBuilderPtr->waitUntilStarted());
  shared->workspace.workspaceLock->cancelWrite();

  EXPECT_TRUE(test::wait_until(
      [blockingBuilderPtr]() { return blockingBuilderPtr->observedCancellation(); }));
  EXPECT_TRUE(blockingBuilderPtr->waitUntilFinished());
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentWaitsForWorkspaceManagerReadyBeforeUpdating) {
  std::promise<void> readyPromise;
  auto readyFuture = readyPromise.get_future().share();
  shared->workspace.workspaceManager =
      std::make_unique<ControlledReadyWorkspaceManager>(readyFuture);
  handler = std::make_unique<DefaultDocumentUpdateHandler>(*shared);

  auto document = makeTextDocument("ready-gated.test", "content");

  handler->didChangeContent({.document = document});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(50)));

  readyPromise.set_value();

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  const auto documentId =
      shared->workspace.documents->getDocumentId(document->uri());
  ASSERT_NE(documentId, workspace::InvalidDocumentId);
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{documentId});
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSwallowsWorkspaceManagerReadyFailures) {
  std::promise<void> readyPromise;
  readyPromise.set_exception(
      std::make_exception_ptr(std::runtime_error("ready failed")));
  shared->workspace.workspaceManager =
      std::make_unique<ControlledReadyWorkspaceManager>(
          readyPromise.get_future().share());
  handler = std::make_unique<DefaultDocumentUpdateHandler>(*shared);

  auto document = makeTextDocument("ready-failure.test", "content");

  handler->didChangeContent({.document = document});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(100)));
  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::DocumentUpdateDispatchFailed);
  EXPECT_NE(observation->message.find("ready failed"), std::string::npos);
  EXPECT_EQ(observation->uri, document->uri());
}

} // namespace
} // namespace pegium
