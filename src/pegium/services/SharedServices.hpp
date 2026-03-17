#pragma once

#include <memory>

#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/lsp/ExecuteCommandHandler.hpp>
#include <pegium/lsp/FileOperationHandler.hpp>
#include <pegium/lsp/FuzzyMatcher.hpp>
#include <pegium/lsp/LanguageServer.hpp>
#include <pegium/lsp/NodeKindProvider.hpp>

namespace lsp {
class Connection;
}

namespace pegium::services {
class WorkspaceSymbolProvider;

struct SharedLspServices {
  struct {
    ::lsp::Connection *connection = nullptr;
    std::unique_ptr<pegium::lsp::DocumentUpdateHandler> documentUpdateHandler;
    std::unique_ptr<pegium::lsp::ExecuteCommandHandler> executeCommandHandler;
    std::unique_ptr<pegium::lsp::FileOperationHandler> fileOperationHandler;
    std::unique_ptr<lsp::FuzzyMatcher> fuzzyMatcher;
    std::unique_ptr<pegium::lsp::LanguageServer> languageServer;
    std::unique_ptr<lsp::NodeKindProvider> nodeKindProvider;
    std::unique_ptr<WorkspaceSymbolProvider> workspaceSymbolProvider;
  } lsp;
};

struct SharedServices : SharedCoreServices, SharedLspServices {
  struct NoDefaultsTag {};

  SharedServices();
  explicit SharedServices(NoDefaultsTag);
  SharedServices(SharedServices &&) noexcept;
  SharedServices &operator=(SharedServices &&) noexcept;
  SharedServices(const SharedServices &) = delete;
  SharedServices &operator=(const SharedServices &) = delete;
  ~SharedServices();
};

} // namespace pegium::services
