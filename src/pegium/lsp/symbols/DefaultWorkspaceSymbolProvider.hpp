#pragma once

#include <pegium/lsp/services/Services.hpp>
#include <pegium/lsp/services/DefaultSharedLspService.hpp>

namespace pegium {
struct SharedServices;
}

namespace pegium {

/// Default workspace-symbol provider backed by the shared symbol index.
class DefaultWorkspaceSymbolProvider
    : public ::pegium::WorkspaceSymbolProvider,
      protected DefaultSharedLspService {
public:
  using DefaultSharedLspService::DefaultSharedLspService;

  std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &params,
             const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
