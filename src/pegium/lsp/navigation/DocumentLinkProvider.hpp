#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides document links for one document.
class DocumentLinkProvider {
public:
  virtual ~DocumentLinkProvider() noexcept = default;
  /// Returns the links found in `document`.
  virtual std::vector<::lsp::DocumentLink>
  getDocumentLinks(const workspace::Document &document,
                   const ::lsp::DocumentLinkParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token) const = 0;
};

} // namespace pegium
