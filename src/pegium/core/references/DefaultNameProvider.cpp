#include <pegium/core/references/DefaultNameProvider.hpp>

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

namespace pegium::references {

std::optional<std::string>
DefaultNameProvider::getName(const AstNode &node) const {
  // Only `NamedAstNode`-derived types are named by the default provider: it
  // reads `NamedAstNode::name` directly. A language whose named type does not
  // derive from `NamedAstNode` must override the `NameProvider` service.
  //
  // Resolving the type (O(1) reflection isSubtype via `ast_ptr_cast`, with a
  // dynamic_cast fallback for standalone nodes) lets unnamed nodes — the bulk
  // of an AST — return immediately, and never touches the CST: name-only
  // callers pay no `find_node_for_feature` scan.
  const auto *namedNode = ast_ptr_cast<const NamedAstNode>(&node);
  if (namedNode == nullptr || namedNode->name.empty()) {
    return std::nullopt;
  }
  return namedNode->name;
}

std::optional<CstNodeView>
DefaultNameProvider::getNameNode(const AstNode &node) const {
  // The `name` assignment's CST source. Nodes named outside the parser have
  // none (callers fall back to the node's own range). The CST lookup is
  // performed lazily here, so it is only paid when a caller actually needs
  // the declaration range.
  if (!node.hasCstNode()) {
    return std::nullopt;
  }
  return find_node_for_feature(node.getCstNode(), "name");
}

} // namespace pegium::references
