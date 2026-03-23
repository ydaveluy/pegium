#pragma once

#include <string>
#include <string_view>

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::workspace {

/// Converts AST nodes to stable string paths and back.
class AstNodeLocator {
public:
  virtual ~AstNodeLocator() noexcept = default;

  /// Returns the path of `node` relative to its AST root.
  [[nodiscard]] virtual std::string getAstNodePath(const AstNode &node) const = 0;

  /// Resolves `path` from an immutable AST root.
  [[nodiscard]] virtual const AstNode *getAstNode(const AstNode &root,
                                                  std::string_view path) const = 0;
  /// Resolves `path` from a mutable AST root.
  [[nodiscard]] virtual AstNode *getAstNode(AstNode &root,
                                            std::string_view path) const = 0;
};

} // namespace pegium::workspace
