#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultDocumentHighlightProvider : protected services::DefaultLanguageService,
                                public services::DocumentHighlightProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::DocumentHighlight>
  getDocumentHighlight(const workspace::Document &document,
                       const ::lsp::DocumentHighlightParams &params,
                       const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
