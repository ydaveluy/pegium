#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides same-document symbol highlights around one cursor position.
class DocumentHighlightProvider {
public:
  virtual ~DocumentHighlightProvider() noexcept = default;
  /// Returns the highlights relevant to `params`.
  virtual std::vector<::lsp::DocumentHighlight>
  getDocumentHighlight(const workspace::Document &document,
                       const ::lsp::DocumentHighlightParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token) const = 0;
};

} // namespace pegium
