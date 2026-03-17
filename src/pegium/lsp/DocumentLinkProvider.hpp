#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class DocumentLinkProvider {
public:
  virtual ~DocumentLinkProvider() noexcept = default;
  virtual std::vector<::lsp::DocumentLink>
  getDocumentLinks(const workspace::Document &document,
                   const ::lsp::DocumentLinkParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
