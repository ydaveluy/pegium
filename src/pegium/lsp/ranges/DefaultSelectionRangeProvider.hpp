#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default selection-range provider based on nested CST containment.
class DefaultSelectionRangeProvider : protected DefaultLanguageService,
                                public ::pegium::SelectionRangeProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &document,
                     const ::lsp::SelectionRangeParams &params,
                     const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
