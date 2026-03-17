#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class AbstractTypeDefinitionProvider : protected services::DefaultLanguageService,
                                public services::TypeDefinitionProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &document,
                    const ::lsp::TypeDefinitionParams &params,
                    const utils::CancellationToken &cancelToken) const override;

protected:
  [[nodiscard]] virtual std::optional<std::vector<::lsp::LocationLink>>
  collectGoToTypeLocationLinks(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::lsp
