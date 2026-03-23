#pragma once

#include <pegium/lsp/services/DefaultSharedLspService.hpp>
#include <pegium/lsp/symbols/NodeKindProvider.hpp>

namespace pegium {

/// Default node-kind provider mapping AST kinds to LSP symbol and completion kinds.
class DefaultNodeKindProvider : public NodeKindProvider,
                                protected DefaultSharedLspService {
public:
  using DefaultSharedLspService::DefaultSharedLspService;

  [[nodiscard]] ::lsp::SymbolKindEnum
  getSymbolKind(const pegium::AstNode &node) const override;

  [[nodiscard]] ::lsp::SymbolKindEnum
  getSymbolKind(const workspace::AstNodeDescription &description) const override;

  [[nodiscard]] ::lsp::CompletionItemKindEnum
  getCompletionItemKind(const pegium::AstNode &node) const override;

  [[nodiscard]] ::lsp::CompletionItemKindEnum
  getCompletionItemKind(
      const workspace::AstNodeDescription &description) const override;
};

} // namespace pegium
