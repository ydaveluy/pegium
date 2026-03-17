#pragma once

#include <string_view>

#include <lsp/types.h>

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/workspace/Symbol.hpp>

namespace pegium::lsp {

class NodeKindProvider {
public:
  virtual ~NodeKindProvider() noexcept = default;

  [[nodiscard]] virtual ::lsp::SymbolKindEnum
  getSymbolKind(const pegium::AstNode &node) const = 0;

  [[nodiscard]] virtual ::lsp::SymbolKindEnum
  getSymbolKind(const workspace::AstNodeDescription &description) const = 0;

  [[nodiscard]] virtual ::lsp::CompletionItemKindEnum
  getCompletionItemKind(const pegium::AstNode &node) const = 0;

  [[nodiscard]] virtual ::lsp::CompletionItemKindEnum
  getCompletionItemKind(const workspace::AstNodeDescription &description) const = 0;
};

} // namespace pegium::lsp
