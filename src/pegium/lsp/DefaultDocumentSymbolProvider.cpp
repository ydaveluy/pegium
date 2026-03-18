#include <pegium/lsp/DefaultDocumentSymbolProvider.hpp>

#include <memory>
#include <utility>

#include <pegium/references/NameProvider.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

std::vector<::lsp::DocumentSymbol>
DefaultDocumentSymbolProvider::getSymbols(
    const workspace::Document &document,
    const ::lsp::DocumentSymbolParams &params,
    const utils::CancellationToken &cancelToken) const {
  (void)params;
  utils::throw_if_cancelled(cancelToken);
  if (const auto *nameProvider = languageServices.references.nameProvider.get();
      document.parseResult.value == nullptr || nameProvider == nullptr) {
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
    if (child == nullptr) {
      continue;
    }
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

  const auto *nameProvider = languageServices.references.nameProvider.get();
  if (nameProvider == nullptr || !node.hasCstNode()) {
    return std::nullopt;
  }

  const auto nameNode = nameProvider->getNameNode(node);
  if (!nameNode) {
    return std::nullopt;
  }

  ::lsp::DocumentSymbol symbol{};
  symbol.name = nameProvider->getName(node);
  if (symbol.name.empty()) {
    symbol.name = std::string(nameNode.getText());
  }

  if (languageServices.sharedServices.lsp.nodeKindProvider != nullptr) {
    symbol.kind =
        languageServices.sharedServices.lsp.nodeKindProvider->getSymbolKind(node);
  } else {
    symbol.kind = ::lsp::SymbolKind::Field;
  }

  symbol.range.start = document.offsetToPosition(node.getCstNode().getBegin());
  symbol.range.end = document.offsetToPosition(node.getCstNode().getEnd());
  symbol.selectionRange.start = document.offsetToPosition(nameNode.getBegin());
  symbol.selectionRange.end = document.offsetToPosition(nameNode.getEnd());

  for (const auto *child : node.getContent()) {
    utils::throw_if_cancelled(cancelToken);
    if (child == nullptr) {
      continue;
    }
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

} // namespace pegium::lsp
