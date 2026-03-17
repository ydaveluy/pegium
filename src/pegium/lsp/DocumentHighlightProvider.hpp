#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class DocumentHighlightProvider {
public:
  virtual ~DocumentHighlightProvider() noexcept = default;
  virtual std::vector<::lsp::DocumentHighlight>
  getDocumentHighlight(const workspace::Document &document,
                       const ::lsp::DocumentHighlightParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
