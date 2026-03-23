#pragma once

#include <pegium/lsp/services/ServiceRequirements.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium {
struct SharedServices;
}

namespace pegium {

class LanguageServerHandlerContext;

/// Registers lifecycle request and notification handlers on `handler`.
void addLanguageServerLifecycleHandlers(LanguageServerHandlerContext &server,
                                        ::lsp::MessageHandler &handler,
                                        pegium::SharedServices &sharedServices);

/// Registers workspace-related request and notification handlers on `handler`.
void addLanguageServerWorkspaceHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    pegium::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements);

/// Registers text-document request and notification handlers on `handler`.
void addLanguageServerTextDocumentHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    pegium::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements);

} // namespace pegium
