#pragma once

#include <memory>

#include <lsp/messagehandler.h>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/lsp/runtime/LanguageClient.hpp>

namespace pegium {

/// Creates a language client that emits notifications through `messageHandler`.
[[nodiscard]] std::unique_ptr<LanguageClient> make_message_handler_language_client(
    ::lsp::MessageHandler &messageHandler,
    observability::ObservabilitySink &observabilitySink);

} // namespace pegium
