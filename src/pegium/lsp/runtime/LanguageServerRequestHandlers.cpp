#include <pegium/lsp/runtime/LanguageServerRequestHandlers.hpp>

#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlerParts.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

void addLanguageServerRequestHandlers(
    LanguageServerHandlerContext &server,
    const ServiceRequirements &serviceRequirements,
    ::lsp::MessageHandler &handler) {
  if (server.handlersRegistered()) {
    return;
  }

  auto &sharedServices = server.sharedServices();
  addLanguageServerLifecycleHandlers(server, handler, sharedServices);
  addLanguageServerWorkspaceHandlers(server, handler, sharedServices,
                                     serviceRequirements);
  addLanguageServerTextDocumentHandlers(server, handler, sharedServices,
                                        serviceRequirements);
  server.setHandlersRegistered(true);
}

} // namespace pegium
