#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultDocumentSymbolProvider : protected services::DefaultLanguageService,
                                public services::DocumentSymbolProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::DocumentSymbol>
  getSymbols(const workspace::Document &document,
             const ::lsp::DocumentSymbolParams &params,
             const utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] std::vector<::lsp::DocumentSymbol>
  getSymbolTree(const AstNode &node, const workspace::Document &document,
                const utils::CancellationToken &cancelToken) const;

  [[nodiscard]] std::optional<::lsp::DocumentSymbol>
  createSymbol(const AstNode &node, const workspace::Document &document,
               const utils::CancellationToken &cancelToken) const;
};

} // namespace pegium::lsp
