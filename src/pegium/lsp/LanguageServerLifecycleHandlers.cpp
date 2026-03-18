#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>

#include <future>
#include <thread>
#include <utility>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/lsp/WorkspaceAdapters.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

namespace {

void consume_background_future(std::future<void> future) {
  if (!future.valid()) {
    return;
  }
  std::thread([future = std::move(future)]() mutable {
    try {
      future.get();
    } catch (...) {
    }
  }).detach();
}

} // namespace

void addLanguageServerLifecycleHandlers(LanguageServerHandlerContext &server,
                                        ::lsp::MessageHandler &handler,
                                        services::SharedServices &sharedServices) {
  handler.add<::lsp::notifications::CancelRequest>(
      [&server](const ::lsp::CancelParams &params) {
        server.cancelRequestByKey(request_key_from_cancel_id(params.id));
      });

  handler.add<::lsp::requests::Initialize>(
      [&server](const ::lsp::InitializeParams &params) {
        auto result = server.initialize(params);
        server.setInitialized(true);
        return result;
      });

  handler.add<::lsp::notifications::Initialized>(
      [&server, &handler, &sharedServices](const ::lsp::InitializedParams &params) {
        server.initialized(params);
        const auto initializedParams =
            make_workspace_initialized_params(&handler);
        consume_background_future(std::async(
            std::launch::async,
            [&sharedServices, initializedParams]() mutable {
              if (sharedServices.workspace.configurationProvider != nullptr) {
                auto future = sharedServices.workspace.configurationProvider
                                  ->initialized(initializedParams);
                if (future.valid()) {
                  try {
                    future.get();
                  } catch (...) {
                  }
                }
              }
              if (sharedServices.workspace.workspaceManager != nullptr) {
                auto future = sharedServices.workspace.workspaceManager
                                  ->initialized(initializedParams);
                if (future.valid()) {
                  future.get();
                }
              }
            }));
      });

  handler.add<::lsp::requests::Shutdown>(
      [&server]() {
        server.setShutdownRequested(true);
        return nullptr;
      });

  handler.add<::lsp::notifications::Exit>(
      [&server]() { server.setExitRequested(true); });
}

} // namespace pegium::lsp
