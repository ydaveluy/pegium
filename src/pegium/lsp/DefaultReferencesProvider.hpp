#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultReferencesProvider : protected services::DefaultLanguageService,
                                public services::ReferencesProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::Location>
  findReferences(const workspace::Document &document,
                 const ::lsp::ReferenceParams &params,
                 const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
