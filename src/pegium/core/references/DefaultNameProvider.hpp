#pragma once

#include <pegium/core/references/NameProvider.hpp>

namespace pegium::references {

/// Default name provider that prefers `pegium::NamedAstNode::name` and falls
/// back to the grammar assignment named `name`.
class DefaultNameProvider : public NameProvider {
public:
  /// Returns the textual name from the AST when the node derives from
  /// `pegium::NamedAstNode`, otherwise reads the `name` feature assignment.
  [[nodiscard]] std::optional<std::string>
  getName(const AstNode &node) const noexcept override;

  /// Returns the direct CST node of the `name` assignment, when present.
  [[nodiscard]] std::optional<CstNodeView>
  getNameNode(const AstNode &node) const noexcept override;
};

} // namespace pegium::references
