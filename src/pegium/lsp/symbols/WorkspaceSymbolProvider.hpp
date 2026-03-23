#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium {

/// Provides workspace-wide symbol search and optional symbol resolution.
class WorkspaceSymbolProvider {
public:
  virtual ~WorkspaceSymbolProvider() noexcept = default;
  /// Returns the symbols matching `params.query`.
  virtual std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &params,
             const utils::CancellationToken &cancelToken =
                 utils::default_cancel_token) const = 0;

  /// Returns whether `resolveSymbol(...)` is implemented.
  [[nodiscard]] virtual bool supportsResolveSymbol() const noexcept {
    return false;
  }

  /// Returns a richer symbol payload for a previously returned workspace symbol.
  virtual std::optional<::lsp::WorkspaceSymbol>
  resolveSymbol(const ::lsp::WorkspaceSymbol &symbol,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const {
    (void)symbol;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium
