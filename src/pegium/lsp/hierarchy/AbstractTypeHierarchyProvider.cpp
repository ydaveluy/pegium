#include <pegium/lsp/hierarchy/AbstractTypeHierarchyProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>
#include <pegium/lsp/support/JsonValue.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

#include <cassert>
#include <limits>

namespace pegium {

namespace {

std::optional<workspace::SymbolId>
symbol_id_from_item(const ::lsp::TypeHierarchyItem &item) {
  if (!item.data.has_value()) {
    return std::nullopt;
  }
  const auto data = from_lsp_any(*item.data);
  if (!data.isInteger()) {
    return std::nullopt;
  }
  const auto value = data.integer();
  if (value < 0 ||
      value > static_cast<std::int64_t>(
                  std::numeric_limits<workspace::SymbolId>::max())) {
    return std::nullopt;
  }
  return static_cast<workspace::SymbolId>(value);
}

const AstNode *resolve_type_hierarchy_target(
    const workspace::Document &document, const ::lsp::TypeHierarchyItem &item,
    const references::References &references) {
  if (const auto symbolId = symbol_id_from_item(item); symbolId.has_value()) {
    if (const auto *node = document.findAstNode(*symbolId); node != nullptr) {
      return node;
    }
  }
  const auto &textDocument = document.textDocument();
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, textDocument.offsetAt(item.selectionRange.start),
      references);
  return declarations.empty() ? nullptr : declarations.front();
}

} // namespace

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::prepareTypeHierarchy(
    const workspace::Document &document,
    const ::lsp::TypeHierarchyPrepareParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &references = *services.references.references;
  const auto &textDocument = document.textDocument();
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, textDocument.offsetAt(params.position), references);
  if (declarations.empty()) {
    return {};
  }
  if (auto item = createTypeHierarchyItem(*declarations.front());
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
  const auto &documents = *services.shared.workspace.documents;
  const auto &references = *services.references.references;
  const auto document = documents.getDocument(params.item.uri.toString());
  assert(document != nullptr);
  if (const auto *node =
          resolve_type_hierarchy_target(*document, params.item, references);
      node != nullptr) {
    return getSupertypes(*node, cancelToken);
  }
  return {};
}

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::subtypes(
    const ::lsp::TypeHierarchySubtypesParams &params,
  const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &documents = *services.shared.workspace.documents;
  const auto &references = *services.references.references;
  const auto document = documents.getDocument(params.item.uri.toString());
  assert(document != nullptr);
  if (const auto *node =
          resolve_type_hierarchy_target(*document, params.item, references);
      node != nullptr) {
    return getSubtypes(*node, cancelToken);
  }
  return {};
}

std::optional<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::createTypeHierarchyItem(const AstNode &node) const {
  const auto *nameProvider = services.references.nameProvider.get();
  const auto &document = getDocument(node);

  auto namedNode = references::named_node_info(node, *nameProvider);
  if (!namedNode.has_value()) {
    return std::nullopt;
  }

  ::lsp::TypeHierarchyItem item{};
  item.kind = ::lsp::SymbolKind::Class;
  item.name = std::move(namedNode->name);
  item.uri = ::lsp::Uri::parse(document.uri);
  const auto &textDocument = document.textDocument();
  item.range.start = textDocument.positionAt(namedNode->nodeCst.getBegin());
  item.range.end = textDocument.positionAt(namedNode->nodeCst.getEnd());
  item.selectionRange.start =
      textDocument.positionAt(namedNode->selectionNode.getBegin());
  item.selectionRange.end =
      textDocument.positionAt(namedNode->selectionNode.getEnd());
  item.data = to_lsp_any(services::JsonValue(
      static_cast<std::int64_t>(document.makeSymbolId(node))));
  customizeTypeHierarchyItem(node, item);
  return item;
}

} // namespace pegium
