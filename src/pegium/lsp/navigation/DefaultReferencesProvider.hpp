#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default references provider backed by indexed workspace references.
class DefaultReferencesProvider : protected DefaultLanguageService,
                                public ::pegium::ReferencesProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::Location>
  findReferences(const workspace::Document &document,
                 const ::lsp::ReferenceParams &params,
                 const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
