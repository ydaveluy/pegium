#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultSelectionRangeProvider : protected services::DefaultLanguageService,
                                public services::SelectionRangeProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &document,
                     const ::lsp::SelectionRangeParams &params,
                     const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
