#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/utils/FunctionRef.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/DefaultWorkspaceManager.hpp>

namespace pegium::workspace {
namespace {

class BlockingBuildDocumentBuilder final : public DocumentBuilder {
public:
  [[nodiscard]] BuildOptions &updateBuildOptions() noexcept override {
    return _options;
  }

  [[nodiscard]] const BuildOptions &updateBuildOptions() const noexcept override {
    return _options;
  }

  void build(std::span<const std::shared_ptr<Document>>,
             const BuildOptions & = {},
             utils::CancellationToken cancelToken = {}) const override {
    _started = true;

    try {
      while (!_released.load() && !cancelToken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (cancelToken.stop_requested()) {
        _observedCancellation = true;
        utils::throw_if_cancelled(cancelToken);
      }
    } catch (...) {
      _finished = true;
      throw;
    }

    _finished = true;
  }

  void update(std::span<const DocumentId>, std::span<const DocumentId>,
              utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
  }

  utils::ScopedDisposable
  onUpdate(std::function<void(std::span<const DocumentId>,
                              std::span<const DocumentId>)>) const override {
    return {};
  }

  utils::ScopedDisposable
  onBuildPhase(DocumentState,
               std::function<void(std::span<const std::shared_ptr<Document>>,
                                  utils::CancellationToken)>) const override {
    return {};
  }

  utils::ScopedDisposable
  onDocumentPhase(DocumentState,
                  std::function<void(const std::shared_ptr<Document> &,
                                     utils::CancellationToken)>) const override {
    return {};
  }

  void waitUntil(DocumentState,
                 utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
  }

  [[nodiscard]] DocumentId
  waitUntil(DocumentState, DocumentId documentId,
            utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    return documentId;
  }

  void resetToState(Document &document, DocumentState state) const override {
    document.state = state;
  }

  void release() { _released = true; }

  [[nodiscard]] bool
  waitUntilStarted(std::chrono::milliseconds timeout =
                       std::chrono::milliseconds(1000)) const {
    return test::wait_until([this]() { return _started.load(); }, timeout);
  }

  [[nodiscard]] bool
  waitUntilFinished(std::chrono::milliseconds timeout =
                        std::chrono::milliseconds(1000)) const {
    return test::wait_until([this]() { return _finished.load(); }, timeout);
  }

  [[nodiscard]] bool observedCancellation() const noexcept {
    return _observedCancellation.load();
  }

private:
  mutable BuildOptions _options;
  mutable std::atomic<bool> _started = false;
  mutable std::atomic<bool> _released = false;
  mutable std::atomic<bool> _finished = false;
  mutable std::atomic<bool> _observedCancellation = false;
};

class TestWorkspaceManager final : public DefaultWorkspaceManager {
public:
  using DefaultWorkspaceManager::DefaultWorkspaceManager;

  std::string rootFolderOverride;
  std::function<void(std::span<const WorkspaceFolder>,
                     utils::function_ref<void(std::shared_ptr<Document>)>,
                     utils::CancellationToken)>
      loadAdditionalDocumentsOverride;
  std::function<void(std::span<const WorkspaceFolder>, utils::CancellationToken)>
      initializeWorkspaceOverride;

protected:
  std::string
  getRootFolder(const WorkspaceFolder &workspaceFolder) const override {
    if (!rootFolderOverride.empty()) {
      return rootFolderOverride;
    }
    return DefaultWorkspaceManager::getRootFolder(workspaceFolder);
  }

  void loadAdditionalDocuments(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::function_ref<void(std::shared_ptr<Document>)> collector,
      utils::CancellationToken cancelToken) override {
    if (loadAdditionalDocumentsOverride) {
      loadAdditionalDocumentsOverride(workspaceFolders, collector, cancelToken);
      return;
    }
    DefaultWorkspaceManager::loadAdditionalDocuments(workspaceFolders, collector,
                                                     cancelToken);
  }

  void initializeWorkspace(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) override {
    if (initializeWorkspaceOverride) {
      initializeWorkspaceOverride(workspaceFolders, cancelToken);
      return;
    }
    DefaultWorkspaceManager::initializeWorkspace(workspaceFolders, cancelToken);
  }
};

TEST(DefaultWorkspaceManagerTest, WorkspaceFoldersUnavailableBeforeInitialize) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);

  DefaultWorkspaceManager manager(*shared);
  EXPECT_FALSE(manager.workspaceFolders().has_value());
}

TEST(DefaultWorkspaceManagerTest,
     SearchFolderIgnoresHiddenAndUnsupportedEntries) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath = std::string("/tmp/pegium-tests/workspace-search");
  const auto rootUri = utils::path_to_file_uri(rootPath);

  fileSystem->directories[rootPath] = {
      rootPath + "/.git", rootPath + "/node_modules", rootPath + "/out",
      rootPath + "/dir",  rootPath + "/keep.test",    rootPath + "/ignore.txt",
  };
  fileSystem->directories[rootPath + "/.git"] = {rootPath +
                                                 "/.git/hidden.test"};
  fileSystem->directories[rootPath + "/node_modules"] = {
      rootPath + "/node_modules/dependency.test"};
  fileSystem->directories[rootPath + "/out"] = {rootPath +
                                                "/out/generated.test"};
  fileSystem->directories[rootPath + "/dir"] = {rootPath + "/dir/nested.test"};
  fileSystem->files[rootPath + "/keep.test"] = "alpha";
  fileSystem->files[rootPath + "/ignore.txt"] = "alpha";
  fileSystem->files[rootPath + "/.git/hidden.test"] = "alpha";
  fileSystem->files[rootPath + "/node_modules/dependency.test"] = "alpha";
  fileSystem->files[rootPath + "/out/generated.test"] = "alpha";
  fileSystem->files[rootPath + "/dir/nested.test"] = "alpha";
  shared->workspace.fileSystemProvider = fileSystem;

  DefaultWorkspaceManager manager(*shared);
  const auto uris = manager.searchFolder(rootUri);

  EXPECT_EQ(uris.size(), 2u);
  EXPECT_TRUE(std::ranges::find(
                  uris, utils::path_to_file_uri(rootPath + "/keep.test")) !=
              uris.end());
  EXPECT_TRUE(std::ranges::find(uris, utils::path_to_file_uri(
                                          rootPath + "/dir/nested.test")) !=
              uris.end());
}

TEST(DefaultWorkspaceManagerTest,
     SearchFolderReturnsEmptyWhenRootDirectoryCannotBeRead) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  shared->workspace.fileSystemProvider = fileSystem;

  DefaultWorkspaceManager manager(*shared);
  const auto uris =
      manager.searchFolder(test::make_file_uri("workspace-missing-root"));

  EXPECT_TRUE(uris.empty());
  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::WorkspaceDirectoryReadFailed);
  EXPECT_NE(observation->message.find("workspace-missing-root"),
            std::string::npos);
}

TEST(DefaultWorkspaceManagerTest,
     InitializedLoadsWorkspaceDocumentsAndMarksManagerReady) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath = std::string("/tmp/pegium-tests/workspace-init");
  const auto rootUri = utils::path_to_file_uri(rootPath);
  const auto filePath = rootPath + "/main.test";
  const auto fileUri = utils::path_to_file_uri(filePath);

  fileSystem->directories[rootPath] = {filePath};
  fileSystem->files[filePath] = "alpha";
  shared->workspace.fileSystemProvider = fileSystem;

  DefaultWorkspaceManager manager(*shared);

  InitializeParams initializeParams{};
  initializeParams.workspaceFolders.push_back(
      WorkspaceFolder{.uri = rootUri, .name = "workspace"});
  manager.initialize(initializeParams);

  ASSERT_TRUE(manager.workspaceFolders().has_value());
  ASSERT_EQ(manager.workspaceFolders()->size(), 1u);
  EXPECT_EQ(manager.ready().wait_for(std::chrono::milliseconds(0)),
            std::future_status::timeout);

  auto future = manager.initialized(InitializedParams{});
  future.get();
  manager.ready().get();

  ASSERT_TRUE(manager.workspaceFolders().has_value());
  ASSERT_EQ(manager.workspaceFolders()->size(), 1u);

  const auto documentId = shared->workspace.documents->getDocumentId(fileUri);
  auto document = shared->workspace.documents->getDocument(documentId);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, fileUri);
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultWorkspaceManagerTest,
     InitializedLoadsAdditionalDocumentsWithoutWorkspaceFolders) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  TestWorkspaceManager manager(*shared);
  manager.initialBuildOptions().validation = true;

  const auto libraryUri = test::make_file_uri("library.test");
  manager.loadAdditionalDocumentsOverride =
      [&shared, libraryUri](
          std::span<const WorkspaceFolder>,
          const std::function<void(std::shared_ptr<Document>)> &collector,
          utils::CancellationToken cancelToken) {
        auto document = shared->workspace.documents->createDocument(
            libraryUri, "library", cancelToken);
        ASSERT_NE(document, nullptr);
        collector(document);
      };

  manager.initialize(InitializeParams{});

  auto future = manager.initialized(InitializedParams{});
  future.get();
  manager.ready().get();

  auto document = shared->workspace.documents->getDocument(libraryUri);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, libraryUri);
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultWorkspaceManagerTest,
     ReadyResolvesBeforeInitialBuildCompletes) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto blockingBuilder = std::make_unique<BlockingBuildDocumentBuilder>();
  auto *blockingBuilderPtr = blockingBuilder.get();
  shared->workspace.documentBuilder = std::move(blockingBuilder);

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath = std::string("/tmp/pegium-tests/workspace-ready-bootstrap");
  const auto rootUri = utils::path_to_file_uri(rootPath);
  const auto filePath = rootPath + "/main.test";
  const auto fileUri = utils::path_to_file_uri(filePath);
  fileSystem->directories[rootPath] = {filePath};
  fileSystem->files[filePath] = "alpha";
  shared->workspace.fileSystemProvider = fileSystem;

  DefaultWorkspaceManager manager(*shared);

  InitializeParams initializeParams{};
  initializeParams.workspaceFolders.push_back(
      WorkspaceFolder{.uri = rootUri, .name = "workspace"});
  manager.initialize(initializeParams);

  auto future = manager.initialized(InitializedParams{});
  ASSERT_TRUE(blockingBuilderPtr->waitUntilStarted());

  EXPECT_EQ(manager.ready().wait_for(std::chrono::milliseconds(0)),
            std::future_status::ready);
  EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)),
            std::future_status::timeout);

  const auto documentId = shared->workspace.documents->getDocumentId(fileUri);
  ASSERT_NE(documentId, InvalidDocumentId);
  auto document = shared->workspace.documents->getDocument(documentId);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, fileUri);
  EXPECT_LT(document->state, DocumentState::Validated);

  blockingBuilderPtr->release();

  EXPECT_NO_THROW(future.get());
  EXPECT_TRUE(blockingBuilderPtr->waitUntilFinished());
}

