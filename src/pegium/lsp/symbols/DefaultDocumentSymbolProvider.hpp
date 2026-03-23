#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default document-symbol provider built from AST names and node kinds.
class DefaultDocumentSymbolProvider : protected DefaultLanguageService,
                                public ::pegium::DocumentSymbolProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

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

} // namespace pegium
