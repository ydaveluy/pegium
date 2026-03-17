#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class DocumentSymbolProvider {
public:
  virtual ~DocumentSymbolProvider() noexcept = default;
  virtual std::vector<::lsp::DocumentSymbol>
  getSymbols(const workspace::Document &document,
             const ::lsp::DocumentSymbolParams &params,
             const utils::CancellationToken &cancelToken =
                 utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
