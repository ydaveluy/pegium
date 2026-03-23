#include <gtest/gtest.h>

#include <atomic>
#include <future>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlerParts.hpp>
#include <pegium/lsp/runtime/LanguageServerRuntimeState.hpp>
#include <pegium/lsp/runtime/internal/LanguageClientFactory.hpp>
#include <pegium/core/workspace/Configuration.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>

namespace pegium {
namespace {

std::shared_future<void> make_ready_shared_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future().share();
}

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
      : gate(std::move(gate)), readyFuture(make_ready_shared_future()) {}

  workspace::BuildOptions &initialBuildOptions() override { return options; }

  const workspace::BuildOptions &initialBuildOptions() const override {
    return options;
  }

  std::shared_future<void> ready() const override { return readyFuture; }

  std::optional<std::vector<workspace::WorkspaceFolder>>
  workspaceFolders() const override {
    return std::vector<workspace::WorkspaceFolder>{};
  }

  void initialize(const workspace::InitializeParams &params) override {
    (void)params;
  }

  std::future<void>
  initialized(const workspace::InitializedParams &params) override {
    (void)params;
    ++initializedCalls;
    auto waitGate = gate;
    return std::async(std::launch::async,
                      [waitGate]() mutable { waitGate.wait(); });
  }

  void initializeWorkspace(
      std::span<const workspace::WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) override {
    (void)workspaceFolders;
    utils::throw_if_cancelled(cancelToken);
  }

  std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const override {
    (void)workspaceUri;
    return {};
  }

  bool shouldIncludeEntry(const workspace::FileSystemNode &entry) const override {
    (void)entry;
    return true;
  }

  std::shared_future<void> gate;
  std::shared_future<void> readyFuture;
  workspace::BuildOptions options{};
  std::atomic<int> initializedCalls = 0;
};

TEST(LanguageServerLifecycleHandlersTest,
     InitializedNotificationDoesNotBlockOnBackgroundInitializationFutures) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);

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
  shared->lsp.languageClient =
      make_message_handler_language_client(handler, *shared->observabilitySink);
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
  EXPECT_EQ(workspaceManagerPtr->initializedCalls.load(), 1);

  gatePromise.set_value();
  future.get();

  EXPECT_TRUE(test::wait_until([workspaceManagerPtr]() {
    return workspaceManagerPtr->initializedCalls.load() == 1;
  }));
  EXPECT_EQ(workspaceManagerPtr->initializedCalls.load(), 1);
  shared->lsp.languageClient.reset();
}

} // namespace
} // namespace pegium
