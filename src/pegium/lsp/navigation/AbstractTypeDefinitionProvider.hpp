#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Shared type-definition provider that resolves a symbol then delegates target collection.
class AbstractTypeDefinitionProvider : protected DefaultLanguageService,
                                public ::pegium::TypeDefinitionProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &document,
                    const ::lsp::TypeDefinitionParams &params,
                    const utils::CancellationToken &cancelToken) const override;

protected:
  /// Returns type-definition links for `element`.
  [[nodiscard]] virtual std::optional<std::vector<::lsp::LocationLink>>
  collectGoToTypeLocationLinks(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium
