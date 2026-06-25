#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::references {

/// Computes the user-visible name of AST nodes that participate in scoping.
///
/// Split into two independent lookups: `getName` is cheap
/// and reads the name without touching the CST, while `getNameNode` performs
/// the CST lookup and is paid only when the source range is actually needed.
class NameProvider {
public:
  virtual ~NameProvider() noexcept = default;

  /// Returns the textual name of `node`, or `std::nullopt` when the node is
  /// unnamed. Must not require a CST lookup, so name-only callers (indexing
  /// filters, hover labels, tests) stay cheap.
  [[nodiscard]] virtual std::optional<std::string>
  getName(const AstNode &node) const = 0;

  /// Returns the CST node that carries the `name` of `node`, or `std::nullopt`
  /// when the name has no CST source (unnamed node, or a name assigned outside
  /// the parser). Callers needing a declaration range should fall back to
  /// `node.getCstNode()` in the latter case (see `declaration_site_node`).
  [[nodiscard]] virtual std::optional<CstNodeView>
  getNameNode(const AstNode &node) const = 0;
};

/// Reusable naming data for editor-facing features.
struct NamedNodeInfo {
  std::string name;
  CstNodeView selectionNode;
  CstNodeView nodeCst;
};

/// Returns the source range that should represent the declaration site.
///
/// This prefers the explicit `name` node when it exists and falls back to the
/// node CST range otherwise.
[[nodiscard]] inline std::optional<CstNodeView>
declaration_site_node(const AstNode &node, const NameProvider &nameProvider) {
  if (auto nameNode = nameProvider.getNameNode(node); nameNode.has_value()) {
    return nameNode;
  }
  if (!node.hasCstNode()) {
    return std::nullopt;
  }
  return node.getCstNode();
}

/// Returns the declaration site range of a node that is known to be navigable.
[[nodiscard]] inline CstNodeView
required_declaration_site_node(const AstNode &node,
                               const NameProvider &nameProvider) {
  const auto site = declaration_site_node(node, nameProvider);
  assert(site.has_value());
  return *site;
}

/// Returns the user-facing name plus the CST ranges used by editor features.
[[nodiscard]] inline std::optional<NamedNodeInfo>
named_node_info(const AstNode &node, const NameProvider &nameProvider) {
  auto name = nameProvider.getName(node);
  if (!name.has_value() || !node.hasCstNode()) {
    return std::nullopt;
  }
  auto nameNode = nameProvider.getNameNode(node);
  if (!nameNode.has_value()) {
    return std::nullopt;
  }
  return NamedNodeInfo{.name = std::move(*name),
                       .selectionNode = *nameNode,
                       .nodeCst = node.getCstNode()};
}

} // namespace pegium::references
