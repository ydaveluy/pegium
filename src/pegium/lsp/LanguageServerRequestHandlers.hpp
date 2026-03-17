#pragma once

#include <pegium/lsp/ServiceRequirements.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium::lsp {

class LanguageServerHandlerContext;

void addLanguageServerRequestHandlers(LanguageServerHandlerContext &context,
                                      const ServiceRequirements &serviceRequirements,
                                      ::lsp::MessageHandler &messageHandler);

} // namespace pegium::lsp
