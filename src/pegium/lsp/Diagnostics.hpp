#pragma once

#include <pegium/workspace/DocumentUpdate.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium::lsp {

void publish_diagnostics(
    ::lsp::MessageHandler *messageHandler,
    const workspace::DocumentDiagnosticsSnapshot &snapshot);

} // namespace pegium::lsp
