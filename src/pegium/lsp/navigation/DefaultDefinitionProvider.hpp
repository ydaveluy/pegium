#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default definition provider backed by core references and CST lookups.
class DefaultDefinitionProvider : protected DefaultLanguageService,
                                public ::pegium::DefinitionProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getDefinition(const workspace::Document &document,
                const ::lsp::DefinitionParams &params,
                const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
