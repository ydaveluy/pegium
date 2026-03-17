#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>

namespace pegium::services {

class WorkspaceSymbolProvider {
public:
  virtual ~WorkspaceSymbolProvider() noexcept = default;
  virtual std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &params,
             const utils::CancellationToken &cancelToken =
                 utils::default_cancel_token) const = 0;

  [[nodiscard]] virtual bool supportsResolveSymbol() const noexcept {
    return false;
  }

  virtual std::optional<::lsp::WorkspaceSymbol>
  resolveSymbol(const ::lsp::WorkspaceSymbol &symbol,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const {
    (void)symbol;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium::services
