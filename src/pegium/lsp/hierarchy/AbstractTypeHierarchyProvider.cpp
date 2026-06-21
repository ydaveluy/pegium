#include <pegium/lsp/hierarchy/AbstractTypeHierarchyProvider.hpp>
#include <pegium/lsp/hierarchy/HierarchyProviderSupport.hpp>

namespace pegium {

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::prepareTypeHierarchy(
    const workspace::Document &document,
    const ::lsp::TypeHierarchyPrepareParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::prepare<::lsp::TypeHierarchyItem>(
      services, document, params,
      [this](const AstNode &node) { return createTypeHierarchyItem(node); },
      cancelToken);
}

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::supertypes(
    const ::lsp::TypeHierarchySupertypesParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::collect_edges(
      services, params,
      [this](const AstNode &node, const utils::CancellationToken &token) {
        return getSupertypes(node, token);
      },
      cancelToken);
}

std::vector<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::subtypes(
    const ::lsp::TypeHierarchySubtypesParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::collect_edges(
      services, params,
      [this](const AstNode &node, const utils::CancellationToken &token) {
        return getSubtypes(node, token);
      },
      cancelToken);
}

std::optional<::lsp::TypeHierarchyItem>
AbstractTypeHierarchyProvider::createTypeHierarchyItem(
    const AstNode &node) const {
  return hierarchy_detail::create_item<::lsp::TypeHierarchyItem>(
      services, node, ::lsp::SymbolKind::Class,
      [this](const AstNode &n, ::lsp::TypeHierarchyItem &item) {
        customizeTypeHierarchyItem(n, item);
      });
}

} // namespace pegium
