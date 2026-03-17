#include <gtest/gtest.h>

#include <atomic>
#include <future>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>
#include <pegium/lsp/LanguageServerRuntimeState.hpp>
#include <pegium/workspace/Configuration.hpp>
#include <pegium/workspace/DocumentBuilder.hpp>
#include <pegium/workspace/WorkspaceManager.hpp>

namespace pegium::lsp {
namespace {

class BlockingConfigurationProvider final
    : public workspace::ConfigurationProvider {
public:
  explicit BlockingConfigurationProvider(std::shared_future<void> gate)
      : gate(std::move(gate)) {}

  void initialize(const workspace::InitializeParams &params) override {
    (void)params;
  }

  std::future<void>
  initialized(const workspace::InitializedParams &params) override {
    ++initializedCalls;
    sawRegisterHandler = static_cast<bool>(params.registerDidChangeConfiguration);
    sawFetchHandler = static_cast<bool>(params.fetchConfiguration);
    auto waitGate = gate;
    return std::async(std::launch::async,
                      [waitGate]() mutable { waitGate.wait(); });
  }

  bool isReady() const noexcept override { return false; }

  void updateConfiguration(
      const workspace::ConfigurationChangeParams &params) override {
    (void)params;
  }

  std::optional<services::JsonValue>
  getConfiguration(std::string_view languageId,
                   std::string_view key) const override {
    (void)languageId;
    (void)key;
    return std::nullopt;
  }

  utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
          listener) override {
    (void)listener;
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(
      std::string_view languageId) const override {
    (void)languageId;
    return {};
  }

  workspace::WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view workspaceUri) const override {
    (void)workspaceUri;
    return {};
  }

  std::shared_future<void> gate;
  std::atomic<int> initializedCalls = 0;
  bool sawRegisterHandler = false;
  bool sawFetchHandler = false;
};

class BlockingWorkspaceManager final : public workspace::WorkspaceManager {
public:
  explicit BlockingWorkspaceManager(std::shared_future<void> gate)
      : gate(std::move(gate)) {}

  workspace::BuildOptions &initialBuildOptions() override { return options; }

  const workspace::BuildOptions &initialBuildOptions() const override {
    return options;
  }

  bool isReady() const noexcept override { return false; }

  void waitUntilReady(utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
  }

  std::vector<workspace::WorkspaceFolder> workspaceFolders() const override {
    return {};
  }

  void initialize(const workspace::InitializeParams &params) override {
    (void)params;
  }

  std::future<void> initialized(
      const workspace::InitializedParams &params,
      utils::CancellationToken cancelToken = {}) override {
    (void)params;
    utils::throw_if_cancelled(cancelToken);
    ++initializedCalls;
    auto waitGate = gate;
    return std::async(std::launch::async,
                      [waitGate]() mutable { waitGate.wait(); });
  }

  std::future<void> initializeWorkspace(
      std::span<const workspace::WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) override {
    (void)workspaceFolders;
    utils::throw_if_cancelled(cancelToken);
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const override {
    (void)workspaceUri;
    return {};
  }

  bool shouldIncludeEntry(std::string_view workspaceUri, std::string_view path,
                          bool isDirectory) const override {
    (void)workspaceUri;
    (void)path;
    (void)isDirectory;
    return true;
  }

  std::shared_future<void> gate;
  workspace::BuildOptions options{};
  std::atomic<int> initializedCalls = 0;
};

TEST(LanguageServerLifecycleHandlersTest,
     InitializedNotificationDoesNotBlockOnBackgroundInitializationFutures) {
  auto shared = test::make_shared_services();

  std::promise<void> gatePromise;
  auto gate = gatePromise.get_future().share();

  auto configurationProvider =
      std::make_shared<BlockingConfigurationProvider>(gate);
  auto workspaceManager = std::make_unique<BlockingWorkspaceManager>(gate);
  auto *workspaceManagerPtr = workspaceManager.get();

  shared->workspace.configurationProvider = configurationProvider;
  shared->workspace.workspaceManager = std::move(workspaceManager);

  DefaultLanguageServer server(*shared);
  LanguageServerRuntimeState runtimeState;
  LanguageServerHandlerContext context(server, *shared, runtimeState);

  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);
  addLanguageServerLifecycleHandlers(context, handler, *shared);

  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::Initialized::Method, ::lsp::InitializedParams{}));

  auto future = std::async(std::launch::async,
                           [&handler]() { handler.processIncomingMessages(); });

  const auto status = future.wait_for(std::chrono::milliseconds(200));
  EXPECT_EQ(status, std::future_status::ready);
  EXPECT_EQ(configurationProvider->initializedCalls.load(), 1);
  EXPECT_TRUE(configurationProvider->sawRegisterHandler);
  EXPECT_TRUE(configurationProvider->sawFetchHandler);
  EXPECT_EQ(workspaceManagerPtr->initializedCalls.load(), 0);

  gatePromise.set_value();
  future.get();

  EXPECT_TRUE(test::wait_until([workspaceManagerPtr]() {
    return workspaceManagerPtr->initializedCalls.load() == 1;
  }));
  EXPECT_EQ(workspaceManagerPtr->initializedCalls.load(), 1);
}

} // namespace
} // namespace pegium::lsp
