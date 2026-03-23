#include <pegium/lsp/runtime/LanguageServerRequestHandlerParts.hpp>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/runtime/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

void addLanguageServerLifecycleHandlers(LanguageServerHandlerContext &server,
                                        ::lsp::MessageHandler &handler,
                                        pegium::SharedServices &sharedServices) {
  (void)sharedServices;
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
      [&server](const ::lsp::InitializedParams &params) {
        server.initialized(params);
      });

  handler.add<::lsp::requests::Shutdown>(
      [&server]() {
        server.setShutdownRequested(true);
        return nullptr;
      });

  handler.add<::lsp::notifications::Exit>(
      [&server]() { server.setExitRequested(true); });
}

} // namespace pegium
