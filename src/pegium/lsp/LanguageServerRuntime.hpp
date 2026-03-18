#pragma once

#include <functional>

#include <pegium/lsp/ServiceRequirements.hpp>
#include <pegium/utils/Disposable.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {

class FileOperationHandler;

void addConfigurationChangeHandler(
    ::lsp::MessageHandler &messageHandler,
    const services::SharedServices &sharedServices,
    std::function<void()> ensureInitialized = {});

void addDiagnosticsHandler(::lsp::MessageHandler &messageHandler,
                           services::SharedServices &sharedServices,
                           utils::DisposableStore &disposables);

void addDocumentUpdateHandler(
    ::lsp::MessageHandler &messageHandler,
    services::SharedServices &sharedServices,
    std::function<void()> ensureInitialized,
    utils::DisposableStore &disposables);

void addFileOperationHandler(::lsp::MessageHandler &messageHandler,
                             FileOperationHandler &fileOperationHandler,
                             const std::function<void()> &ensureInitialized);

[[nodiscard]] int startLanguageServer(
    services::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements = {});

} // namespace pegium::lsp
