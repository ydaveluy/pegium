#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default document-highlight provider backed by references and declarations.
class DefaultDocumentHighlightProvider : protected DefaultLanguageService,
                                public ::pegium::DocumentHighlightProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::DocumentHighlight>
  getDocumentHighlight(const workspace::Document &document,
                       const ::lsp::DocumentHighlightParams &params,
                       const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
