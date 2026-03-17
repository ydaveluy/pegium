#pragma once

#include <pegium/lsp/ServiceRequirements.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {

class LanguageServerHandlerContext;

void addLanguageServerLifecycleHandlers(LanguageServerHandlerContext &server,
                                        ::lsp::MessageHandler &handler,
                                        services::SharedServices &sharedServices);

void addLanguageServerWorkspaceHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    services::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements);

void addLanguageServerTextDocumentHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    services::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements);

} // namespace pegium::lsp
