#pragma once

#include <pegium/core/workspace/DocumentUpdate.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium {

/// Publishes one diagnostics snapshot to the connected client.
void publish_diagnostics(
    ::lsp::MessageHandler *messageHandler,
    const workspace::DocumentDiagnosticsSnapshot &snapshot);

} // namespace pegium
