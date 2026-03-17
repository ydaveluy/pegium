#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultDefinitionProvider : protected services::DefaultLanguageService,
                                public services::DefinitionProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getDefinition(const workspace::Document &document,
                const ::lsp::DefinitionParams &params,
                const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
