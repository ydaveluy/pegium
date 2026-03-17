#pragma once

#include <pegium/services/DefaultSharedLspService.hpp>
#include <pegium/lsp/NodeKindProvider.hpp>

namespace pegium::lsp {

class DefaultNodeKindProvider : public NodeKindProvider,
                                protected services::DefaultSharedLspService {
public:
  using services::DefaultSharedLspService::DefaultSharedLspService;

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

} // namespace pegium::lsp
