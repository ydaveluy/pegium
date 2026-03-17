#pragma once

#include <pegium/services/Services.hpp>
#include <pegium/services/DefaultSharedLspService.hpp>

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {

class DefaultWorkspaceSymbolProvider
    : public services::WorkspaceSymbolProvider,
      protected services::DefaultSharedLspService {
public:
  using services::DefaultSharedLspService::DefaultSharedLspService;

  std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &params,
             const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
