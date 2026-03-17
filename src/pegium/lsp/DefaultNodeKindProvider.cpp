#include <pegium/lsp/DefaultNodeKindProvider.hpp>

namespace pegium::lsp {

::lsp::SymbolKindEnum
DefaultNodeKindProvider::getSymbolKind(const pegium::AstNode &) const {
  return ::lsp::SymbolKind::Field;
}

::lsp::SymbolKindEnum DefaultNodeKindProvider::getSymbolKind(
    const workspace::AstNodeDescription &) const {
  return ::lsp::SymbolKind::Field;
}

::lsp::CompletionItemKindEnum
DefaultNodeKindProvider::getCompletionItemKind(const pegium::AstNode &) const {
  return ::lsp::CompletionItemKind::Reference;
}

::lsp::CompletionItemKindEnum DefaultNodeKindProvider::getCompletionItemKind(
    const workspace::AstNodeDescription &) const {
  return ::lsp::CompletionItemKind::Reference;
}

} // namespace pegium::lsp
