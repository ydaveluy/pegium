#pragma once

#include <pegium/core/workspace/AstNodeLocator.hpp>

namespace pegium::workspace {

/// Default locator using the canonical AST path format used by the workspace.
class DefaultAstNodeLocator final : public AstNodeLocator {
public:
  [[nodiscard]] std::string getAstNodePath(const AstNode &node) const override;

  [[nodiscard]] const AstNode *getAstNode(const AstNode &root,
                                          std::string_view path) const override;
  [[nodiscard]] AstNode *getAstNode(AstNode &root,
                                    std::string_view path) const override;
};

} // namespace pegium::workspace
