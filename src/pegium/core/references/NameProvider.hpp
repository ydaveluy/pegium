#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::references {

/// Combined name lookup result returned by `NameProvider::nameOf`.
///
/// `name` is the textual symbol; empty when the node is unnamed. `cstNode`
/// points at the CST source of `name` (typically the `name` assignment) and
/// is invalid when the name was assigned outside the parser (hand-built
/// nodes) — callers should fall back to `node.getCstNode()` in that case.
struct AstNodeName {
  std::string name;
  CstNodeView cstNode;

  [[nodiscard]] bool empty() const noexcept { return name.empty(); }
};

/// Computes the user-visible name of AST nodes that participate in scoping.
class NameProvider {
public:
  virtual ~NameProvider() noexcept = default;

  /// Returns the textual name and its CST source for `node` in a single
  /// lookup. The result is empty when the node has no name. This is the
  /// preferred entry point for indexing/scope-computation paths that need
  /// both pieces of information; the legacy `getName` / `getNameNode`
  /// helpers below delegate to this method.
  [[nodiscard]] virtual AstNodeName nameOf(const AstNode &node) const = 0;

  /// Returns the symbolic name of `node`, or `std::nullopt` when the node
  /// is unnamed. Default implementation delegates to `nameOf`; override only
  /// when a faster name-only path exists.
  [[nodiscard]] virtual std::optional<std::string>
  getName(const AstNode &node) const {
    auto info = nameOf(node);
    if (info.empty()) {
      return std::nullopt;
    }
    return std::move(info.name);
  }

  /// Returns the CST node that carries the `name` assignment of `node`.
  /// Default implementation delegates to `nameOf`.
  [[nodiscard]] virtual std::optional<CstNodeView>
  getNameNode(const AstNode &node) const {
    auto info = nameOf(node);
    if (!info.cstNode.valid()) {
      return std::nullopt;
    }
    return info.cstNode;
  }
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
  auto info = nameProvider.nameOf(node);
  if (info.empty() || !info.cstNode.valid() || !node.hasCstNode()) {
    return std::nullopt;
  }
  return NamedNodeInfo{.name = std::move(info.name),
                       .selectionNode = info.cstNode,
                       .nodeCst = node.getCstNode()};
}

} // namespace pegium::references
