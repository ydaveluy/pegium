#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultFoldingRangeProvider : protected services::DefaultLanguageService,
                                public services::FoldingRangeProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::FoldingRange>
  getFoldingRanges(const workspace::Document &document,
                   const ::lsp::FoldingRangeParams &params,
                   const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
