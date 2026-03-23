#include <pegium/lsp/symbols/DefaultDocumentSymbolProvider.hpp>

#include <cassert>
#include <memory>
#include <utility>

#include <pegium/core/references/NameProvider.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

std::vector<::lsp::DocumentSymbol>
DefaultDocumentSymbolProvider::getSymbols(
    const workspace::Document &document,
    const ::lsp::DocumentSymbolParams &params,
    const utils::CancellationToken &cancelToken) const {
  (void)params;
  utils::throw_if_cancelled(cancelToken);
  if (!document.hasAst()) {
    return {};
  }
  return getSymbolTree(*document.parseResult.value, document, cancelToken);
}

std::vector<::lsp::DocumentSymbol> DefaultDocumentSymbolProvider::getSymbolTree(
    const AstNode &node, const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  std::vector<::lsp::DocumentSymbol> symbols;
  if (auto symbol = createSymbol(node, document, cancelToken); symbol.has_value()) {
    symbols.push_back(std::move(*symbol));
    return symbols;
  }

  for (const auto *child : node.getContent()) {
    utils::throw_if_cancelled(cancelToken);
    auto childSymbols = getSymbolTree(*child, document, cancelToken);
    symbols.insert(symbols.end(),
                   std::make_move_iterator(childSymbols.begin()),
                   std::make_move_iterator(childSymbols.end()));
  }
  return symbols;
}

std::optional<::lsp::DocumentSymbol> DefaultDocumentSymbolProvider::createSymbol(
    const AstNode &node, const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  const auto &nameProvider = *services.references.nameProvider;
  const auto &nodeKindProvider = *services.shared.lsp.nodeKindProvider;
  auto namedNode = references::named_node_info(node, nameProvider);
  if (!namedNode.has_value()) {
    return std::nullopt;
  }

  ::lsp::DocumentSymbol symbol{};
  symbol.name = std::move(namedNode->name);
  symbol.kind = nodeKindProvider.getSymbolKind(node);
  const auto &textDocument = document.textDocument();

  symbol.range.start = textDocument.positionAt(namedNode->nodeCst.getBegin());
  symbol.range.end = textDocument.positionAt(namedNode->nodeCst.getEnd());
  symbol.selectionRange.start =
      textDocument.positionAt(namedNode->selectionNode.getBegin());
  symbol.selectionRange.end =
      textDocument.positionAt(namedNode->selectionNode.getEnd());

  for (const auto *child : node.getContent()) {
    utils::throw_if_cancelled(cancelToken);
    auto childSymbols = getSymbolTree(*child, document, cancelToken);
    if (!childSymbols.empty()) {
      if (!symbol.children.has_value()) {
        symbol.children = ::lsp::Array<::lsp::DocumentSymbol>{};
      }
      auto &children = *symbol.children;
      children.insert(children.end(),
                      std::make_move_iterator(childSymbols.begin()),
                      std::make_move_iterator(childSymbols.end()));
    }
  }

  return symbol;
}

} // namespace pegium
