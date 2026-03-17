#include <pegium/lsp/LanguageServerRequestHandlers.hpp>

#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

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

} // namespace pegium::lsp
