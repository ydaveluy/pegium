#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default folding-range provider based on the CST structure.
class DefaultFoldingRangeProvider : protected DefaultLanguageService,
                                public ::pegium::FoldingRangeProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::FoldingRange>
  getFoldingRanges(const workspace::Document &document,
                   const ::lsp::FoldingRangeParams &params,
                   const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
