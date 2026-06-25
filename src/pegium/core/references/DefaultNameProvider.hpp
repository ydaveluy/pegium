#pragma once

#include <pegium/core/references/NameProvider.hpp>

namespace pegium::references {

/// Default name provider: names only `pegium::NamedAstNode`-derived nodes,
/// reading `NamedAstNode::name` directly. A language whose named type does not
/// derive from `NamedAstNode` must override the `NameProvider` service.
class DefaultNameProvider : public NameProvider {
public:
  /// Returns `NamedAstNode::name`, or `std::nullopt` for any node that does not
  /// derive from `NamedAstNode` or whose name is empty. No CST lookup.
  [[nodiscard]] std::optional<std::string>
  getName(const AstNode &node) const override;

  /// Returns the CST node of the `name` assignment, or `std::nullopt` when the
  /// node has no CST or no `name` feature.
  [[nodiscard]] std::optional<CstNodeView>
  getNameNode(const AstNode &node) const override;
};

} // namespace pegium::references
