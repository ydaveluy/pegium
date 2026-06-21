#include <pegium/lsp/hierarchy/AbstractCallHierarchyProvider.hpp>
#include <pegium/lsp/hierarchy/HierarchyProviderSupport.hpp>

namespace pegium {

std::vector<::lsp::CallHierarchyItem>
AbstractCallHierarchyProvider::prepareCallHierarchy(
    const workspace::Document &document,
    const ::lsp::CallHierarchyPrepareParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::prepare<::lsp::CallHierarchyItem>(
      services, document, params,
      [this](const AstNode &node) { return createCallHierarchyItem(node); },
      cancelToken);
}

std::vector<::lsp::CallHierarchyIncomingCall>
AbstractCallHierarchyProvider::incomingCalls(
    const ::lsp::CallHierarchyIncomingCallsParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::collect_edges(
      services, params,
      [this](const AstNode &node, const utils::CancellationToken &token) {
        return getIncomingCalls(node, token);
      },
      cancelToken);
}

std::vector<::lsp::CallHierarchyOutgoingCall>
AbstractCallHierarchyProvider::outgoingCalls(
    const ::lsp::CallHierarchyOutgoingCallsParams &params,
    const utils::CancellationToken &cancelToken) const {
  return hierarchy_detail::collect_edges(
      services, params,
      [this](const AstNode &node, const utils::CancellationToken &token) {
        return getOutgoingCalls(node, token);
      },
      cancelToken);
}

std::optional<::lsp::CallHierarchyItem>
AbstractCallHierarchyProvider::createCallHierarchyItem(
    const AstNode &node) const {
  return hierarchy_detail::create_item<::lsp::CallHierarchyItem>(
      services, node, ::lsp::SymbolKind::Method,
      [this](const AstNode &n, ::lsp::CallHierarchyItem &item) {
        customizeCallHierarchyItem(n, item);
      });
}

} // namespace pegium
