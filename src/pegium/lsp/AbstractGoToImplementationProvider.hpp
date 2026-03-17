#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class AbstractGoToImplementationProvider : protected services::DefaultLanguageService,
                                public services::ImplementationProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &document,
                    const ::lsp::ImplementationParams &params,
                    const utils::CancellationToken &cancelToken) const override;

protected:
  [[nodiscard]] virtual std::optional<std::vector<::lsp::LocationLink>>
  collectGoToImplementationLocationLinks(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::lsp
