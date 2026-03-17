#include <gtest/gtest.h>

#include <algorithm>
#include <functional>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/DefaultConfigurationProvider.hpp>
#include <pegium/workspace/DefaultWorkspaceManager.hpp>

namespace pegium::workspace {
namespace {

class TestWorkspaceManager final : public DefaultWorkspaceManager {
public:
  using DefaultWorkspaceManager::DefaultWorkspaceManager;

  std::string rootFolderOverride;
  std::function<void(
      std::span<const WorkspaceFolder>,
      const std::function<void(std::shared_ptr<Document>)> &,
      utils::CancellationToken)>
      loadAdditionalDocumentsOverride;

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
      const std::function<void(std::shared_ptr<Document>)> &collector,
      utils::CancellationToken cancelToken) override {
    if (loadAdditionalDocumentsOverride) {
      loadAdditionalDocumentsOverride(workspaceFolders, collector, cancelToken);
      return;
    }
    DefaultWorkspaceManager::loadAdditionalDocuments(workspaceFolders, collector,
                                                     cancelToken);
  }
};

TEST(DefaultWorkspaceManagerTest,
     SearchFolderIgnoresHiddenAndUnsupportedEntries) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath = std::string("/tmp/pegium-tests/workspace-search");
  const auto rootUri = utils::path_to_file_uri(rootPath);

  fileSystem->directories[rootPath] = {
      rootPath + "/.git",
      rootPath + "/node_modules",
      rootPath + "/out",
      rootPath + "/dir",
      rootPath + "/keep.test",
      rootPath + "/ignore.txt",
  };
  fileSystem->directories[rootPath + "/.git"] = {rootPath + "/.git/hidden.test"};
  fileSystem->directories[rootPath + "/node_modules"] = {
      rootPath + "/node_modules/dependency.test"};
  fileSystem->directories[rootPath + "/out"] = {rootPath + "/out/generated.test"};
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
  EXPECT_TRUE(std::ranges::find(uris, utils::path_to_file_uri(rootPath + "/keep.test")) !=
              uris.end());
  EXPECT_TRUE(std::ranges::find(uris, utils::path_to_file_uri(rootPath + "/dir/nested.test")) !=
              uris.end());
}

TEST(DefaultWorkspaceManagerTest, SearchFolderHonorsPegiumBuildIgnorePatterns) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  auto configurationProvider =
      std::make_shared<DefaultConfigurationProvider>(shared->serviceRegistry.get());
  shared->workspace.configurationProvider = configurationProvider;

  services::JsonValue::Object buildConfig;
  buildConfig.emplace("ignorePatterns", "generated, dir/nested.test");
  ConfigurationChangeParams params;
  params.settings = services::JsonValue(services::JsonValue::Object{
      {"pegium", services::JsonValue(services::JsonValue::Object{
                     {"build", services::JsonValue(std::move(buildConfig))},
                 })},
  });
  configurationProvider->updateConfiguration(params);

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  const auto rootPath =
      std::string("/tmp/pegium-tests/workspace-search-ignore-patterns");
  const auto rootUri = utils::path_to_file_uri(rootPath);

  fileSystem->directories[rootPath] = {
      rootPath + "/generated",
      rootPath + "/dir",
      rootPath + "/keep.test",
  };
  fileSystem->directories[rootPath + "/generated"] = {
      rootPath + "/generated/generated.test"};
  fileSystem->directories[rootPath + "/dir"] = {rootPath + "/dir/nested.test"};
  fileSystem->files[rootPath + "/generated/generated.test"] = "ignored";
  fileSystem->files[rootPath + "/dir/nested.test"] = "ignored";
  fileSystem->files[rootPath + "/keep.test"] = "kept";
  shared->workspace.fileSystemProvider = fileSystem;

  DefaultWorkspaceManager manager(*shared);
  const auto uris = manager.searchFolder(rootUri);

  ASSERT_EQ(uris.size(), 1u);
  EXPECT_EQ(uris.front(), utils::path_to_file_uri(rootPath + "/keep.test"));
}

TEST(DefaultWorkspaceManagerTest,
     InitializedLoadsWorkspaceDocumentsAndMarksManagerReady) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

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

  auto future = manager.initialized(InitializedParams{});
  future.get();
  manager.waitUntilReady();

  EXPECT_TRUE(manager.isReady());
  ASSERT_EQ(manager.workspaceFolders().size(), 1u);

  const auto documentId = shared->workspace.documents->getDocumentId(fileUri);
  auto document = shared->workspace.documents->getDocument(documentId);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, fileUri);
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultWorkspaceManagerTest,
     InitializedLoadsAdditionalDocumentsWithoutWorkspaceFolders) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  TestWorkspaceManager manager(*shared);
  manager.initialBuildOptions().validation.enabled = true;

  const auto libraryUri = test::make_file_uri("library.test");
  manager.loadAdditionalDocumentsOverride =
      [&shared, libraryUri](
          std::span<const WorkspaceFolder>,
          const std::function<void(std::shared_ptr<Document>)> &collector,
          utils::CancellationToken cancelToken) {
        auto document = shared->workspace.documents->createDocument(
            libraryUri, "library", "test", cancelToken);
        ASSERT_NE(document, nullptr);
        collector(document);
      };

  manager.initialize(InitializeParams{});

  auto future = manager.initialized(InitializedParams{});
  future.get();
  manager.waitUntilReady();

  EXPECT_TRUE(manager.isReady());
  auto document = shared->workspace.documents->getDocument(libraryUri);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, libraryUri);
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultWorkspaceManagerTest,
     InitializedUsesOverriddenRootFolderForWorkspaceTraversal) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

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
  manager.waitUntilReady();

  EXPECT_TRUE(manager.isReady());
  EXPECT_EQ(shared->workspace.documents->getDocument(ignoredUri), nullptr);
  auto included = shared->workspace.documents->getDocument(includedUri);
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(included->uri, includedUri);
}

} // namespace
} // namespace pegium::workspace
