#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Shared implementation provider that resolves a symbol then delegates link collection.
class AbstractGoToImplementationProvider : protected DefaultLanguageService,
                                public ::pegium::ImplementationProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &document,
                    const ::lsp::ImplementationParams &params,
                    const utils::CancellationToken &cancelToken) const override;

protected:
  /// Returns location links for concrete implementations of `element`.
  [[nodiscard]] virtual std::optional<std::vector<::lsp::LocationLink>>
  collectGoToImplementationLocationLinks(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium
