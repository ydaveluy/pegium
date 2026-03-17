#include <pegium/lsp/AbstractTypeHierarchyProvider.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::prepareTypeHierarchy(
    const workspace::Document &document,
    const ::lsp::TypeHierarchyPrepareParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *references = languageServices.references.references.get();
  if (references == nullptr) {
    return {};
  }
  const auto declaration =
      references->findDeclarationAt(document, document.positionToOffset(params.position));
  if (!declaration.has_value() || declaration->node == nullptr) {
    return {};
  }
  if (auto item = createTypeHierarchyItem(*declaration->node);
      item.has_value()) {
    return std::vector<::lsp::TypeHierarchyItem>{std::move(*item)};
  }
  return {};
}

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::supertypes(
    const ::lsp::TypeHierarchySupertypesParams &params,
  const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *documents = languageServices.sharedServices.workspace.documents.get();
  const auto *references = languageServices.references.references.get();
  if (documents == nullptr || references == nullptr) {
    return {};
  }
  const auto document = documents->getDocument(params.item.uri.toString());
  if (document == nullptr) {
    return {};
  }
  const auto declaration =
      references->findDeclarationAt(*document,
                                    document->positionToOffset(
                                        params.item.selectionRange.start));
  if (!declaration.has_value() || declaration->node == nullptr) {
    return {};
  }
  return getSupertypes(*declaration->node, cancelToken);
}

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::subtypes(
    const ::lsp::TypeHierarchySubtypesParams &params,
  const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *documents = languageServices.sharedServices.workspace.documents.get();
  const auto *references = languageServices.references.references.get();
  if (documents == nullptr || references == nullptr) {
    return {};
  }
  const auto document = documents->getDocument(params.item.uri.toString());
  if (document == nullptr) {
    return {};
  }
  const auto declaration =
      references->findDeclarationAt(*document,
                                    document->positionToOffset(
                                        params.item.selectionRange.start));
  if (!declaration.has_value() || declaration->node == nullptr) {
    return {};
  }
  return getSubtypes(*declaration->node, cancelToken);
}

std::optional<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::createTypeHierarchyItem(const AstNode &node) const {
  const auto *nameProvider = languageServices.references.nameProvider.get();
  const auto *document = tryGetDocument(node);
  if (nameProvider == nullptr || document == nullptr || !node.hasCstNode()) {
    return std::nullopt;
  }

  const auto nameNode = nameProvider->getNameNode(node);
  const auto name = nameProvider->getName(node);
  if (!nameNode || name.empty()) {
    return std::nullopt;
  }

  ::lsp::TypeHierarchyItem item{};
  item.kind = ::lsp::SymbolKind::Class;
  item.name = name;
  item.uri = ::lsp::Uri::parse(document->uri);
  item.range.start = document->offsetToPosition(node.getCstNode().getBegin());
  item.range.end = document->offsetToPosition(node.getCstNode().getEnd());
  item.selectionRange.start = document->offsetToPosition(nameNode.getBegin());
  item.selectionRange.end = document->offsetToPosition(nameNode.getEnd());
  customizeTypeHierarchyItem(node, item);
  return item;
}

} // namespace pegium::lsp