TEST(DefaultWorkspaceManagerTest,
     InitializedUsesOverriddenRootFolderForWorkspaceTraversal) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath = std::string("/tmp/pegium-tests/workspace-root");
  const auto srcPath = rootPath + "/src";
  const auto rootUri = utils::path_to_file_uri(rootPath);
  const auto srcUri = utils::path_to_file_uri(srcPath);
  const auto ignoredPath = rootPath + "/ignored.test";
  const auto ignoredUri = utils::path_to_file_uri(ignoredPath);
  const auto includedPath = srcPath + "/main.test";
  const auto includedUri = utils::path_to_file_uri(includedPath);

  fileSystem->directories[rootPath] = {ignoredPath, srcPath};
  fileSystem->directories[srcPath] = {includedPath};
  fileSystem->files[ignoredPath] = "ignored";
  fileSystem->files[includedPath] = "included";
  shared->workspace.fileSystemProvider = fileSystem;

  TestWorkspaceManager manager(*shared);
  manager.rootFolderOverride = srcUri;

  InitializeParams initializeParams{};
  initializeParams.workspaceFolders.push_back(
      WorkspaceFolder{.uri = rootUri, .name = "workspace"});
  manager.initialize(initializeParams);

  auto future = manager.initialized(InitializedParams{});
  future.get();
  manager.ready().get();

  EXPECT_EQ(shared->workspace.documents->getDocument(ignoredUri), nullptr);
  auto included = shared->workspace.documents->getDocument(includedUri);
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(included->uri, includedUri);
}

TEST(DefaultWorkspaceManagerTest,
     InitializedPropagatesWorkspaceLockCancellationToInitializeWorkspace) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);

  TestWorkspaceManager manager(*shared);
  manager.initialize(InitializeParams{});

  std::promise<void> started;
  auto startedFuture = started.get_future();
  std::atomic<bool> observedCancellation = false;

  manager.initializeWorkspaceOverride =
      [&started, &observedCancellation](
          std::span<const WorkspaceFolder> workspaceFolders,
          utils::CancellationToken cancelToken) {
        EXPECT_TRUE(workspaceFolders.empty());
        started.set_value();
        while (!cancelToken.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        observedCancellation = true;
        utils::throw_if_cancelled(cancelToken);
      };

  auto future = manager.initialized(InitializedParams{});
  ASSERT_EQ(startedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  shared->workspace.workspaceLock->cancelWrite();

  EXPECT_NO_THROW(future.get());
  EXPECT_TRUE(test::wait_until(
      [&observedCancellation]() { return observedCancellation.load(); }));
}

TEST(DefaultWorkspaceManagerTest,
     InitializedPublishesWorkspaceBootstrapFailureForInitializeWorkspaceError) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;

  TestWorkspaceManager manager(*shared);

  InitializeParams initializeParams{};
  initializeParams.workspaceFolders.push_back(
      WorkspaceFolder{.uri = test::make_file_uri("bootstrap-root"),
                      .name = "workspace"});
  manager.initialize(initializeParams);
  manager.initializeWorkspaceOverride =
      [](std::span<const WorkspaceFolder>, utils::CancellationToken) {
        throw std::runtime_error("bootstrap exploded");
      };

  auto future = manager.initialized(InitializedParams{});

  EXPECT_THROW(future.get(), std::runtime_error);
  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::WorkspaceBootstrapFailed);
  EXPECT_NE(observation->message.find("bootstrap exploded"),
            std::string::npos);
  EXPECT_EQ(observation->uri, test::make_file_uri("bootstrap-root"));
}

} // namespace
} // namespace pegium::workspace
