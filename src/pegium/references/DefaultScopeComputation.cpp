#include <pegium/references/DefaultScopeComputation.hpp>

#include <utility>

#include <pegium/services/CoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::references {

std::vector<workspace::AstNodeDescription>
DefaultScopeComputation::collectExportedSymbols(
    const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  std::vector<workspace::AstNodeDescription> exports;
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  if (!document.hasAst() || document.parseResult.value == nullptr ||
      descriptionProvider == nullptr) {
    return exports;
  }

  auto add_exported = [descriptionProvider, &document,
                       &exports](const AstNode &node) {
    auto description = descriptionProvider->createDescription(node, document);
    if (!description.has_value()) {
      return;
    }
    exports.push_back(std::move(*description));
  };

  add_exported(*document.parseResult.value);
  for (const auto *node : document.parseResult.value->getContent()) {
    utils::throw_if_cancelled(cancelToken);
    if (node == nullptr) {
      continue;
    }
    add_exported(*node);
  }

  return exports;
}

workspace::LocalSymbols
DefaultScopeComputation::collectLocalSymbols(
    const workspace::Document &document,
    const utils::CancellationToken &cancelToken) const {
  workspace::LocalSymbols symbols;
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  if (!document.hasAst() || document.parseResult.value == nullptr ||
      descriptionProvider == nullptr) {
    return symbols;
  }

  for (const auto *node : document.parseResult.value->getAllContent()) {
    utils::throw_if_cancelled(cancelToken);
    if (node == nullptr) {
      continue;
    }

    auto *container = node->getContainer();
    if (container == nullptr) {
      continue;
    }

    auto description = descriptionProvider->createDescription(*node, document);
    if (!description.has_value()) {
      continue;
    }

    symbols.emplace(container, std::move(*description));
  }

  return symbols;
}

} // namespace pegium::references
