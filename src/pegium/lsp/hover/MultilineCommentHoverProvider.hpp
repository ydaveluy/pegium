#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Hover provider that renders documentation from preceding multiline comments.
class MultilineCommentHoverProvider : protected DefaultLanguageService,
                                public ::pegium::HoverProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::Hover>
  getHoverContent(const workspace::Document &document,
                  const ::lsp::HoverParams &params,
                  const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
