#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class MultilineCommentHoverProvider : protected services::DefaultLanguageService,
                                public services::HoverProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::Hover>
  getHoverContent(const workspace::Document &document,
                  const ::lsp::HoverParams &params,
                  const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
