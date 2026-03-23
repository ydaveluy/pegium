#include <pegium/core/references/DefaultScopeComputation.hpp>

#include <cassert>
#include <string>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::references {

std::vector<workspace::AstNodeDescription>
DefaultScopeComputation::collectExportedSymbols(
    const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  if (!document.hasAst()) {
    return {};
  }

  return collectExportedSymbolsForNode(*document.parseResult.value, document,
                                       cancelToken);
}

workspace::LocalSymbols
DefaultScopeComputation::collectLocalSymbols(
    const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  workspace::LocalSymbols symbols;
  if (!document.hasAst()) {
    return symbols;
  }

  collectLocalSymbolsForNode(*document.parseResult.value, document, symbols,
                             cancelToken);
  return symbols;
}

std::vector<workspace::AstNodeDescription>
DefaultScopeComputation::collectExportedSymbolsForNode(
    const AstNode &parentNode, const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  std::vector<workspace::AstNodeDescription> exports;
  addExportedSymbol(parentNode, exports, document);
  for (const auto *node : parentNode.getContent()) {
    utils::throw_if_cancelled(cancelToken);
    addExportedSymbol(*node, exports, document);
  }
  return exports;
}

void DefaultScopeComputation::collectLocalSymbolsForNode(
    const AstNode &rootNode, const workspace::Document &document,
    workspace::LocalSymbols &symbols,
    const utils::CancellationToken &cancelToken) const {
  for (const auto *node : rootNode.getAllContent()) {
    utils::throw_if_cancelled(cancelToken);
    addLocalSymbol(*node, document, symbols);
  }
}

void DefaultScopeComputation::addExportedSymbol(
    const AstNode &node, std::vector<workspace::AstNodeDescription> &exports,
    const workspace::Document &document) const {
  auto name = services.references.nameProvider->getName(node);
  if (!name.has_value()) {
    return;
  }
  if (auto description =
          services.workspace.astNodeDescriptionProvider->createDescription(
              node, document, std::move(*name));
      description.has_value()) {
    exports.push_back(std::move(*description));
  }
}

void DefaultScopeComputation::addLocalSymbol(
    const AstNode &node, const workspace::Document &document,
    workspace::LocalSymbols &symbols) const {
  auto *container = node.getContainer();
  assert(container != nullptr);

  auto name = services.references.nameProvider->getName(node);
  if (!name.has_value()) {
    return;
  }
  if (auto description =
          services.workspace.astNodeDescriptionProvider->createDescription(
              node, document, std::move(*name));
      description.has_value()) {
    symbols.emplace(container, std::move(*description));
  }
}

} // namespace pegium::references
