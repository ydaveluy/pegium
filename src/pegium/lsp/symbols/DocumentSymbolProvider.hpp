#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides document symbols for one document.
class DocumentSymbolProvider {
public:
  virtual ~DocumentSymbolProvider() noexcept = default;
  /// Returns the symbol tree to expose for `document`.
  virtual std::vector<::lsp::DocumentSymbol>
  getSymbols(const workspace::Document &document,
             const ::lsp::DocumentSymbolParams &params,
             const utils::CancellationToken &cancelToken =
                 utils::default_cancel_token) const = 0;
};

} // namespace pegium
