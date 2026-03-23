#pragma once

#include <functional>
#include <string_view>

#include <pegium/lsp/services/ServiceRequirements.hpp>
#include <pegium/core/utils/Disposable.hpp>

namespace lsp {
class Connection;
class MessageHandler;
}

namespace pegium {
struct SharedServices;
}

namespace pegium {

class FileOperationHandler;

/// Registers the workspace-configuration change handler.
void addConfigurationChangeHandler(
    ::lsp::MessageHandler &messageHandler,
    const pegium::SharedServices &sharedServices,
    std::function<void()> ensureInitialized = {});

/// Bridges document diagnostics to the LSP message handler.
void addDiagnosticsHandler(::lsp::MessageHandler &messageHandler,
                           pegium::SharedServices &sharedServices,
                           utils::DisposableStore &disposables);

/// Bridges text-document notifications to the configured update handler.
void addDocumentUpdateHandler(
    ::lsp::MessageHandler &messageHandler,
    pegium::SharedServices &sharedServices,
    std::function<void()> ensureInitialized,
    utils::DisposableStore &disposables);

/// Registers file operation request and notification handlers.
void addFileOperationHandler(::lsp::MessageHandler &messageHandler,
                             FileOperationHandler &fileOperationHandler,
                             const std::function<void()> &ensureInitialized);

/// Runs a complete language-server process from command-line entry parameters.
[[nodiscard]] int runLanguageServerMain(
    int argc, char **argv, std::string_view serverName,
    const std::function<bool(pegium::SharedServices &)> &registerLanguageServices,
    const ServiceRequirements &serviceRequirements = {});

/// Starts the language server on an existing connection.
[[nodiscard]] int startLanguageServer(
    pegium::SharedServices &sharedServices, ::lsp::Connection &connection,
    const ServiceRequirements &serviceRequirements = {});

} // namespace pegium
