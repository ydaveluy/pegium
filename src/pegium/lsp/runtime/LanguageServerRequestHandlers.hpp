#pragma once

#include <pegium/lsp/services/ServiceRequirements.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium {

class LanguageServerHandlerContext;

/// Registers every standard LSP request handler on `messageHandler`.
void addLanguageServerRequestHandlers(LanguageServerHandlerContext &context,
                                      const ServiceRequirements &serviceRequirements,
                                      ::lsp::MessageHandler &messageHandler);

} // namespace pegium
