#pragma once

#include <functional>

#include <lsp/messagehandler.h>

#include <pegium/workspace/TextDocuments.hpp>

namespace pegium::lsp {

void addTextDocumentHandlers(::lsp::MessageHandler &messageHandler,
                             workspace::TextDocuments &documents,
                             std::function<void()> ensureInitialized = {});

} // namespace pegium::lsp
